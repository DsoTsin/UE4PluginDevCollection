// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma optimize("", off)
#include "MeshSync.h"
#include "MeshSyncSettings.h"
#include "Async.h"

#include "HAL/ThreadSafeCounter.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/OutputDeviceRedirector.h"
#include "IPAddress.h"

#include "Sockets.h"
#include "SocketSubsystem.h"

#if WITH_EDITOR
	#include "ISettingsModule.h"
	#include "ISettingsSection.h"
	#include "UObject/Class.h"
	#include "UObject/WeakObjectPtr.h"
	#include "Editor.h"
#endif

#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"

#include "Factories/MaterialInstanceConstantFactoryNew.h"

#include "RawMesh.h"
#include "StaticMeshResources.h"
#include "PhysicsEngine/BodySetup.h"
#include "AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/Paths.h"
#include "Package.h"


#define MT_TERRAIN		TEXT("/MeshSync/Materials/MT_Terrain.MT_Terrain")
#define MT_DECOR			TEXT("/MeshSync/Materials/MT_Decor.MT_Decor")
#define MT_KNOBS			TEXT("/MeshSync/Materials/MT_Knobs.MT_Knobs")

enum class EMeshSyncCommand : uint32 {
	SendMesh,
	SendMaterial,
};

enum class MeshFlag : uint32 {
	NONE = 0,
	HAS_INDICES = 1,
	HAS_UV0 = 2,
	HAS_UV1 = 4,
	HAS_NORMAL = 8,
	HAS_COLOR_0 = 16,
	HAS_TEX_ID = 32,
	HAS_INSTANCE_POSITION = 64,
	HAS_INSTANCE_COLOR0 = 128,
	HAS_INDICES_UV01_COLOR0_TEXID = (HAS_INDICES | HAS_UV0 | HAS_UV1 | HAS_COLOR_0 | HAS_TEX_ID),
	HAS_INDICES_INSTANCE_POS_COLOR = (HAS_INDICES | HAS_INSTANCE_POSITION | HAS_INSTANCE_COLOR0)
};

enum EMaterialMS {
	MAT_MS_TERRAIN,
	MAT_MS_DECOR,
	MAT_MS_KNOBS,
	MAT_MS_WATER
};

const uint64_t MagicNumber = 0x64202114f;

struct MeshSyncPayload
{
	uint64_t Magic;
	uint32_t Length;
	EMeshSyncCommand Command;
};

static_assert(sizeof(FColor) == 4, "Size of FColor invalid");

class FMeshSyncServer;

class FSyncedMeshDesc
{
public:
	FString		Name;
	FRawMesh	RawMesh;
	TArray<FString>		MaterialSlots;
	// Tile ID
	uint32		TileX;
	uint32		TileY;
	uint32		TileZ;
};

class FMeshSyncConnectionThreaded : public FRunnable {
public:
	FMeshSyncConnectionThreaded(FMeshSyncServer* InServer, FSocket* InSocket, FString const& InPackage);
	~FMeshSyncConnectionThreaded()
	{
		WorkerThread->Kill(true);
		delete WorkerThread;
		WorkerThread = NULL;
	}

	virtual bool Init() override
	{
		DispProcs.Add(EMeshSyncCommand::SendMesh) = &FMeshSyncConnectionThreaded::ProcessingIncomingMesh;
		DispProcs.Add(EMeshSyncCommand::SendMaterial) = &FMeshSyncConnectionThreaded::ProcessingIncomingMaterial;
		return true;
	}

	bool ReadSizedBuffer(TArray<uint8>& Buffer) {
		int32 Recved = 0;
		auto Ret = Socket->Recv(Buffer.GetData(), Buffer.Num(), Recved);
		return Ret && Recved == Buffer.Num();
	}

	bool ReadString(FString& Str) {
		uint32 Length = 0;
		ReadPrim(Length);
		if (Length > 0) {
			char* Buffer = new char[Length + 1];
			int32 Recved = 0;
			Socket->Recv((uint8*)Buffer, Length, Recved);
			Buffer[Length] = 0;
			Str = ANSI_TO_TCHAR(Buffer);
			delete[] Buffer;
		}
		return true;
	}

	bool ReadStringList(TArray<FString>& StrList) {
		uint32 Count = 0;
		ReadPrim(Count);
		if (Count > 0) {
			for (uint32 i = 0; i < Count; i++) {
				FString ReadStr;
				ReadString(ReadStr);
				StrList.Add(ReadStr);
			}
		}
		return true;
	}

	template <typename T>
	bool ReadPrim(T& Prim) {
		int32 Recved = 0;
		auto Ret = Socket->Recv((uint8*)&Prim, sizeof(T), Recved);
		return Ret && Recved == sizeof(T);
	}

	template <typename T>
	bool ReadArray(TArray<T>& Array) {
		uint32 Length = 0;
		ReadPrim(Length);
		if (Length > 0) {
			Array.AddUninitialized(Length);
			int32 Recved = 0;
			Socket->Recv((uint8*)Array.GetData(), sizeof(T)*Array.Num(), Recved);
			if (Recved != sizeof(T)*Array.Num())
				return false;
		}
		return true;
	}

	bool IsAlive() const {
		if (!IsRunning())
			return false;
		return Socket && 
			Socket->GetConnectionState() == SCS_Connected;
	}

	bool ProcessingPayload(MeshSyncPayload& Payload);
	bool Dispatch(EMeshSyncCommand Command, uint32 Length);
	bool ProcessingIncomingMesh(uint32 Length);
	bool ProcessingIncomingMaterial(uint32 Length);

	virtual uint32 Run() override
	{
		while (!StopRequested.GetValue())
		{
			MeshSyncPayload Payload = {};
			if (!ProcessingPayload(Payload)) {
				break;
			}
			if (!Dispatch(Payload.Command, Payload.Length)) {
				break;
			}
		}
		return true;
	}

	virtual void Stop() override
	{
		StopRequested.Set(true);
	}

	virtual void Exit() override
	{
		Socket->Close();
		ISocketSubsystem::Get()->DestroySocket(Socket);
		Socket = NULL;
		Running.Set(false);
	}

	bool IsRunning() const
	{
		return (Running.GetValue() != 0);
	}

	void GetAddress(FInternetAddr& Addr)
	{
		Socket->GetAddress(Addr);
	}

	void GetPeerAddress(FInternetAddr& Addr)
	{
		Socket->GetPeerAddress(Addr);
	}

private:
	FSocket* Socket;
	FThreadSafeCounter StopRequested;
	FThreadSafeCounter Running;
	FRunnableThread* WorkerThread;
	FMeshSyncServer* Server;
	typedef bool(FMeshSyncConnectionThreaded::*FnProcessing)(uint32 Length);
	TMap<EMeshSyncCommand, FnProcessing> DispProcs;
	FString PathPackage;
};

class FMeshSyncServer : public FRunnable
{
public:
	FMeshSyncServer() {}
	~FMeshSyncServer();

	void Create(int InPort);

	virtual bool Init() override
	{
		return true;
	}

	void Stop() {
		StopRequested.Set(1);
	}

	//void AddMesh(FSyncedMeshDesc const& Desc) {
	//	FScopeLock ScopeLock(&MeshMutex);
	//	bool bIsAlreayExists = false;
	//	Meshes.Add(Desc, &bIsAlreayExists);
	//	if (bIsAlreayExists) {
	//		UE_LOG(LogMeshSync, Display, TEXT("%s is already added !"), *Desc.Name);
	//	}
	//}

	virtual uint32 Run() override;

	const FString& MainPackage() const { return PathPackage; }
	const FString& MaterialsPackage() const { return PathPackageMaterials; }

	UMaterialInterface* FindMaterial(FString const& Name);
	void AddMaterial(FString const& Name, UMaterialInterface* Material);

private:
	void InitMaterials();

	// Holds the server (listening) socket.
	FSocket*	Socket;
	FString		PathPackage;
	FString		PathPackageMaterials;
	// Holds the server thread object.
	FRunnableThread* Thread;
	// Holds the address that the server is bound to.
	TSharedPtr<FInternetAddr> ListenAddr;
	// Holds a flag indicating whether the thread should stop executing
	FThreadSafeCounter StopRequested;
	// Is the Listner thread up and running. 
	FThreadSafeCounter Running;
	TArray<FMeshSyncConnectionThreaded*> Connections;
	TMap<FString, UMaterialInterface*> Materials;
/*
	FCriticalSection			MeshMutex;
	TSet<FSyncedMeshDesc> Meshes;
*/

	TMap<FString, UMaterialInstanceConstant*> MaterialInstances;
};

#define LOCTEXT_NAMESPACE "FMeshSyncModule"

class FInternetAddr;
class FSocket;

void FMeshSyncModule::StartupModule()
{
#if WITH_EDITOR
	// register settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "MeshSync",
			LOCTEXT("MeshSyncSettingsName", "Mesh Sync"),
			LOCTEXT("MeshSyncSettingsDescription", "Configure the MeshSync plug-in."),
			GetMutableDefault<UMeshSyncSettings>()
		);
	}
#endif //WITH_EDITOR

	Server = new FMeshSyncServer();
	Server->Create(20196);
}

void FMeshSyncModule::ShutdownModule()
{
	Server->Stop();
	delete Server;
#if WITH_EDITOR
	// unregister settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "MeshSync");
	}
#endif
}

void FMeshSyncModule::StopMeshSyncServer()
{
	Server->Stop();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMeshSyncModule, MeshSync)

DEFINE_LOG_CATEGORY(LogMeshSync);

FMeshSyncServer::~FMeshSyncServer()
{
	if (Thread != NULL)
	{
		Thread->Kill(true);
		delete Thread;
		Thread = NULL;
	}

	Socket->Close();
	ISocketSubsystem::Get()->DestroySocket(Socket);
	Socket = NULL;
}

void FMeshSyncServer::Create(int InPort)
{
	// Create Package
	PathPackage = FString("/Game/Lego/Scene/");
	PathPackageMaterials = "/Game/Lego/Scene/Materials/";
	FString absolutePathPackage = FPaths::ProjectContentDir() + "/Lego/Scene/";
	FString absolutePathPackageMaterials = FPaths::ProjectContentDir() + "/Lego/Scene/Materials/";
	FPackageName::RegisterMountPoint(*PathPackage, *absolutePathPackage);
	FPackageName::RegisterMountPoint(*PathPackageMaterials, *absolutePathPackageMaterials);
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	if (!SocketSubsystem)
	{
		UE_LOG(LogMeshSync, Error, TEXT("Could not get socket subsystem."));
	}
	else
	{
		Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("FMeshSyncServer tcp-listen"));
		if (!Socket)
		{
			UE_LOG(LogMeshSync, Error, TEXT("Could not create listen socket."));
		}
		else
		{
			ListenAddr = SocketSubsystem->GetLocalBindAddr(*GLog);
			ListenAddr->SetPort(InPort);
			Socket->SetReuseAddr();
			if (!Socket->Bind(*ListenAddr))
			{
				UE_LOG(LogMeshSync, Warning, TEXT("Failed to bind listen socket %s in FMeshSyncServer"), *ListenAddr->ToString(true));
			}
			else if (!Socket->Listen(16))
			{
				UE_LOG(LogMeshSync, Warning, TEXT("Failed to listen on socket %s in FMeshSyncServer"), *ListenAddr->ToString(true));
			}
			else
			{
				int32 port = Socket->GetPortNo();
				check((InPort == 0 && port != 0) || port == InPort);
				ListenAddr->SetPort(port);
				Thread = FRunnableThread::Create(this, TEXT("FMeshSyncServer"), 8 * 1024, TPri_AboveNormal);
				UE_LOG(LogMeshSync, Display, TEXT("Mesh Sync Server is ready for client connections on %s!"), *ListenAddr->ToString(true));
			}
		}
	}
	InitMaterials();
}

uint32 FMeshSyncServer::Run()
{
	Running.Set(true);
	while (!StopRequested.GetValue())
	{
		bool bReadReady = false;
		if (Socket->WaitForPendingConnection(bReadReady, FTimespan::FromSeconds(0.25f)))
		{
			for (int32 ConnectionIndex = 0; ConnectionIndex < Connections.Num(); ++ConnectionIndex)
			{
				FMeshSyncConnectionThreaded* Connection = Connections[ConnectionIndex];
				if (!Connection->IsAlive())
				{
					Connections.RemoveAtSwap(ConnectionIndex);
					delete Connection;
				}
			}

			if (bReadReady)
			{
				FSocket* ClientSocket = Socket->Accept(TEXT("Remote Connection"));
				if (ClientSocket != NULL)
				{
					FMeshSyncConnectionThreaded* Connection = new FMeshSyncConnectionThreaded(this, ClientSocket, PathPackage);
					Connections.Add(Connection);
				}
			}
		}
		else
		{
			FPlatformProcess::Sleep(0.25f);
		}
	}

	return 0;
}

UMaterialInterface* FMeshSyncServer::FindMaterial(FString const& Name)
{
	UMaterialInterface** Material = Materials.Find(Name);
	if(Material)
		return *Material;
	else // not found! search in package
	{
		FString SearchName = PathPackageMaterials + Name;
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *SearchName);
		if (Mat) {
			AddMaterial(Name, Mat);
		} else { // NULL
			UE_LOG(LogMeshSync, Warning, TEXT("Material %s not found!"), *Name);
		}
		return Mat;
	}
}

void FMeshSyncServer::AddMaterial(FString const& Name, UMaterialInterface* Material)
{
	Materials.Add(Name) = Material;
}

void FMeshSyncServer::InitMaterials()
{
	Materials.Add(TEXT("MT_Knobs")) =
		LoadObject<UMaterial>(nullptr, MT_KNOBS);
	Materials.Add(TEXT("MT_Terrain")) =
		LoadObject<UMaterial>(nullptr, MT_TERRAIN);
	Materials.Add(TEXT("MT_Decor")) =
		LoadObject<UMaterial>(nullptr, MT_DECOR);
}

FMeshSyncConnectionThreaded::FMeshSyncConnectionThreaded(FMeshSyncServer* InServer, FSocket* InSocket, FString const& InPackage)
	: Socket(InSocket)
	, Server(InServer)
	, PathPackage(InPackage)
{
	Running.Set(true);
	StopRequested.Reset();

#if UE_BUILD_DEBUG
	// this thread needs more space in debug builds as it tries to log messages and such
	const static uint32 MeshSyncServerThreadSize = 2 * 1024 * 1024;
#else
	const static uint32 MeshSyncServerThreadSize = 1 * 1024 * 1024;
#endif

	WorkerThread = FRunnableThread::Create(this, TEXT("FMeshSyncServerConnection"), MeshSyncServerThreadSize, TPri_AboveNormal);
}

bool FMeshSyncConnectionThreaded::ProcessingPayload(MeshSyncPayload & Payload)
{
	int32 Recved = 0;
	auto Ret = Socket->Recv((uint8*)&Payload, sizeof(Payload), Recved);
	if (!Ret || Recved != sizeof(Payload)) {
		UE_LOG(LogMeshSync, Warning, TEXT("Unable to receive payload, terminating connection"));
		return false;
	}
	if (Payload.Magic != MagicNumber) {
		UE_LOG(LogMeshSync, Warning, TEXT("Unable to process payload magic number, terminating connection"));
		return false;
	}
	return true;
}

bool FMeshSyncConnectionThreaded::Dispatch(EMeshSyncCommand Command, uint32 Length)
{
	if (DispProcs.Find(Command)) {
		return (this->*DispProcs[Command])(Length);
	}
	return false;
}

bool FMeshSyncConnectionThreaded::ProcessingIncomingMesh(uint32 Length)
{
	auto pDesc = new FSyncedMeshDesc;
	FSyncedMeshDesc& Desc = *pDesc;
	FRawMesh& Mesh = Desc.RawMesh;

	ReadString(Desc.Name);
	// Name format [Name]+X_Y_Z

	MeshFlag Flag;
	ReadPrim(Flag); // Read Mesh Flag

	ReadPrim(Desc.TileX);
	ReadPrim(Desc.TileY);
	ReadPrim(Desc.TileZ);

	ReadArray(Mesh.FaceMaterialIndices);
	ReadArray(Mesh.FaceSmoothingMasks);
	ReadArray(Mesh.WedgeIndices);

	ReadArray(Mesh.VertexPositions);
	TArray<FVector> Normal;
	ReadArray(Normal);

	ReadArray(Mesh.WedgeTexCoords[0]); // Main Texcoord
	ReadArray(Mesh.WedgeTexCoords[1]); // Normal Texcoord
	ReadArray(Mesh.WedgeTexCoords[2]); // Roughness Metallic

	ReadArray(Mesh.WedgeColors);

	uint32 MaterialId = 0;
	ReadPrim(MaterialId);

	ReadStringList(Desc.MaterialSlots);

	TArray<FVector> InstancePositions;
	ReadArray(InstancePositions);
	TArray<FColor> InstanceColors;
	ReadArray(InstanceColors);

	if (!Mesh.IsValidOrFixable()) {
		delete pDesc;
		return false;
	}

	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		FString MeshPackageName = PathPackage + pDesc->Name;
		UPackage* Package = FindPackage(nullptr, *MeshPackageName);
		if (!Package) {
			Package = CreatePackage(nullptr, *MeshPackageName);
		} else { // already exists
			UE_LOG(LogMeshSync, Display, TEXT("Mesh %s is already existed!"), *MeshPackageName);
			delete pDesc;
			return;
		}

		FName StaticMeshName = MakeUniqueObjectName(Package, UStaticMesh::StaticClass(), FName(*pDesc->Name));
		UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, StaticMeshName, RF_Public | RF_Standalone | RF_Transactional);
		checkSlow(StaticMesh);

		if (!StaticMesh) {
			delete pDesc;
			return;
		}
		StaticMesh->PreEditChange(nullptr);

		StaticMesh->SourceModels.Empty();

		// Saving mesh in the StaticMesh
		new(StaticMesh->SourceModels) FStaticMeshSourceModel();
		StaticMesh->SourceModels[0].RawMeshBulkData->SaveRawMesh(pDesc->RawMesh);

		FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[0];

		// Model Configuration
		StaticMesh->SourceModels[0].BuildSettings.bRecomputeNormals = true;
		StaticMesh->SourceModels[0].BuildSettings.bRecomputeTangents = true;
		StaticMesh->SourceModels[0].BuildSettings.bUseMikkTSpace = false;
		StaticMesh->SourceModels[0].BuildSettings.bGenerateLightmapUVs = true;
		StaticMesh->SourceModels[0].BuildSettings.bBuildAdjacencyBuffer = false;
		StaticMesh->SourceModels[0].BuildSettings.bBuildReversedIndexBuffer = false;
		StaticMesh->SourceModels[0].BuildSettings.bUseFullPrecisionUVs = false;
		StaticMesh->SourceModels[0].BuildSettings.bUseHighPrecisionTangentBasis = false;

		// Assign the Materials to the Slots (optional
		for (int32 i = 0; i < pDesc->MaterialSlots.Num(); i++) {
			FString MaterialName = pDesc->MaterialSlots[i]; // search imported MIC by name
			if (!MaterialName.StartsWith(TEXT("MT_"))) {
				MaterialName = FString(TEXT("MT_")) + MaterialName;
			}
			// with slots?
			FStaticMaterial Material;
			Material.MaterialInterface = Server->FindMaterial(MaterialName);
			StaticMesh->StaticMaterials.Add(Material);
			StaticMesh->SectionInfoMap.Set(0, i, FMeshSectionInfo(i));
		}

		FAssetRegistryModule::AssetCreated(StaticMesh);

		Package->MarkPackageDirty();
		//Package->FullyLoad();

		// Processing the StaticMesh and Marking it as not saved
		StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
		StaticMesh->CreateBodySetup();
		StaticMesh->BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
		StaticMesh->SetLightingGuid();
		StaticMesh->PostEditChange();

		TArray<UObject*>ObjectsToSync;
		ObjectsToSync.Add(StaticMesh);
		GEditor->SyncBrowserToObjects(ObjectsToSync);
		delete pDesc;
	});

	return true;
}

bool FMeshSyncConnectionThreaded::ProcessingIncomingMaterial(uint32 Length)
{
	FString MaterialName;
	ReadString(MaterialName);
	uint32 MaterialId;
	ReadPrim(MaterialId);
	FVector BaseColor;
	ReadPrim(BaseColor);
	float Roughness;
	ReadPrim(Roughness);
	float Metallic;
	ReadPrim(Metallic);
	FString BaseColorMap;
	ReadString(BaseColorMap);
	FString NormalMap;
	ReadString(NormalMap);

	uint32 MaterialCatagory = (MaterialId >> 16);
	uint32 MaterialImpId = (MaterialId & 0x0000ffff);
	
	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		if (MaterialName.StartsWith(TEXT("MT_"))) 
		{

		}
		else // need build mic and reduce materials
		{
			FString RealName = TEXT("MT_");
			RealName += MaterialName;
			FString MaterialPackageName = Server->MaterialsPackage() + RealName;
			UPackage* Package = FindPackage(nullptr, *MaterialPackageName);
			if (!Package) {
				Package = CreatePackage(nullptr, *MaterialPackageName);
			} else { // already has this material
				UE_LOG(LogMeshSync, Display, TEXT("Material %s is already existed!"), *MaterialPackageName);
				return;
			}
			FName MaterialInstanceName = MakeUniqueObjectName(Package, UStaticMesh::StaticClass(), FName(*RealName));
			UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
			switch (MaterialCatagory) {
			case MAT_MS_TERRAIN:
				Factory->InitialParent = Server->FindMaterial(TEXT("MT_Terrain"));
				break;
			case MAT_MS_DECOR:
				Factory->InitialParent = Server->FindMaterial(TEXT("MT_Decor"));
				break;
			case MAT_MS_KNOBS:
				Factory->InitialParent = Server->FindMaterial(TEXT("MT_Knobs"));
				break;
			case MAT_MS_WATER:
				//Factory->InitialParent = Material;
				break;
			}
			UMaterialInstanceConstant* MIC = (UMaterialInstanceConstant*)Factory->FactoryCreateNew(
				UMaterialInstanceConstant::StaticClass(), 
				Package, MaterialInstanceName, RF_Standalone | RF_Public | RF_Transactional, NULL, GWarn);
			checkSlow(MIC);
			FAssetRegistryModule::AssetCreated(MIC);
			//Package->FullyLoad();
			Package->SetDirtyFlag(true);
			//MaterialImpId;
			//UObject* NewAsset = AssetTools.CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UMaterialInstanceConstant::StaticClass(), Factory);
			TArray<UObject*>ObjectsToSync;
			ObjectsToSync.Add(MIC);
			GEditor->SyncBrowserToObjects(ObjectsToSync);
			Server->AddMaterial(RealName, MIC);
			MIC->PreEditChange(NULL);
			MIC->PostEditChange();
		}

	});
	
	return false;
}
#pragma optimize("", on)