#include "AutoP4.h"
#include "XmlParser.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4458)
#endif
#include <p4/clientapi.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogAutoP4, Log, All);
DEFINE_LOG_CATEGORY(LogAutoP4);

#define FROM_TCHAR(InText, bIsUnicodeServer) (bIsUnicodeServer ? TCHAR_TO_UTF8(InText) : TCHAR_TO_ANSI(InText))
#define TO_TCHAR(InText, bIsUnicodeServer) (bIsUnicodeServer ? UTF8_TO_TCHAR(InText) : ANSI_TO_TCHAR(InText))

typedef TMap<FString,FString>	FRecord;
typedef TArray<FRecord>			FRecords;

class FP4User : public ClientUser
{
public:
	FP4User(TArray<FText>& ErrorMessages, FRecords& InRecords, bool InIsUnicodeServer = false)
		: OutErrorMessages(ErrorMessages)
		, Records(InRecords)
		, bIsUnicodeServer(InIsUnicodeServer)
	{}

	virtual void HandleError(Error *InError) override
	{
		StrBuf ErrorMessage;
		InError->Fmt(&ErrorMessage);
		OutErrorMessages.Add(FText::FromString(FString(TO_TCHAR(ErrorMessage.Text(), bIsUnicodeServer))));
	}

	void OutputStat(StrDict* VarList) override
	{
		TMap<FString,FString> Record;
		StrRef Var, Value;

		// Iterate over each variable and add to records
		for (int32 Index = 0; VarList->GetVar(Index, Var, Value); Index++)
		{
			Record.Add(TO_TCHAR(Var.Text(), bIsUnicodeServer), TO_TCHAR(Value.Text(), bIsUnicodeServer));
		}

		Records.Add(Record);
	}

	virtual void OutputInfo(ANSICHAR Level, const ANSICHAR *Data) override
	{
		const int32 ChangeTextLen = FCString::Strlen(TEXT("Change "));
		if (FString(TO_TCHAR(Data, bIsUnicodeServer)).StartsWith(TEXT("Change ")))
		{
		}
	}

	TArray<FText>& OutErrorMessages;
	FRecords& Records;
	bool bIsUnicodeServer;
};

class FP4LoginUser : public FP4User
{
public:
	FP4LoginUser(TArray<FText>& OutErrorMessages, FRecords& InRecords, const FString& InPassword)
		: FP4User(OutErrorMessages, InRecords),
		Password(InPassword)
	{}

	void Prompt( const StrPtr& InMessage, StrBuf& OutPrompt, int NoEcho, Error* InError ) override
	{
		OutPrompt.Set(FROM_TCHAR(*Password, bIsUnicodeServer));
	}

	FString Password;
};

class FP4GetClientsUser : public FP4User
{
public:
	FP4GetClientsUser(TArray<FText>& OutErrorMessages, FRecords& InRecords)
		: FP4User(OutErrorMessages, InRecords)
	{}

	void OutputInfo(ANSICHAR Level, const ANSICHAR *Data) override 
	{
	}
};

struct FWorkspace
{
	FString Name;
	FString Host;
	FString Directory;
	FString Stream;
};

struct FConnectionMap 
{
  FString Name;
  FString User;
  FString P4Port;
  FString ServerName;
};

class FP4GetWorkspaceSpecUser : public FP4User
{
public:
	FP4GetWorkspaceSpecUser(TArray<FText>& OutErrorMessages,FRecords& InRecords, FWorkspace& InWorkspace)
		: FP4User(OutErrorMessages, InRecords)
		, Workspace(InWorkspace)
	{}

	void OutputStat(StrDict* VarList) override
	{
		StrRef Var, Value;
		// Iterate over each variable and add to records
		for (int32 Index = 0; VarList->GetVar(Index, Var, Value); Index++)
		{
			FString Key(TO_TCHAR(Var.Text(), bIsUnicodeServer));
			FString RecordValue(TO_TCHAR(Value.Text(), bIsUnicodeServer));
		}
	}

	void OutputInfo(ANSICHAR Level, const ANSICHAR *Data) override 
	{
	}

	FWorkspace& Workspace;
};

class FAutoP4 : public IP4, public IModuleInterface
{
public:
	FAutoP4(): ChangeListNumber(0) {}

	~FAutoP4() override
	{}
	
	virtual void StartupModule( ) override
	{
		FString UserDir = FPlatformProcess::UserDir();
		FString P4QtDir = UserDir / TEXT("../.p4qt");
		FString ConnectionMapFile = P4QtDir / TEXT("connectionmap.xml");
        TArray<FConnectionMap> ConnectionMaps;
		if (FPaths::FileExists(ConnectionMapFile))
		{
			FXmlFile XmlFile;
			auto bResult = XmlFile.LoadFile(ConnectionMapFile, EConstructMethod::ConstructFromFile);
			if (bResult)
			{
				static const FString ConnectionMapList(TEXT("ConnectionMapList"));
				static const FString ConnectionMap(TEXT("ConnectionMap"));
				static const FString Name(TEXT("Name"));
				static const FString User(TEXT("User"));
				static const FString ServerName(TEXT("ServerName"));
				static const FString P4Port(TEXT("P4Port"));

				auto GetText = [](const FXmlNode* Node, const FString& Name) -> FString 
				{
					const FXmlNode* NameNode = Node->FindChildNode(Name);
					return NameNode ? NameNode->GetContent(): FString();
				};

                auto Root = XmlFile.GetRootNode();
                if (Root == nullptr || Root->GetTag() != ConnectionMapList) {
					return;
				}
				const FXmlNode* ConnectionMapNode = Root->FindChildNode(ConnectionMap);
				if (ConnectionMapNode == nullptr)
				{
					return;
				}
                while (ConnectionMapNode) {
					FString ConnName = GetText(ConnectionMapNode, Name);
					FString ConnUser = GetText(ConnectionMapNode, User);
					FString ConnP4Port = GetText(ConnectionMapNode, P4Port);
					FString ConnServerName = GetText(ConnectionMapNode, ServerName);
					ConnectionMaps.Add(FConnectionMap{ MoveTemp(ConnName), MoveTemp(ConnUser), MoveTemp(ConnP4Port), MoveTemp(ConnServerName) });
                    ConnectionMapNode = ConnectionMapNode->GetNextNode();
				}
			}
		}
		
		FString ProjectName = FApp::GetProjectName();
		FString ProbablyP4Port;
		FString ProbablyP4User;
		for (auto& Conn: ConnectionMaps)
		{
			if (Conn.P4Port.Contains(*ProjectName))
			{
				ProbablyP4Port = Conn.P4Port;
				ProbablyP4User = Conn.User;
				break;
			}
		}
		FString P4User;
		FString P4Passwd;
		FString P4Port;
		FParse::Value(FCommandLine::Get(), TEXT("p4user"), P4User);
		FParse::Value(FCommandLine::Get(), TEXT("p4passwd"), P4Passwd);
		FParse::Value(FCommandLine::Get(), TEXT("p4port"), P4Port);

		if (P4User.IsEmpty())
		{
			P4User = ProbablyP4User;
		}
		if (P4Port.IsEmpty())
		{
			P4Port = ProbablyP4Port;
		}

		Api.SetPort(TCHAR_TO_ANSI(*P4Port));
		Api.SetUser(TCHAR_TO_ANSI(*P4User));
		
		Api.SetProg("UE4");
		Api.SetProtocol("tag", "");
		Api.SetProtocol("enableStreams", "");

		Error P4Error;
		Api.Init(&P4Error);
		if (P4Error.Test())
		{
			UE_LOG(LogAutoP4, Error, TEXT("P4ERROR: Invalid connection to server."));
		}
		FString Ticket;
		TArray<FText> ErrorMessages;
		// get info
		{
			FRecords Records;
			FP4LoginUser User(ErrorMessages, Records, P4Passwd);
			Api.Run("info", &User);
			if (ErrorMessages.Num() > 0)
			{
				for (auto& ErrorMessage: ErrorMessages)
				{
					UE_LOG(LogAutoP4, Error, TEXT("P4ERROR: %s"), *ErrorMessage.ToString());
				}
				ErrorMessages.Empty();
			}
		}

		{
			FRecords Records;
			FP4LoginUser User(ErrorMessages, Records, P4Passwd);
			const char *ArgV[] = { "-a" };
			Api.SetArgv(1, const_cast<char*const*>(ArgV));
			Api.Run("login", &User);
			if (ErrorMessages.Num() > 0)
			{
				for (auto& ErrorMessage: ErrorMessages)
				{
					UE_LOG(LogAutoP4, Error, TEXT("P4ERROR: %s"), *ErrorMessage.ToString());
				}
				ErrorMessages.Empty();
			}
		}
		
		TArray<FWorkspace> WorkspaceList;

		FString ProjectFilePath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
		FString LocalHostName;
		if(LocalHostName.Len() == 0)
		{
			// No host override, check environment variable
			LocalHostName = FPlatformMisc::GetEnvironmentVariable(TEXT("P4HOST"));
		}

		if (LocalHostName.Len() == 0)
		{
			// no host name, use local machine name
			LocalHostName = FString(FPlatformProcess::ComputerName()).ToLower();
		}
		else
		{
			LocalHostName = LocalHostName.ToLower();
		}

		{
			FRecords Records;
			FP4GetClientsUser User(ErrorMessages, Records);
			const char *ArgV[] = { "--me" };
			Api.SetArgv(1, const_cast<char*const*>(ArgV));
			Api.Run("clients", &User);
			for (auto& Record: Records)
			{
				FString Host = Record["Host"];
				if (Host.ToLower() == LocalHostName)
				{
					FWorkspace Workspace 
					{
						Record["client"],
						Host,
						Record["Root"],
						Record["Stream"]
					};
					FPaths::NormalizeDirectoryName(Workspace.Directory);
					if (ProjectFilePath.StartsWith(Workspace.Directory))
					{
						WorkspaceList.Add(Workspace);
					}
				}				
			}
		}
		Algo::SortBy(WorkspaceList, [](const FWorkspace& Ws){ return Ws.Directory.Len(); }, [](int32 A, int32 B) -> bool { return A > B; });
		
		if (WorkspaceList.Num() > 0)
		{
			CurrentWorkspace = WorkspaceList[0].Name;
			CurrentStream = WorkspaceList[0].Stream;
			Api.SetClient(TCHAR_TO_ANSI(*CurrentWorkspace));
            Api.SetCwd(TCHAR_TO_ANSI(*WorkspaceList[0].Directory));
			FRecords Records;
			FP4GetClientsUser User(ErrorMessages, Records);
			const char *ArgV[] = { "-m1", "...#have" };
			Api.SetArgv(2, const_cast<char*const*>(ArgV));
			Api.Run("changes", &User);
			if (Records.Num() > 0)
			{
				auto Change = Records[0].Find("change");
				if (Change)
				{
					ChangeListNumber = FCString::Atoi64(**Change);
				}
			}
		}
		UE_LOG(LogAutoP4, Display, TEXT("Current workspace @%llu"), ChangeListNumber);
	}

	virtual void ShutdownModule( ) override
	{
	}

	virtual uint64 GetLocalCurrentChangelist() const override 
	{
		return ChangeListNumber;
	}
	
	virtual FString GetCurrentWorkspace() const override
	{
		return CurrentWorkspace;
	}

	virtual FString GetCurrentStream() const override
	{
		return CurrentStream;
	}
private:
	uint64 ChangeListNumber;
	FString CurrentWorkspace;
	FString CurrentStream;
	ClientApi Api;
};

IMPLEMENT_MODULE(FAutoP4, AutoP4);