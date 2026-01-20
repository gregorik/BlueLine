// Copyright YourTeamName. All Rights Reserved.

#include "Settings/UBlueLineEditorSettings.h"

// Define the static delegate
UBlueLineEditorSettings::FOnBlueLineSettingsChanged UBlueLineEditorSettings::OnSettingsChanged;

UBlueLineEditorSettings::UBlueLineEditorSettings()
{
	// Visual Defaults
	bEnableManhattanRouting = true;
	bDimWiresBehindNodes = true;

	// Formatting Defaults
	FormatterPadding = 80.0f;       // Comfortable breathing room
	MagnetEvaluationDistance = 300.0f; // Snap if within ~3 grid squares
}

#if WITH_EDITOR
void UBlueLineEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// When any setting changes (like toggling wires), broadcast this event.
	// The Graph Module listens to this to redraw the graph immediately.
	OnSettingsChanged.Broadcast();
}
#endif