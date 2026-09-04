// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "UI/FBlueLineGraphMenuExtender.h"
#include "Routing/FBlueLineManhattanRouter.h"
#include "Commands/FBlueLineCommands.h"
#include "FBlueLineSmartTagCommands.h"
#include "GraphEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphNode.h"
#include "SGraphPanel.h"
#include "BlueLineLog.h"
#include "ScopedTransaction.h"

#include "Utils/BlueLineContextUtils.h"

static FDelegateHandle GraphEditorMenuExtenderHandle;

void FBlueLineGraphMenuExtender::Register()
{
	FGraphEditorModule& GraphEditorModule = FModuleManager::LoadModuleChecked<FGraphEditorModule>("GraphEditor");

	// 1. Create the Delegate
	FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode Delegate =
		FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode::CreateStatic(&FBlueLineGraphMenuExtender::OnExtendContextMenu);

	// 2. Extract Handle BEFORE adding (The handle is inside the delegate instance)
	GraphEditorMenuExtenderHandle = Delegate.GetHandle();

	// 3. Add to array (This adds the delegate to the list, we ignore the int return value)
	GraphEditorModule.GetAllGraphEditorContextMenuExtender().Add(Delegate);

	UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine: Context Menu Extender Registered."));
}

void FBlueLineGraphMenuExtender::Unregister()
{
	if (FModuleManager::Get().IsModuleLoaded("GraphEditor"))
	{
		FGraphEditorModule& GraphEditorModule = FModuleManager::GetModuleChecked<FGraphEditorModule>("GraphEditor");

		if (GraphEditorMenuExtenderHandle.IsValid())
		{
			// 4. Remove using the Handle ID we saved
			GraphEditorModule.GetAllGraphEditorContextMenuExtender().RemoveAll(
				[](const FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode& Delegate)
				{
					return Delegate.GetHandle() == GraphEditorMenuExtenderHandle;
				}
			);
			GraphEditorMenuExtenderHandle.Reset();
		}
	}
}

#include "BlueLineGraphModule.h"

TSharedRef<FExtender> FBlueLineGraphMenuExtender::OnExtendContextMenu(const TSharedRef<FUICommandList> CommandList, const UEdGraph* Graph, const UEdGraphNode* Node, const UEdGraphPin* Pin, bool bIsDebug)
{
	// Link the plugin commands to the context menu's command list so the shortcuts actually execute
	FBlueLineGraphModule& Module = FModuleManager::GetModuleChecked<FBlueLineGraphModule>("BlueLineGraph");
	if (Module.GetPluginCommands().IsValid())
	{
		CommandList->Append(Module.GetPluginCommands().ToSharedRef());
	}

	TSharedRef<FExtender> Extender = MakeShareable(new FExtender());

	if (Pin)
	{
		const UEdGraphPin* ConstPin = Pin;
		Extender->AddMenuExtension(
			"EdGraphSchemaPinActions",
			EExtensionHook::After,
			CommandList,
			FMenuExtensionDelegate::CreateStatic(&FBlueLineGraphMenuExtender::AddPinMenuEntries, ConstPin)
		);
	}
	else if (Node || (Graph && !Pin))
	{
		Extender->AddMenuExtension(
			"EdGraphSchemaOrganization",
			EExtensionHook::After,
			CommandList,
			FMenuExtensionDelegate::CreateStatic(&FBlueLineGraphMenuExtender::AddGraphMenuEntries, const_cast<UEdGraph*>(Graph))
		);
	}

	return Extender;
}

void FBlueLineGraphMenuExtender::AddPinMenuEntries(FMenuBuilder& MenuBuilder, const UEdGraphPin* Pin)
{
	if (!Pin || Pin->LinkedTo.Num() == 0) return;

	MenuBuilder.BeginSection("BlueLine", NSLOCTEXT("BlueLine", "Header", "BlueLine Routing"));
	{
		UEdGraphPin* MutablePin = const_cast<UEdGraphPin*>(Pin);

		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("BlueLine", "Straighten", "Straighten Connections"),
			NSLOCTEXT("BlueLine", "StraightenTip", "Insert Reroute nodes to force 90-degree lines."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.RerouteNode_16x"),
			FUIAction(FExecuteAction::CreateStatic(&FBlueLineGraphMenuExtender::ExecuteStraightenConnection, MutablePin))
		);
	}
	MenuBuilder.EndSection();
}

void FBlueLineGraphMenuExtender::AddGraphMenuEntries(FMenuBuilder& MenuBuilder, UEdGraph* Graph)
{
	if (!Graph) return;

	MenuBuilder.BeginSection("BlueLine", NSLOCTEXT("BlueLine", "Header", "BlueLine Graph"));
	{
		if (FBlueLineCommands::Get().CleanGraph.IsValid())
			MenuBuilder.AddMenuEntry(FBlueLineCommands::Get().CleanGraph);
		if (FBlueLineCommands::Get().AutoFormatSelected.IsValid())
			MenuBuilder.AddMenuEntry(FBlueLineCommands::Get().AutoFormatSelected);
		if (FBlueLineCommands::Get().RigidifyConnections.IsValid())
			MenuBuilder.AddMenuEntry(FBlueLineCommands::Get().RigidifyConnections);

		MenuBuilder.AddMenuSeparator();

		if (FBlueLineSmartTagCommands::Get().AutoTagGraph.IsValid())
			MenuBuilder.AddMenuEntry(FBlueLineSmartTagCommands::Get().AutoTagGraph);

		MenuBuilder.AddMenuSeparator();

		if (FBlueLineCommands::Get().ToggleWireStyle.IsValid())
			MenuBuilder.AddMenuEntry(FBlueLineCommands::Get().ToggleWireStyle);

		MenuBuilder.AddMenuSeparator();

		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("BlueLine", "CleanOrphans", "Cleanup Auto-Reroutes"),
			NSLOCTEXT("BlueLine", "CleanTip", "Remove unused reroute nodes created by BlueLine."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.BreakLink_16x"),
			FUIAction(FExecuteAction::CreateStatic(&FBlueLineGraphMenuExtender::ExecuteCleanup, Graph))
		);
	}
	MenuBuilder.EndSection();
}

void FBlueLineGraphMenuExtender::ExecuteStraightenConnection(UEdGraphPin* Pin)
{
	if (!Pin || !Pin->GetOwningNode()) return;
	UEdGraph* Graph = Pin->GetOwningNode()->GetGraph();
	if (!Graph) return;

	const FScopedTransaction Transaction(NSLOCTEXT("BlueLine", "Straighten", "Straighten Connections"));
	Graph->Modify();

	TArray<UEdGraphPin*> Links = Pin->LinkedTo;

	for (UEdGraphPin* LinkedPin : Links)
	{
		if (!LinkedPin) continue;

		UEdGraphPin* Output = (Pin->Direction == EGPD_Output) ? Pin : LinkedPin;
		UEdGraphPin* Input = (Pin->Direction == EGPD_Input) ? Pin : LinkedPin;

		if (!Input->GetOwningNode() || !Output->GetOwningNode()) continue;

		// IQ Check: Only straighten if there is enough horizontal space to prevent tangling.
		// Use the same logic as the main Rigidify command.
		if (Input->GetOwningNode()->NodePosX > Output->GetOwningNode()->NodePosX + 100)
		{
			FBlueLineManhattanRouter::RouteConnection(Output, Input, Graph);
		}
	}

	Graph->NotifyGraphChanged();
}

void FBlueLineGraphMenuExtender::ExecuteCleanup(UEdGraph* Graph)
{
	FBlueLineManhattanRouter::CleanupOrphanedRerouteNodes(Graph);
}

void FBlueLineGraphMenuExtender::ExecuteStraightenAll(UEdGraph* Graph)
{
	if (!Graph) return;

	const FScopedTransaction Transaction(NSLOCTEXT("BlueLine", "StraightenAll", "Straighten All Connections"));
	Graph->Modify();

	// Process all connections in the graph
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output) continue;

			// FIX: Copy LinkedTo array to prevent mutation during iteration
			TArray<UEdGraphPin*> Links = Pin->LinkedTo;
			for (UEdGraphPin* LinkedPin : Links)
			{
				if (!LinkedPin) continue;

				UEdGraphPin* Output = Pin;
				UEdGraphPin* Input = LinkedPin;

				if (!Input->GetOwningNode() || !Output->GetOwningNode()) continue;

				// Only straighten if there is enough horizontal space
				if (Input->GetOwningNode()->NodePosX > Output->GetOwningNode()->NodePosX + 100)
				{
					FBlueLineManhattanRouter::RouteConnection(Output, Input, Graph);
				}
			}
		}
	}

	Graph->NotifyGraphChanged();
}
