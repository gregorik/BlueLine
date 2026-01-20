// Copyright YourTeamName. All Rights Reserved.

using UnrealBuildTool;

public class BlueLineSmartTags : ModuleRules
{
    public BlueLineSmartTags(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "GameplayTags",
                "BlueLineCore" // Link to our Runtime Core to read the Theme Data Asset
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "InputCore",
                "EditorFramework", // Required for modern UE5 editor styling
                "UnrealEd",        // Essential for Editor-only logic
                "PropertyEditor",  // Required to register IPropertyTypeCustomization
                "EditorStyle",     // For accessing UE5 fonts and brushes
                "GameplayTagsEditor" // To interact with existing Tag UI logic
            }
        );
    }
}