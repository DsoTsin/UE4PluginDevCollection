// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Procedural : ModuleRules
{
	public Procedural(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				"EjPx/Public"
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				"EjPx/Private",
				// ... add other private include paths required here ...
			}
			);
			
		if(Target.Type == TargetType.Editor)
        {
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                "Core",
                "UnrealEd",
                "ComponentVisualizers"
                    // ... add other public dependencies that you statically link with here ...
                }
                );
        }
        else
        {
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                "Core",
                    // ... add other public dependencies that you statically link with here ...
                }
                );
        }
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "RenderCore",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);


        SetupModulePhysXAPEXSupport(Target);
        if (UEBuildConfiguration.bCompilePhysX && (UEBuildConfiguration.bBuildEditor || UEBuildConfiguration.bCompileAPEX))
        {
            DynamicallyLoadedModuleNames.Add("PhysXCooking");
        }

        if (UEBuildConfiguration.bCompilePhysX)
        {
            // Engine public headers need to know about some types (enums etc.)
            PublicIncludePathModuleNames.Add("ClothingSystemRuntimeInterface");
            PublicDependencyModuleNames.Add("ClothingSystemRuntimeInterface");

            if (UEBuildConfiguration.bBuildEditor)
            {
                PrivateDependencyModuleNames.Add("ClothingSystemEditorInterface");
                PrivateIncludePathModuleNames.Add("ClothingSystemEditorInterface");
            }
        }
    }
}
