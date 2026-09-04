// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "Commands/FBlueLineCommands.h"
#include "Styles/FBlueLineStyle.h"  // From BlueLineCore

#define LOCTEXT_NAMESPACE "BlueLineGraph"

void FBlueLineCommands::RegisterCommands()
{
	UI_COMMAND(
		AutoFormatSelected, 
		"Auto-Format Selection", 
		"Aligns the selected nodes grid-relative to their input connections. Does not touch unselected nodes.", 
		EUserInterfaceActionType::Button, 
		FInputChord(EModifierKey::Shift, EKeys::Q) // Default: Shift + Q
	);

	UI_COMMAND(
		ToggleWireStyle, 
		"Toggle Wire Style", 
		"Instantly switches between BlueLine Manhattan wires and Standard Bezier splines.", 
		EUserInterfaceActionType::Button, 
		FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::W) // Shift+Alt+W to avoid conflict with "Possess or Eject Player"
	);

	UI_COMMAND(
		RigidifyConnections,
		"Rigidify Wires",
		"Inserts Reroute nodes between selected nodes to force 90-degree lines.",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Shift, EKeys::R)
	);

	UI_COMMAND(
		CleanGraph,
		"Clean Graph",
		"Analyzes the entire graph and performs an intelligent, non-destructive reorganization.",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Shift, EKeys::C)
	);

	// Note: AutoTagGraph command is defined in BlueLineSmartTags module (Shift+T)
	// to avoid duplicate command registration conflicts
}

#undef LOCTEXT_NAMESPACE
