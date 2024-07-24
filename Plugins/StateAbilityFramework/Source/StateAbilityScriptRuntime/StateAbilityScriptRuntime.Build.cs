// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StateAbilityScriptRuntime : ModuleRules
{
	public StateAbilityScriptRuntime(ReadOnlyTargetRules Target) : base(Target)
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
				"EnhancedInput",
                "DeveloperSettings",
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
				"RenderCore",
                "CoreOnline",
                "StructUtils",
                "NetCore",

				// ECS
				"MassEntity",

				// 临时借用一下Mover内部的类
                "Mover",
				// 临时借用一下Debug
				"Chaos",
            }
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
