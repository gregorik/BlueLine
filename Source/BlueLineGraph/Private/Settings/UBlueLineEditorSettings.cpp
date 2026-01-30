// Copyright YourTeamName. All Rights Reserved.

#include "Settings/UBlueLineEditorSettings.h"

// Define the static delegate
UBlueLineEditorSettings::FOnBlueLineSettingsChanged UBlueLineEditorSettings::OnSettingsChanged;

UBlueLineEditorSettings::UBlueLineEditorSettings()
{
	bEnableManhattanRouting = true;
	
	// New Defaults
	StubLength = 20.0f;
	StubThickness = 2.0f;
	bShowConnectionCount = true;

	FormatterPadding = 80.0f;
	MagnetEvaluationDistance = 300.0f;
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