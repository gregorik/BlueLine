// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// Forward Declarations
class FUICommandList;
struct FGraphPanelNodeFactory;
class FBlueLineGraphPinFactory; // <--- NEW Forward Declaration

class FBlueLineGraphModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	// Panel Factory (Nodes/Connections)
	void InstallGraphDrawingPolicy();
	void UninstallGraphDrawingPolicy();

	// Pin Factory (Colored Pins)
	void InstallGraphPinFactory();   // <--- NEW Declaration
	void UninstallGraphPinFactory(); // <--- NEW Declaration

	void RegisterCommands();

	// Members
	TSharedPtr<FGraphPanelNodeFactory> BlueLineGraphPanelFactory;
	TSharedPtr<FBlueLineGraphPinFactory> BlueLinePinFactory; // <--- NEW Storage

	TSharedPtr<FUICommandList> PluginCommands;
};