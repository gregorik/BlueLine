// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UBlueLineEditorSettings.generated.h"

/**
 * UBlueLineEditorSettings
 * 
 * Local Editor Preferences for the BlueLine plugin.
 * 
 * KEY DISTINCTION:
 * - Shared visual rules (Colors, Wire Thickness) go in UBlueLineThemeData (Source Controlled).
 * - Personal workflow preferences (Enable toggle, Sensitivity) go here (Local User Config).
 */
UCLASS(config = EditorPerProjectUserSettings, defaultconfig, meta = (DisplayName = "BlueLine Graph"))
class BLUELINEGRAPH_API UBlueLineEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBlueLineEditorSettings();

	/** 
	 * Master toggle for the Manhattan (Circuit Board) wire style.
	 * If disabled, wires revert to standard Unreal splines (Bezier curves).
	 */
	UPROPERTY(EditAnywhere, config, Category = "Visuals")
	bool bEnableManhattanRouting;

	/**
	 * If true, wires that pass BEHIND a node are rendered with reduced opacity.
	 * This helps solve the "Visual Spaghetti" problem without expensive pathfinding.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Visuals")
	bool bDimWiresBehindNodes;

	/**
	 * When using the "Soft Format" (Magnet) command, how much padding 
	 * should be left between the nodes?
	 */
	UPROPERTY(EditAnywhere, config, Category = "Formatting", meta = (ClampMin = "10.0", ClampMax = "200.0"))
	float FormatterPadding;

	/**
	 * Sensitivity of the grid snapping logic.
	 * Higher values mean nodes snap to alignment from further away.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Formatting", meta = (ClampMin = "50.0", ClampMax = "500.0"))
	float MagnetEvaluationDistance;

public:
	/** UDeveloperSettings Interface */
	virtual FName GetContainerName() const override { return FName("Editor"); }
	virtual FName GetCategoryName() const override { return FName("Plugins"); }
	virtual FName GetSectionName() const override { return FName("BlueLine Graph"); }

#if WITH_EDITOR
	/** Used to trigger a graph repaint when settings change */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Delegate to broadcast when settings change so the Graph Factory can refresh */
	DECLARE_MULTICAST_DELEGATE(FOnBlueLineSettingsChanged);
	static FOnBlueLineSettingsChanged OnSettingsChanged;
};