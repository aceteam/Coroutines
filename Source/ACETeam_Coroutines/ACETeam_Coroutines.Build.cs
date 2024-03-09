// Copyright ACE Team Software S.A. All Rights Reserved.

using UnrealBuildTool;

public class ACETeam_Coroutines : ModuleRules
{
	public ACETeam_Coroutines(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
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
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		bUseUnity = false;

#if UE_5_2_OR_LATER
		SetupGameplayDebuggerSupport(Target);
		if (Target.bUseGameplayDebugger)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Slate",
					"SlateCore",
					"InputCore",
				}
			);
		}
#else
		if (Target.bBuildDeveloperTools || (Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Configuration != UnrealTargetConfiguration.Test))
		{
			PrivateDependencyModuleNames.Add("GameplayDebugger");
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"GameplayDebugger",
					"Slate",
					"SlateCore",
					"InputCore",
				}
			);
			PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER=0");
		}
#endif
		PublicDefinitions.Add("WITH_ACETEAM_COROUTINE_DEBUGGER=WITH_GAMEPLAY_DEBUGGER");
	}
}
