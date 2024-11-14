using UnrealBuildTool;
using System.IO;
public class AutoP4 : ModuleRules
{
	public AutoP4(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
			new string [] {
				"Core",
				"Projects"
            }
		);
        
		AddEngineThirdPartyPrivateStaticDependencies(Target, "Perforce");
		
		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Mac)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");
		}
	}
}
