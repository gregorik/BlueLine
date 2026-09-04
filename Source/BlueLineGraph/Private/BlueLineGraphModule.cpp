// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "BlueLineGraphModule.h"

// Visualization Includes
#include "Drawing/FBlueLineGraphConnectionFactory.h"
#include "Drawing/FBlueLineGraphPinFactory.h"
#include "Formatting/BlueLineFormatter.h"
#include "Styles/FBlueLineStyle.h"  // From BlueLineCore (shared)
#include "BlueLineCore/Public/Settings/UBlueLineEditorSettings.h"

// Routing & UI Includes
#include "Routing/FBlueLineManhattanRouter.h"
#include "Routing/FBlueLineConnectionInterceptor.h"
#include "Routing/FBlueLineWireSnapper.h"
#include "Formatting/FBlueLineGraphCleaner.h"
#include "UI/FBlueLineGraphMenuExtender.h"

// Framework Includes
#include "Commands/FBlueLineCommands.h"
#include "BlueLineLog.h" 
#include "Modules/ModuleManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Framework/Commands/UICommandList.h"
#include "EdGraphUtilities.h"

// For dialogs
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "BlueLineGraph"

void FBlueLineGraphModule::StartupModule()
{
	// Note: FBlueLineStyle is initialized by BlueLineCore (shared style system)
	FBlueLineCommands::Register();
	RegisterCommands();

	// Factories
	InstallGraphConnectionFactory();
	InstallGraphPinFactory();

	// Menu Extension
	FBlueLineGraphMenuExtender::Register(); // <--- Init Context Menu

	FBlueLineConnectionInterceptor::Enable();
	FBlueLineWireSnapper::Enable();

	UE_LOG(LogBlueLineCore, Log, TEXT("BlueLineGraph: Module Started."));
}

void FBlueLineGraphModule::ShutdownModule()
{
	FBlueLineConnectionInterceptor::Disable();
	FBlueLineWireSnapper::Disable();

	// Cleanup Menus
	FBlueLineGraphMenuExtender::Unregister();

	// Cleanup Factories
	UninstallGraphPinFactory();
	UninstallGraphConnectionFactory();

	if (MainFrameLoadedHandle.IsValid())
	{
		FModuleManager::Get().OnModulesChanged().Remove(MainFrameLoadedHandle);
		MainFrameLoadedHandle.Reset();
	}

	if (PluginCommands.IsValid())
	{
		PluginCommands.Reset();
	}

	FBlueLineCommands::Unregister();
	// Note: FBlueLineStyle is shut down by BlueLineCore (shared style system)
}

void FBlueLineGraphModule::RegisterCommands()
{
	PluginCommands = MakeShareable(new FUICommandList);
	UE_LOG(LogBlueLineCore, Log, TEXT("BlueLineGraph: Registering commands..."));

	// Shift+Q (Format)
	PluginCommands->MapAction(
		FBlueLineCommands::Get().AutoFormatSelected,
		FExecuteAction::CreateStatic(&FBlueLineFormatter::FormatActiveGraphSelection)
	);

	// Shift+R (Manhattan Router)
	PluginCommands->MapAction(
		FBlueLineCommands::Get().RigidifyConnections,
		FExecuteAction::CreateStatic(&FBlueLineManhattanRouter::RigidifySelectedConnections)
	);

	// Shift+C (Clean Graph)
	PluginCommands->MapAction(
		FBlueLineCommands::Get().CleanGraph,
		FExecuteAction::CreateStatic(&FBlueLineGraphCleaner::CleanActiveGraph)
	);

	// Shift+W Toggle (Cycles wire styles)
	UE_LOG(LogBlueLineCore, Log, TEXT("BlueLineGraph: Mapping Shift+W to ToggleWireStyle"));
	PluginCommands->MapAction(
		FBlueLineCommands::Get().ToggleWireStyle,
		FExecuteAction::CreateLambda([]() {
			UBlueLineEditorSettings* S = GetMutableDefault<UBlueLineEditorSettings>();
			int32 CurrentMethod = static_cast<int32>(S->RoutingMethod);
			int32 NextMethod = (CurrentMethod + 1) % 4;
			S->RoutingMethod = static_cast<EBlueLineRoutingMethod>(NextMethod);
			S->PostEditChange();
			S->SaveConfig();
			
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication& SlateApp = FSlateApplication::Get();
				TArray<TSharedRef<SWindow>> AllWindows;
				SlateApp.GetAllVisibleWindowsOrdered(AllWindows);
				for (const TSharedRef<SWindow>& Window : AllWindows)
				{
					TArray<TSharedRef<SWidget>> WidgetStack;
					WidgetStack.Add(Window);
					while (WidgetStack.Num() > 0)
					{
						TSharedRef<SWidget> Widget = WidgetStack.Pop();
						if (Widget->GetType() == TEXT("SGraphEditor"))
						{
							Widget->Invalidate(EInvalidateWidgetReason::PaintAndVolatility);
						}
						FChildren* Children = Widget->GetChildren();
						for (int32 i = 0; i < Children->Num(); ++i)
						{
							WidgetStack.Add(Children->GetChildAt(i));
						}
					}
				}
			}
			
			static const TCHAR* RoutingNames[] = { TEXT("Curved"), TEXT("Manhattan"), TEXT("Circuit"), TEXT("Hybrid") };
			UE_LOG(LogBlueLineCore, Log, TEXT("ToggleWireStyle executed: routing=%d (%s)"), NextMethod, RoutingNames[NextMethod]);
		})
	);

	// Bind to MainFrame (global commands)
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		MainFrame.GetMainFrameCommandBindings()->Append(PluginCommands.ToSharedRef());
		UE_LOG(LogBlueLineCore, Log, TEXT("BlueLineGraph: Commands bound to MainFrame"));
	}
	else
	{
		MainFrameLoadedHandle = FModuleManager::Get().OnModulesChanged().AddLambda([this](FName Name, EModuleChangeReason Reason)
		{
			if (Name == "MainFrame" && Reason == EModuleChangeReason::ModuleLoaded)
			{
				if (FModuleManager::Get().IsModuleLoaded("MainFrame") && PluginCommands.IsValid())
				{
					IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
					MainFrame.GetMainFrameCommandBindings()->Append(PluginCommands.ToSharedRef());
					UE_LOG(LogBlueLineCore, Log, TEXT("BlueLineGraph: Commands bound to MainFrame dynamically"));
				}
			}
		});
	}
}

void FBlueLineGraphModule::InstallGraphConnectionFactory()
{
	if (BlueLineConnectionFactory.IsValid()) return;
	BlueLineConnectionFactory = MakeShareable(new FBlueLineGraphConnectionFactory());
	FEdGraphUtilities::RegisterVisualPinConnectionFactory(BlueLineConnectionFactory);
}

void FBlueLineGraphModule::UninstallGraphConnectionFactory()
{
	if (BlueLineConnectionFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinConnectionFactory(BlueLineConnectionFactory);
		BlueLineConnectionFactory.Reset();
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

