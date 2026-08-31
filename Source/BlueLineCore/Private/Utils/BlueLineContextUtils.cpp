// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "Utils/BlueLineContextUtils.h"
#include "BlueLineLog.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "SGraphPanel.h"
#include "GraphEditor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"

namespace
{
    const FName GraphEditorType(TEXT("SGraphEditor"));
    const FName GraphPanelType(TEXT("SGraphPanel"));

    bool IsGraphEditorWidgetType(const FName WidgetType)
    {
        return WidgetType == GraphEditorType;
    }

    bool IsGraphPanelWidgetType(const FName WidgetType)
    {
        return WidgetType == GraphPanelType;
    }

    TSharedPtr<SGraphEditor> TryCastGraphEditor(const TSharedPtr<SWidget>& Widget)
    {
        if (!Widget.IsValid() || !IsGraphEditorWidgetType(Widget->GetType()))
        {
            return nullptr;
        }

        return StaticCastSharedPtr<SGraphEditor>(Widget);
    }

    TSharedPtr<SGraphPanel> TryCastGraphPanel(const TSharedPtr<SWidget>& Widget)
    {
        if (!Widget.IsValid() || !IsGraphPanelWidgetType(Widget->GetType()))
        {
            return nullptr;
        }

        return StaticCastSharedPtr<SGraphPanel>(Widget);
    }
}

TSharedPtr<SGraphPanel> FBlueLineContextUtils::GetFocusedGraphPanel()
{
    if (!FSlateApplication::IsInitialized())
    {
        return nullptr;
    }

    TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
    if (!FocusedWidget.IsValid())
    {
        return nullptr;
    }

    // Try to find SGraphPanel parent
    TSharedPtr<SWidget> CurrentWidget = FocusedWidget;
    int32 Depth = 0;

    while (CurrentWidget.IsValid() && Depth < 50)
    {
        if (TSharedPtr<SGraphPanel> GraphPanel = TryCastGraphPanel(CurrentWidget))
        {
            return GraphPanel;
        }

        CurrentWidget = CurrentWidget->GetParentWidget();
        Depth++;
    }

    return nullptr;
}

TSharedPtr<SGraphEditor> FBlueLineContextUtils::GetFocusedGraphEditor()
{
    if (!FSlateApplication::IsInitialized())
    {
        return nullptr;
    }

    TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
    if (!FocusedWidget.IsValid())
    {
        return nullptr;
    }

    TSharedPtr<SWidget> CurrentWidget = FocusedWidget;
    int32 Depth = 0;

    while (CurrentWidget.IsValid() && Depth < 50)
    {
        if (TSharedPtr<SGraphEditor> GraphEditor = TryCastGraphEditor(CurrentWidget))
        {
            return GraphEditor;
        }

        CurrentWidget = CurrentWidget->GetParentWidget();
        Depth++;
    }

    return nullptr;
}

UEdGraph* FBlueLineContextUtils::GetCurrentGraphFromFocus()
{
    UE_LOG(LogBlueLineCore, Verbose, TEXT("GetCurrentGraphFromFocus: Starting..."));
    
    // Try keyboard focus first
    TSharedPtr<SGraphPanel> GraphPanel = GetFocusedGraphPanel();
    if (GraphPanel.IsValid())
    {
        UE_LOG(LogBlueLineCore, Verbose, TEXT("GetCurrentGraphFromFocus: Found graph from SGraphPanel"));
        return GraphPanel->GetGraphObj();
    }

    // Fallback to GraphEditor
    TSharedPtr<SGraphEditor> GraphEditor = GetFocusedGraphEditor();
    if (GraphEditor.IsValid())
    {
        UE_LOG(LogBlueLineCore, Verbose, TEXT("GetCurrentGraphFromFocus: Found graph from SGraphEditor"));
        return GraphEditor->GetCurrentGraph();
    }

    // FINAL FALLBACK: Try to get from active tab/window
    // This handles cases where hotkey is pressed but focus detection fails
    UE_LOG(LogBlueLineCore, Verbose, TEXT("GetCurrentGraphFromFocus: Trying active tab fallback..."));
    UEdGraph* Graph = GetCurrentGraphFromActiveTab();
    if (Graph)
    {
        UE_LOG(LogBlueLineCore, Verbose, TEXT("GetCurrentGraphFromFocus: Found graph from active tab: %s"), *Graph->GetName());
    }
    else
    {
        UE_LOG(LogBlueLineCore, Warning, TEXT("GetCurrentGraphFromFocus: Could not find graph from any source!"));
    }
    return Graph;
}

UEdGraph* FBlueLineContextUtils::GetCurrentGraphFromActiveTab()
{
    // Try to find graph from the active editor window
    // This is useful when keyboard focus detection fails

    if (!FSlateApplication::IsInitialized())
    {
        return nullptr;
    }

    FSlateApplication& SlateApp = FSlateApplication::Get();

    // Get the active top-level window
    TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelWindow();
    if (!ActiveWindow.IsValid())
    {
        return nullptr;
    }
    
    // Search for graph widgets in the window
    TFunction<void(TSharedRef<SWidget>)> SearchForGraph;
    UEdGraph* FoundGraph = nullptr;
    
    SearchForGraph = [&](TSharedRef<SWidget> Widget)
    {
        if (FoundGraph)
        {
            return;
        }

        TSharedPtr<SWidget> WidgetPtr = Widget;
        if (TSharedPtr<SGraphPanel> GraphPanel = TryCastGraphPanel(WidgetPtr))
        {
            FoundGraph = GraphPanel->GetGraphObj();
            return;
        }

        if (TSharedPtr<SGraphEditor> GraphEditor = TryCastGraphEditor(WidgetPtr))
        {
            FoundGraph = GraphEditor->GetCurrentGraph();
            return;
        }

        // Recurse into children
        FChildren* Children = Widget->GetChildren();
        if (Children)
        {
            for (int32 i = 0; i < Children->Num(); ++i)
            {
                if (FoundGraph)
                {
                    return;
                }
                SearchForGraph(Children->GetChildAt(i));
            }
        }
    };
    
    SearchForGraph(ActiveWindow.ToSharedRef());
    return FoundGraph;
}

int32 FBlueLineContextUtils::GetSelectedNodesFromFocus(TArray<UEdGraphNode*>& OutSelectedNodes)
{
    OutSelectedNodes.Empty();

    if (!FSlateApplication::IsInitialized())
    {
        return 0;
    }

    // Try keyboard focus first
    TSharedPtr<SGraphPanel> GraphPanel = GetFocusedGraphPanel();

    // FALLBACK: If no focused panel, try to find from active tab
    if (!GraphPanel.IsValid())
    {
        if (UEdGraph* Graph = GetCurrentGraphFromActiveTab())
        {
            // We found a graph, but we need the SGraphPanel to get selection
            // Search for the graph panel that owns this graph
            FSlateApplication& SlateApp = FSlateApplication::Get();
            TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelWindow();
            if (ActiveWindow.IsValid())
            {
                TFunction<void(TSharedRef<SWidget>)> SearchForGraphPanel;
                SearchForGraphPanel = [&](TSharedRef<SWidget> Widget)
                {
                    if (GraphPanel.IsValid()) return;

                    TSharedPtr<SWidget> WidgetPtr = Widget;
                    if (TSharedPtr<SGraphPanel> Panel = TryCastGraphPanel(WidgetPtr))
                    {
                        if (Panel->GetGraphObj() == Graph)
                        {
                            GraphPanel = Panel;
                            return;
                        }
                    }

                    FChildren* Children = Widget->GetChildren();
                    if (Children)
                    {
                        for (int32 i = 0; i < Children->Num(); ++i)
                        {
                            SearchForGraphPanel(Children->GetChildAt(i));
                        }
                    }
                };
                SearchForGraphPanel(ActiveWindow.ToSharedRef());
            }
        }
    }
    
    if (!GraphPanel.IsValid())
    {
        return 0;
    }

    const FGraphPanelSelectionSet& Selection = GraphPanel->SelectionManager.GetSelectedNodes();
    for (UObject* Obj : Selection)
    {
        if (UEdGraphNode* Node = Cast<UEdGraphNode>(Obj))
        {
            OutSelectedNodes.Add(Node);
        }
    }

    return OutSelectedNodes.Num();
}

bool FBlueLineContextUtils::IsInBlueprintGraphContext()
{
    return GetCurrentGraphFromFocus() != nullptr;
}

