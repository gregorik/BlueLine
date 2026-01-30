// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

/**
 * FBlueLineCommands
 * 
 * Defines the keyboard shortcuts and menu actions for the BlueLine Graph module.
 * 
 * To remap these in the Editor:
 * Go to Editor Preferences -> Keyboard Shortcuts -> BlueLine Graph
 */
class FBlueLineCommands : public TCommands<FBlueLineCommands>
{
public:
	/** Constructor: Sets up the context name and parent style */
	FBlueLineCommands()
		: TCommands<FBlueLineCommands>(
			TEXT("BlueLineGraph"), // Context name for keybindings
			NSLOCTEXT("Contexts", "BlueLineGraph", "BlueLine Graph"), // User facing name
			NAME_None, // Parent context
			FAppStyle::GetAppStyleSetName() // Icon style set
		)
	{}

	/** Register commands with the system */
	virtual void RegisterCommands() override;

	/**
	 * Command: Auto-Format Selected
	 * Feature: The "Soft" formatter.
	 * Action: Aligns selected nodes to the grid relative to their inputs/outputs.
	 * Solves: The "Diff War" weakness (only moves what you touch).
	 */
	TSharedPtr<FUICommandInfo> AutoFormatSelected; // Shift + Q
	TSharedPtr<FUICommandInfo> RigidifyConnections; // Shift + R

	/** 
	 * Command: Toggle Wire Style
	 * Feature: Instantly switch between Manhattan (BlueLine) and Spline (Vanilla) wires.
	 * Useful for debugging or if the "Circuit Board" look is confusing in a specific messy graph.
	 */
	TSharedPtr<FUICommandInfo> ToggleWireStyle;
};