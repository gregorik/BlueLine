// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UBlueLineThemeData.generated.h"

/**
 * Defines the visual style for a specific Gameplay Tag.
 */
USTRUCT(BlueprintType)
struct FBlueLineTagStyle
{
	GENERATED_BODY()

	/** The Gameplay Tag (or parent tag) to style. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlueLine|Tags")
	FGameplayTag Tag;

	/** The color to display in the Editor and Debug HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlueLine|Tags")
	FLinearColor Color = FLinearColor::White;

	/** 
	 * If true, this color applies to all child tags unless a child has its own override.
	 * Example: If 'Status.Damage' is Red, 'Status.Damage.Fire' will be Red.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlueLine|Tags")
	bool bApplyToChildren = true;
};

/**
 * Settings for the Manhattan-style wire rendering.
 */
USTRUCT(BlueprintType)
struct FBlueLineWireSettings
{
	GENERATED_BODY()

	/** Thickness of the wire lines. Default Unreal is ~1.5. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlueLine|Wires", meta = (ClampMin = "1.0", ClampMax = "10.0"))
	float WireThickness = 2.0f;

	/** Size of the bubble loop when a wire connects to a pin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlueLine|Wires")
	FVector2D BubbleSize = FVector2D(12.0f, 12.0f);

	/** 
	 * Opacity of wires when they pass BEHIND a node (The "Ghost Wire" effect).
	 * Lower values make overlapping wires less distracting. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlueLine|Wires", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float GhostWireOpacity = 0.35f;
};

/**
 * UBlueLineThemeData
 * 
 * The central configuration asset for the BlueLine plugin.
 * By checking this DataAsset into source control, the whole team shares
 * the same tag colors and graph formatting rules, eliminating "Local Config" friction.
 */
UCLASS(BlueprintType, Const)
class BLUELINECORE_API UBlueLineThemeData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** List of specific tag styling rules. Processed top-to-bottom. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	TArray<FBlueLineTagStyle> TagStyles;

	/** Fallback color if no matching tag style is found. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FLinearColor DefaultTagColor = FLinearColor(0.7f, 0.7f, 0.7f, 1.0f);

	/** Global settings for graph wire visualization. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wires")
	FBlueLineWireSettings WireSettings;

public:
	/**
	 * Helper to find the correct color for a given tag based on the rules.
	 * Handles the 'bApplyToChildren' logic.
	 */
	UFUNCTION(BlueprintCallable, Category = "BlueLine")
	FLinearColor GetColorForTag(const FGameplayTag& Tag) const;
};