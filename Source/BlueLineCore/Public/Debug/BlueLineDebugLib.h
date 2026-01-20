// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "BlueLineDebugLib.generated.h"

class UBlueLineThemeData;

/**
 * UBlueLineDebugLib
 * 
 * Provides static utility functions to visualize BlueLine tags at runtime.
 * This bridges the gap between the Editor UI colors and the Runtime HUD.
 */
UCLASS()
class BLUELINECORE_API UBlueLineDebugLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Draws a debug string at the specified location, colored according to the BlueLine theme.
	 * 
	 * @param Tag			The GameplayTag to display.
	 * @param Location		World location to draw the text.
	 * @param TextScale		Scale of the text.
	 * @param Duration		How long the debug text should persist (0 = one frame).
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", DevelopmentOnly), Category = "BlueLine|Debug")
	static void DrawBlueLineDebugTag(const UObject* WorldContextObject, FGameplayTag Tag, FVector Location, float TextScale = 1.0f, float Duration = 0.0f);

	/**
	 * Returns the color defined for this Tag in the BlueLine theme.
	 * Useful if you want to apply the standard tag color to your own UMG widgets.
	 * 
	 * @param Tag			The tag to resolve.
	 * @Return				The FLinearColor defined in the active theme (or default grey).
	 */
	UFUNCTION(BlueprintPure, Category = "BlueLine|Theme")
	static FLinearColor GetColorForTag(FGameplayTag Tag);

	/**
	 * Tries to locate the BlueLine Theme DataAssets.
	 * 
	 * Note: In a production plugin, this would likely look at GetMutableDefault<UBlueLineProjectSettings>()
	 * to find the asset assigned in Project Settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "BlueLine|System")
	static UBlueLineThemeData* GetActiveThemeData();
};