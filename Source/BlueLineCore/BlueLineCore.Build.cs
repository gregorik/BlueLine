// Copyright YourTeamName. All Rights Reserved.

using UnrealBuildTool;

public class BlueLineCore : ModuleRules
{
    public BlueLineCore(ReadOnlyTargetRules Target) : base(Target)
    {
        // Use Shared PCHs for faster compilation
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

	//	bUseUnity = false;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",          // Needed for UDataAsset and Runtime Debug Drawing (HUD)
                "GameplayTags",
             //   "GameplayTagsEditor", // Critical: The Theme Data asset relies on FGameplayTag structures,
                "AssetRegistry"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore"
                
            }
        );

        // DynamicallyLoadedModuleNames are rarely needed in modern UE5 setups
        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
            }
        );
    }
}