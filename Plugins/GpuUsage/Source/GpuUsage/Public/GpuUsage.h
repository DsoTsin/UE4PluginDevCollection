// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleManager.h"


/**
* The public interface to this module
*/
class IGpuUsageModule : public IModuleInterface
{
public:

    virtual int QueryCurrentLoad() = 0; /*{ return 0; }*/
};
