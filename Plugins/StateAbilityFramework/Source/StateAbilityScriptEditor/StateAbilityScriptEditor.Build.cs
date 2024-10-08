// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StateAbilityScriptEditor : ModuleRules
{
	public StateAbilityScriptEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
        PublicIncludePaths.AddRange(
            new string[] {
                System.IO.Path.Combine(GetModuleDirectory("CoreUObject"), "Private"),
                System.IO.Path.Combine(GetModuleDirectory("PropertyEditor"), "Private")
           }
        );

        PrivateIncludePaths.AddRange(
            new string[] {
                System.IO.Path.Combine(GetModuleDirectory("CoreUObject"), "Private"),
                System.IO.Path.Combine(GetModuleDirectory("PropertyEditor"), "Private")
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
				"AssetTools",
                "UnrealEd",
                "ToolMenus",
                "GraphEditor",
                "ApplicationCore",
                "GameplayTags",
                "ContentBrowser",
                "ClassViewer",
                "StructViewer",
                "AssetRegistry",
                "KismetWidgets",
                "InputCore",

                "AIGraph",        //@Todo: 暂时借用一下它的Class搜集器
                "Documentation",
                "StructUtils",
                "ToolWidgets",
                "Projects",
                "BlueprintGraph",
                "PropertyEditor",// @TODO：为了使用SPropertyValueWidget的暂时处理方式
                "SceneOutliner",

                "StateAbilityScriptRuntime",
				"ConfigVars",
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
