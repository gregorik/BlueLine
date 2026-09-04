// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FUICommandList;
struct FGraphPanelPinConnectionFactory;
class FBlueLineGraphPinFactory;

class FBlueLineGraphModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TSharedPtr<FUICommandList> GetPluginCommands() const { return PluginCommands; }

private:

	void InstallGraphConnectionFactory();
	void UninstallGraphConnectionFactory();
	void InstallGraphPinFactory();
	void UninstallGraphPinFactory();
	void RegisterCommands();

	TSharedPtr<FGraphPanelPinConnectionFactory> BlueLineConnectionFactory;
	TSharedPtr<FBlueLineGraphPinFactory> BlueLinePinFactory;
	TSharedPtr<FUICommandList> PluginCommands;
	FDelegateHandle MainFrameLoadedHandle;
};
