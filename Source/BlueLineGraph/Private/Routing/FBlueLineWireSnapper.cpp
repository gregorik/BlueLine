// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "Routing/FBlueLineWireSnapper.h"
#include "Utils/BlueLineContextUtils.h"
#include "BlueLineCore/Public/Settings/UBlueLineEditorSettings.h"
#include "Routing/FBlueLineManhattanRouter.h"
#include "Framework/Application/SlateApplication.h"
#include "SGraphPanel.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "GraphEditorDragDropAction.h"
#include "Input/DragAndDrop.h"

TSharedPtr<FBlueLineWireSnapper> FBlueLineWireSnapper::Instance = nullptr;
bool FBlueLineWireSnapper::bIsDraggingWire = false;

void FBlueLineWireSnapper::Enable()
{
    if (!Instance.IsValid() && FSlateApplication::IsInitialized())
    {
        Instance = MakeShared<FBlueLineWireSnapper>();
        FSlateApplication::Get().RegisterInputPreProcessor(Instance, 0);
    }
}

void FBlueLineWireSnapper::Disable()
{
    if (Instance.IsValid())
    {
        // Module shutdown can run after Slate teardown during editor exit.
        if (FSlateApplication::IsInitialized())
        {
            FSlateApplication::Get().UnregisterInputPreProcessor(Instance);
        }

        bIsDraggingWire = false;
        Instance.Reset();
    }
}

void FBlueLineWireSnapper::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
    const UBlueLineEditorSettings* Settings = UBlueLineEditorSettings::Get();
    if (!Settings || !Settings->bEnableBlueLine || !Settings->bEnableWireSnapping)
    {
        bHasDragOrigin = false;
        bIsDraggingWire = false;
        return;
    }

    if (!SlateApp.IsDragDropping())
    {
        bHasDragOrigin = false;
        bIsDraggingWire = false;
        return;
    }

    const TSharedPtr<FDragDropOperation> DragDropOperation = SlateApp.GetDragDroppingContent();
    const bool bIsConnectionDrag = DragDropOperation.IsValid()
        && DragDropOperation->IsOfTypeImpl(TEXT("FDragConnection"));
    bIsDraggingWire = bIsDraggingWire || bIsConnectionDrag;

    if (!bIsDraggingWire)
    {
        bHasDragOrigin = false;
        return;
    }

    TSharedPtr<SGraphPanel> GraphPanel = FBlueLineContextUtils::GetFocusedGraphPanel();
    if (!GraphPanel.IsValid()) return;

    UEdGraph* Graph = GraphPanel->GetGraphObj();
    if (!Graph) return;

    if (Graph->Nodes.Num() > 256)
    {
        return;
    }

    FVector2D CursorPos = Cursor->GetPosition();

    if (!bHasDragOrigin)
    {
        DragOriginScreen = CursorPos;
        bHasDragOrigin = true;
    }

    // Don't snap if we just started dragging (prevent snapping to the pin we drag from)
    if (FVector2D::Distance(DragOriginScreen, CursorPos) < 20.0f)
    {
        return;
    }

    FGeometry PanelGeometry = GraphPanel->GetTickSpaceGeometry();
    FVector2D LocalCursor = PanelGeometry.AbsoluteToLocal(CursorPos);
    FVector2D GraphCursor = GraphPanel->PanelCoordToGraphCoord(LocalCursor);

    // Find nearest pin
    UEdGraphPin* NearestPin = nullptr;
    float MinDistSq = FMath::Square(Settings->WireSnapRadius); 

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node) continue;
        
        // Basic culling: only check nodes within 500 units of cursor
        if (FMath::Abs(Node->NodePosX - GraphCursor.X) > 500.0f || FMath::Abs(Node->NodePosY - GraphCursor.Y) > 500.0f)
        {
            continue;
        }

        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->bHidden) continue;

            FVector2D PinGraphPos = FBlueLineManhattanRouter::GetPinPos(Pin);
            float DistSq = FVector2D::DistSquared(GraphCursor, PinGraphPos);
            
            if (DistSq < MinDistSq)
            {
                MinDistSq = DistSq;
                NearestPin = Pin;
            }
        }
    }

    if (NearestPin)
    {
        FVector2D PinGraphPos = FBlueLineManhattanRouter::GetPinPos(NearestPin);
        
        FVector2D ViewOffset = GraphPanel->GetViewOffset();
        float ZoomAmount = GraphPanel->GetZoomAmount();
        FVector2D PinLocalPos = (PinGraphPos - ViewOffset) * ZoomAmount;
        
        FVector2D PinAbsPos = PanelGeometry.LocalToAbsolute(PinLocalPos);
        
        if (FVector2D::Distance(CursorPos, PinAbsPos) > 1.0f)
        {
            Cursor->SetPosition(FMath::RoundToInt32(PinAbsPos.X), FMath::RoundToInt32(PinAbsPos.Y));
        }
    }
}

