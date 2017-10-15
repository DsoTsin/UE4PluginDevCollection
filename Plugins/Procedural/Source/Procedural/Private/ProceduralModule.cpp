// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Procedural.h"
#if WITH_EDITOR
#include "AssetRegistryModule.h"
#include "UnrealEd.h"
#endif
#include "Classes/ArcComponent.h"

#define LOCTEXT_NAMESPACE "FProceduralModule"

void FProceduralModule::StartupModule()
{
#if WITH_EDITOR
    if (GUnrealEd)
    {
        GUnrealEd->RegisterComponentVisualizer(UArcComponent::StaticClass()->GetFName(), MakeShareable(new FArcCompVisualizer));
    }
#endif
}

void FProceduralModule::ShutdownModule()
{
#if WITH_EDITOR
    if (GUnrealEd)
    {
        GUnrealEd->UnregisterComponentVisualizer(UArcComponent::StaticClass()->GetFName());
    }
#endif
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FProceduralModule, Procedural)