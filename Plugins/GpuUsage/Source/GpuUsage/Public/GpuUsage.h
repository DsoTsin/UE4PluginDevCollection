// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleManager.h"

struct FGpuMemoryInfo
{
    unsigned int DedicatedVideoMemoryInMB;
    unsigned int AvailableDedicatedVideoMemoryInMB;
    unsigned int CurrentAvailableDedicatedVideoMemoryInMB;
};

/**
* The public interface to this module
*/
class IGpuUsageModule : public IModuleInterface
{
public:

    virtual int QueryCurrentLoad() = 0; /*{ return 0; }*/
    virtual FGpuMemoryInfo QueryVideoMemory(int index) = 0;

};
