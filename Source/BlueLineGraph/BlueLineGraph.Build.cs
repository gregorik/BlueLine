// Copyright YourTeamName. All Rights Reserved.

using UnrealBuildTool;

public class BlueLineGraph : ModuleRules
{
    public BlueLineGraph(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
   //     bUseUnity = false;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "BlueLineCore", // Runtime Logic
                "GameplayTags"  // <--- CRITICAL FIX: Required for FGameplayTag::StaticStruct
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "InputCore",
                "EditorFramework",
                "UnrealEd",
                "GraphEditor",
                "BlueprintGraph",
                "EditorStyle",
                "KismetWidgets",
                "DeveloperSettings",
                "Projects",
                "Kismet",
                "ToolMenus",
                "GameplayTagsEditor" // <--- CRITICAL FIX: Required for Pin Picker Widget
            }
        );
    }
}