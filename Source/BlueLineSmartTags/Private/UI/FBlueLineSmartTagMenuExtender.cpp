// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "UI/FBlueLineSmartTagMenuExtender.h"
#include "BlueLineLog.h"
#include "FBlueLineSmartTagCommands.h"
#include "GraphEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "ScopedTransaction.h"
#include "Demos/UK2Node_KingSafety.h"
#include "Demos/UK2Node_AWSTag.h"
#include "Demos/UK2Node_TagDemo.h"
#include "Utils/BlueLineContextUtils.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "BlueLineSmartTags"

static FDelegateHandle SmartTagMenuExtenderHandle;
static const FName BlueLineSmartTagsOwnerName(TEXT("BlueLineSmartTags"));
static const FName BlueLineDemoSectionName(TEXT("BlueLineDemo"));
static const FName SpawnMessyDemoEntryName(TEXT("SpawnMessyDemo"));

void FBlueLineSmartTagMenuExtender::Register()
{
    UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine: Registering Smart Tag Menu Extender..."));
    
    FGraphEditorModule& GraphEditorModule = FModuleManager::LoadModuleChecked<FGraphEditorModule>("GraphEditor");
    FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode Delegate =
        FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode::CreateStatic(&FBlueLineSmartTagMenuExtender::OnExtendContextMenu);

    SmartTagMenuExtenderHandle = Delegate.GetHandle();
    GraphEditorModule.GetAllGraphEditorContextMenuExtender().Add(Delegate);

    // Modern UE5 ToolMenus Fallback
    FToolMenuOwnerScoped MenuOwner(BlueLineSmartTagsOwnerName);
    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("GraphEditor.GraphContext");
    if (Menu)
    {
        UToolMenus::Get()->RemoveEntry("GraphEditor.GraphContext", BlueLineDemoSectionName, SpawnMessyDemoEntryName);
        FToolMenuSection& Section = Menu->FindOrAddSection(BlueLineDemoSectionName, LOCTEXT("DemoHeader", "BlueLine Demo"));

        FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
            SpawnMessyDemoEntryName,
            LOCTEXT("SpawnDemo", "Spawn Messy Demo"),
            LOCTEXT("SpawnDemoTip", "Generates a tangled web of demo nodes to test BlueLine organization."),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Macro_16x"),
            FUIAction(FExecuteAction::CreateStatic(&FBlueLineSmartTagMenuExtender::ExecuteSpawnMessyDemoFromMenu))
        );
        Section.AddEntry(Entry);
    }
}

void FBlueLineSmartTagMenuExtender::ExecuteSpawnMessyDemoFromMenu()
{
    ExecuteSpawnMessyDemo(FBlueLineContextUtils::GetCurrentGraphFromFocus());
}


void FBlueLineSmartTagMenuExtender::Unregister()
{
    if (FModuleManager::Get().IsModuleLoaded("GraphEditor"))
    {
        FGraphEditorModule& GraphEditorModule = FModuleManager::GetModuleChecked<FGraphEditorModule>("GraphEditor");
        GraphEditorModule.GetAllGraphEditorContextMenuExtender().RemoveAll(
            [](const FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode& Delegate)
            {
                return Delegate.GetHandle() == SmartTagMenuExtenderHandle;
            }
        );
    }

    if (UToolMenus* ToolMenus = UToolMenus::TryGet())
    {
        ToolMenus->UnregisterOwnerByName(BlueLineSmartTagsOwnerName);
    }
}

TSharedRef<FExtender> FBlueLineSmartTagMenuExtender::OnExtendContextMenu(const TSharedRef<FUICommandList> CommandList, const UEdGraph* Graph, const UEdGraphNode* Node, const UEdGraphPin* Pin, bool bIsDebug)
{
    TSharedRef<FExtender> Extender = MakeShareable(new FExtender());

    // Only extend if right-clicking the graph or a node (not a pin, as that's too crowded)
    const bool bIsK2Graph = Graph && Graph->GetSchema() && Graph->GetSchema()->IsA(UEdGraphSchema_K2::StaticClass());
    if (!Pin && bIsK2Graph)
    {
        Extender->AddMenuExtension(
            "EdGraphSchemaOrganization",
            EExtensionHook::After,
            CommandList,
            FMenuExtensionDelegate::CreateStatic(&FBlueLineSmartTagMenuExtender::AddGraphMenuEntries, const_cast<UEdGraph*>(Graph))
        );

        Extender->AddMenuExtension(
            "GraphCanvasContextMenuFreeSelection",
            EExtensionHook::After,
            CommandList,
            FMenuExtensionDelegate::CreateStatic(&FBlueLineSmartTagMenuExtender::AddGraphMenuEntries, const_cast<UEdGraph*>(Graph))
        );
    }

    return Extender;
}

void FBlueLineSmartTagMenuExtender::AddGraphMenuEntries(FMenuBuilder& MenuBuilder, UEdGraph* Graph)
{
    if (FBlueLineSmartTagCommands::IsRegistered() && FBlueLineSmartTagCommands::Get().AutoTagGraph.IsValid())
    {
        MenuBuilder.BeginSection("BlueLineSmartTags", LOCTEXT("SmartTagsHeader", "BlueLine Smart Tags"));
        {
            MenuBuilder.AddMenuEntry(FBlueLineSmartTagCommands::Get().AutoTagGraph);
        }
        MenuBuilder.EndSection();
    }

    MenuBuilder.BeginSection("BlueLineDemo", LOCTEXT("DemoHeader", "BlueLine Demo"));
    {
        MenuBuilder.AddMenuEntry(
            LOCTEXT("SpawnDemo", "Spawn Messy Demo"),
            LOCTEXT("SpawnDemoTip", "Generates a tangled web of demo nodes to test BlueLine organization."),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Macro_16x"),
            FUIAction(FExecuteAction::CreateStatic(&FBlueLineSmartTagMenuExtender::ExecuteSpawnMessyDemo, Graph))
        );
    }
    MenuBuilder.EndSection();
}

void FBlueLineSmartTagMenuExtender::ExecuteSpawnMessyDemo(UEdGraph* Graph)
{
    if (!Graph) return;

    const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
    if (!K2Schema)
    {
        UE_LOG(LogBlueLineCore, Warning, TEXT("BlueLine: Spawn Messy Demo is only supported in Blueprint graphs"));
        return;
    }

    const FScopedTransaction Transaction(LOCTEXT("SpawnDemoTrans", "Spawn Messy Demo"));
    Graph->Modify();

    // Create a cluster of Combat nodes
    TArray<UK2Node_KingSafety*> CombatNodes;
    for (int32 i = 0; i < 4; ++i)
    {
        FGraphNodeCreator<UK2Node_KingSafety> Creator(*Graph);
        UK2Node_KingSafety* Node = Creator.CreateNode();
        Node->NodePosX = 1000 + (i * 120);
        Node->NodePosY = 1000 + (i * 200); 
        Creator.Finalize();
        CombatNodes.Add(Node);
    }

    // Inter-connect combat nodes messily
    for (int32 i = 0; i < CombatNodes.Num() - 1; ++i)
    {
        K2Schema->TryCreateConnection(CombatNodes[i]->GetThenPin(), CombatNodes[i+1]->GetInputPin());
        K2Schema->TryCreateConnection(CombatNodes[i]->FindPin(TEXT("SafetyScore")), CombatNodes[i+1]->FindPin(TEXT("KingDanger")));
    }

    // Create a cluster of Data nodes
    TArray<UK2Node_AWSTag*> DataNodes;
    for (int32 i = 0; i < 3; ++i)
    {
        FGraphNodeCreator<UK2Node_AWSTag> Creator(*Graph);
        UK2Node_AWSTag* Node = Creator.CreateNode();
        Node->NodePosX = 2000 + (i * 150);
        Node->NodePosY = 1200 - (i * 180); 
        Creator.Finalize();
        DataNodes.Add(Node);
    }

    // Connect clusters messily
    if (CombatNodes.Num() > 1 && DataNodes.Num() > 0)
    {
        K2Schema->TryCreateConnection(CombatNodes[1]->FindPin(TEXT("SafetyScore")), DataNodes[0]->FindPin(TEXT("ResourceName")));
        K2Schema->TryCreateConnection(CombatNodes.Last()->GetThenPin(), DataNodes.Last()->FindPin(TEXT("ResourceARN")));
    }

    Graph->NotifyGraphChanged();
}

#undef LOCTEXT_NAMESPACE

