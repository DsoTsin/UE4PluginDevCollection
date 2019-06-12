// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMeshSyncServer;

class FMeshSyncModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual void StopMeshSyncServer();

private:
	FMeshSyncServer* Server;
};

DECLARE_LOG_CATEGORY_EXTERN(LogMeshSync, Log, All);

enum class EMeshSyncServiceError
{
	Succeeded,
};

class FMeshSyncService
{
public:
	virtual EMeshSyncServiceError OnReceiveMesh();
};