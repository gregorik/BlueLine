// Copyright YourTeamName. All Rights Reserved.

#include "BlueLineGraphModule.h"

// Visualization Includes
#include "Drawing/FBlueLineGraphPanelFactory.h"
#include "Drawing/FBlueLineGraphPinFactory.h" // <--- CRITICAL INCLUDE
#include "Formatting/BlueLineFormatter.h"
#include "Styles/FBlueLineStyle.h"
#include "Settings/UBlueLineEditorSettings.h"

// Framework Includes
#include "Commands/FBlueLineCommands.h"
#include "BlueLineLog.h" 
#include "Modules/ModuleManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Framework/Commands/UICommandList.h"
#include "EdGraphUtilities.h"

#define LOCTEXT_NAMESPACE "BlueLineGraph"

void FBlueLineGraphModule::StartupModule()
{
	FBlueLineStyle::Initialize();
	FBlueLineCommands::Register();

	RegisterCommands();

	// Install Visual Factories
	InstallGraphDrawingPolicy();
	InstallGraphPinFactory();

	UE_LOG(LogBlueLineCore, Log, TEXT("BlueLineGraph: Module Started."));
}

void FBlueLineGraphModule::ShutdownModule()
{
	UninstallGraphPinFactory();
	UninstallGraphDrawingPolicy();

	if (PluginCommands.IsValid())
	{
		PluginCommands.Reset();
	}

	FBlueLineCommands::Unregister();
	FBlueLineStyle::Shutdown();
}

void FBlueLineGraphModule::RegisterCommands()
{
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FBlueLineCommands::Get().AutoFormatSelected,
		FExecuteAction::CreateStatic(&FBlueLineFormatter::FormatActiveGraphSelection)
	);

	PluginCommands->MapAction(
		FBlueLineCommands::Get().ToggleWireStyle,
		FExecuteAction::CreateLambda([]() {
			UBlueLineEditorSettings* S = GetMutableDefault<UBlueLineEditorSettings>();
			S->bEnableManhattanRouting = !S->bEnableManhattanRouting;
			S->PostEditChange();
			S->SaveConfig();
			})
	);

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		MainFrame.GetMainFrameCommandBindings()->Append(PluginCommands.ToSharedRef());
	}
}

void FBlueLineGraphModule::InstallGraphDrawingPolicy()
{
	if (BlueLineGraphPanelFactory.IsValid()) return;

	// Note: Explicit type <FGraphPanelNodeFactory> helps MakeShareable matching
	BlueLineGraphPanelFactory = MakeShareable<FGraphPanelNodeFactory>(new FBlueLineGraphPanelFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(BlueLineGraphPanelFactory);
}

void FBlueLineGraphModule::UninstallGraphDrawingPolicy()
{
	if (BlueLineGraphPanelFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(BlueLineGraphPanelFactory);
		BlueLineGraphPanelFactory.Reset();
	}
}

void FBlueLineGraphModule::InstallGraphPinFactory()
{
	if (BlueLinePinFactory.IsValid()) return;

	BlueLinePinFactory = MakeShareable(new FBlueLineGraphPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(BlueLinePinFactory);
}

void FBlueLineGraphModule::UninstallGraphPinFactory()
{
	if (BlueLinePinFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinFactory(BlueLinePinFactory);
		BlueLinePinFactory.Reset();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueLineGraphModule, BlueLineGraph)