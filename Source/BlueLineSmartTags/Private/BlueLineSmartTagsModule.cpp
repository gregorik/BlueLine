// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "BlueLineSmartTagsModule.h"
#include "BlueLineLog.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Data/UBlueLineThemeData.h"
#include "Customization/FBlueLineTagCustomization.h"
#include "UI/FBlueLineSmartTagMenuExtender.h"
#include "FBlueLineSmartTagCommands.h"
#include "FBlueLineSmartTagAnalyzer.h"
#include "Utils/BlueLineContextUtils.h"
#include "EdGraph/EdGraph.h"
#include "Framework/Commands/UICommandList.h"
#include "Interfaces/IMainFrameModule.h"
#include "GameplayTagsManager.h"
#include "Settings/UBlueLineEditorSettings.h"

#define LOCTEXT_NAMESPACE "BlueLineSmartTags"

void FBlueLineSmartTagsModule::StartupModule()
{
	FModuleManager::Get().LoadModuleChecked<IModuleInterface>("GraphEditor");
	FBlueLineSmartTagCommands::Register();
	FBlueLineSmartTagMenuExtender::Register();
	RegisterCommands();
	RegisterPropertyTypeCustomizations();

	// Register Native Gameplay Tags
	UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();
	TagManager.AddNativeGameplayTag(FName("BlueLine.Type.Movement"), TEXT("Movement related nodes"));
	TagManager.AddNativeGameplayTag(FName("BlueLine.Type.Combat"), TEXT("Combat related nodes"));
	TagManager.AddNativeGameplayTag(FName("BlueLine.Type.UI"), TEXT("UI related nodes"));
	TagManager.AddNativeGameplayTag(FName("BlueLine.Type.Input"), TEXT("Input related nodes"));
	TagManager.AddNativeGameplayTag(FName("BlueLine.Type.Networking"), TEXT("Networking related nodes"));
	TagManager.AddNativeGameplayTag(FName("BlueLine.Type.Audio"), TEXT("Audio related nodes"));
	TagManager.AddNativeGameplayTag(FName("BlueLine.Type.Visuals"), TEXT("Visuals related nodes"));
	TagManager.AddNativeGameplayTag(FName("BlueLine.Type.AI"), TEXT("AI related nodes"));
	TagManager.AddNativeGameplayTag(FName("BlueLine.Type.Logic"), TEXT("Logic related nodes"));
	TagManager.AddNativeGameplayTag(FName("BlueLine.Type.Data"), TEXT("Data related nodes"));
	TagManager.AddNativeGameplayTag(FName("BlueLine.Type.Unknown"), TEXT("Unknown tag category"));
}

void FBlueLineSmartTagsModule::ShutdownModule()
{
	UnregisterPropertyTypeCustomizations();
	FBlueLineSmartTagMenuExtender::Unregister();
	if (PluginCommands.IsValid())
	{
		if (FBlueLineSmartTagCommands::IsRegistered())
		{
			PluginCommands->UnmapAction(FBlueLineSmartTagCommands::Get().AutoTagGraph);
		}
		PluginCommands.Reset();
	}
	FBlueLineSmartTagCommands::Unregister();
}

void FBlueLineSmartTagsModule::RegisterCommands()
{
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FBlueLineSmartTagCommands::Get().AutoTagGraph,
		FExecuteAction::CreateStatic(&FBlueLineSmartTagsModule::ExecuteAutoTagGraph),
		FCanExecuteAction::CreateLambda([]()
		{
			const UBlueLineEditorSettings* Settings = UBlueLineEditorSettings::Get();
			return Settings && Settings->bEnableBlueLine && Settings->bEnableSmartTags && Settings->bEnableAutoTagCommand;
		})
	);

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		MainFrame.GetMainFrameCommandBindings()->Append(PluginCommands.ToSharedRef());
	}
}

void FBlueLineSmartTagsModule::ExecuteAutoTagGraph()
{
	const UBlueLineEditorSettings* Settings = UBlueLineEditorSettings::Get();
	if (!Settings || !Settings->bEnableBlueLine || !Settings->bEnableSmartTags || !Settings->bEnableAutoTagCommand)
	{
		return;
	}

	UEdGraph* Graph = FBlueLineContextUtils::GetCurrentGraphFromFocus();
	if (!Graph)
	{
		return;
	}

	TArray<UEdGraphNode*> SelectedNodes;
	FBlueLineContextUtils::GetSelectedNodesFromFocus(SelectedNodes);
	for (UEdGraphNode* Node : SelectedNodes)
	{
		if (Node)
		{
			UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine: Selected node '%s' at position (%d, %d)"),
				*Node->GetNodeTitle(ENodeTitleType::ListView).ToString(), Node->NodePosX, Node->NodePosY);
		}
	}

	UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine: Total selected UEdGraphNodes: %d"), SelectedNodes.Num());

	FBlueLineSmartTagAnalyzer::AutoTagGraph(Graph, SelectedNodes);
}

void FBlueLineSmartTagsModule::RegisterPropertyTypeCustomizations()
{
	if (!FModuleManager::Get().IsModuleLoaded("PropertyEditor")) return;

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Customize our Theme Style struct only.
	// We leave FGameplayTag alone so the engine defaults handle the dropdown logic.
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FBlueLineTagStyle::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FBlueLineTagCustomization::MakeInstance)
	);
}

void FBlueLineSmartTagsModule::UnregisterPropertyTypeCustomizations()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(FBlueLineTagStyle::StaticStruct()->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FBlueLineSmartTagsModule, BlueLineSmartTags)
