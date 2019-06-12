#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "MeshSyncSettings.generated.h"

UCLASS(config = Game)
class MESHSYNC_API UMeshSyncSettings
	: public UObject
{
	GENERATED_BODY()

public:

	/** Default constructor. */
	UMeshSyncSettings();

public:

	/** The listening port for MeshSyncServer. */
	UPROPERTY(config, EditAnywhere, Category = Server)
	int32 Port;

	/** The package path specified for MeshSync assets */
	UPROPERTY(config, EditAnywhere, Category = Server)
	FString SyncPackageLocation;
};