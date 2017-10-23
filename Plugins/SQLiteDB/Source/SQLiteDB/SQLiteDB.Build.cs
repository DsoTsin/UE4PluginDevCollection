// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class SQLiteDB : ModuleRules
{
	public SQLiteDB(TargetInfo Target)
	{
		
		PublicIncludePaths.AddRange(
			new string[] {
				"SQLiteDB/Public"
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				"SQLiteDB/Private",
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);


        string PlatformName = "";
        string ConfigurationName = "";

        switch (Target.Platform)
        {
            case UnrealTargetPlatform.Win32:
                PlatformName = "Win32/";
                break;
            case UnrealTargetPlatform.Win64:
                PlatformName = "x64/";
                break;

            case UnrealTargetPlatform.IOS:
            case UnrealTargetPlatform.TVOS:
                PlatformName = "IOS/";
                break;
            case UnrealTargetPlatform.Mac:
                PlatformName = "Mac/";
                break;
            case UnrealTargetPlatform.Linux:
                PlatformName = "Linux/";
                break;
        }

        switch (Target.Configuration)
        {
            case UnrealTargetConfiguration.Debug:
                ConfigurationName = "Debug/";
                break;
            case UnrealTargetConfiguration.DebugGame:
                ConfigurationName = "Debug/";
                break;
            default:
                ConfigurationName = "Release/";
                break;
        }

        string LibraryPath = ThirdPartyPath + "/lib/" + PlatformName + ConfigurationName;
        string LibraryFilename = Path.Combine(LibraryPath, "sqlite" + UEBuildPlatform.GetBuildPlatform(Target.Platform).GetBinaryExtension(UEBuildBinaryType.StaticLibrary));

        if (!File.Exists(LibraryFilename))
        {
            //throw new BuildException("Please refer to the Engine/Source/ThirdParty/sqlite/README.txt file prior to enabling this module.");
        }

        PublicIncludePaths.Add(ThirdPartyPath + "/include/");

        PublicDependencyModuleNames.AddRange(
            new string[] {
                    "Core",
                    "DatabaseSupport",
            }
        );

        // Lib file
        PublicLibraryPaths.Add(LibraryPath);
        PublicAdditionalLibraries.Add(LibraryFilename);
    }


    private string ThirdPartyPath
    {
        get { return Path.GetFullPath(Path.Combine(ModuleDirectory, "../../ThirdParty/")); }
    }
}
