// Copyright YourTeamName. All Rights Reserved.

#include "Commands/FBlueLineCommands.h"
#include "Styles/FBlueLineStyle.h"

#define LOCTEXT_NAMESPACE "BlueLineGraph"

void FBlueLineCommands::RegisterCommands()
{
	UI_COMMAND(
		AutoFormatSelected, 
		"Soft Align (Magnet)", 
		"Aligns the selected nodes grid-relative to their input connections. Does not touch unselected nodes.", 
		EUserInterfaceActionType::Button, 
		FInputChord(EModifierKey::Shift, EKeys::Q) // Default: Shift + Q
	);

	UI_COMMAND(
		ToggleWireStyle, 
		"Toggle Wire Style", 
		"Instantly switches between BlueLine Manhattan wires and Standard Bezier splines.", 
		EUserInterfaceActionType::Button, 
		FInputChord(EKeys::F8) // Default: F8
	);

	UI_COMMAND(
		RigidifyConnections,
		"Rigidify Wires",
		"Inserts Reroute nodes between selected nodes to force 90-degree lines.",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Shift, EKeys::R)
	);
}

#undef LOCTEXT_NAMESPACE