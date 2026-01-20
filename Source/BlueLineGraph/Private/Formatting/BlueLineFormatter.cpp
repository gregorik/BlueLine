// Copyright YourTeamName. All Rights Reserved.

#include "Formatting/BlueLineFormatter.h"
#include "Settings/UBlueLineEditorSettings.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraph/EdGraph.h"
#include "ScopedTransaction.h"
#include "Framework/Application/SlateApplication.h"
#include "GraphEditor.h" 
#include "SGraphPanel.h" 

#define LOCTEXT_NAMESPACE "BlueLineFormatter"


void FBlueLineFormatter::FormatActiveGraphSelection()
{
	TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	if (!FocusedWidget.IsValid()) return;

	TSharedPtr<SGraphPanel> GraphPanel;
	TSharedPtr<SWidget> CurrentWidget = FocusedWidget;

	// Recursive find up to 50 levels
	int32 Depth = 0;
	while (CurrentWidget.IsValid() && Depth < 50)
	{
		if (CurrentWidget->GetType().ToString().Contains(TEXT("GraphPanel")))
		{
			GraphPanel = StaticCastSharedPtr<SGraphPanel>(CurrentWidget);
			break;
		}
		CurrentWidget = CurrentWidget->GetParentWidget();
		Depth++;
	}

	if (!GraphPanel.IsValid()) return;

	const FGraphPanelSelectionSet& Selection = GraphPanel->SelectionManager.GetSelectedNodes();
	if (Selection.Num() > 0)
	{
		AutoAlignSelectedNodes(Selection);
	}
}

void FBlueLineFormatter::AutoAlignSelectedNodes(const TSet<UObject*>& SelectedNodes)
{
	if (SelectedNodes.Num() < 2) return;

	FScopedTransaction Transaction(LOCTEXT("BlueLineAutoAlign", "BlueLine: Align Wires"));

	const UBlueLineEditorSettings* Settings = GetDefault<UBlueLineEditorSettings>();
	const float HorizontalSpacing = Settings ? Settings->FormatterPadding : 80.0f;

	// Separate Nodes from Comments/Other objects
	TArray<UEdGraphNode*> LayoutNodes;
	UEdGraph* GraphContext = nullptr;

	for (UObject* Obj : SelectedNodes)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(Obj))
		{
			LayoutNodes.Add(Node);
			if (!GraphContext) GraphContext = Node->GetGraph();
			Node->Modify();
		}
	}

	if (!GraphContext) return;

	// 1. Sort X Left-to-Right
	// This ensures we process parents before children (usually)
	LayoutNodes.Sort([](const UEdGraphNode& A, const UEdGraphNode& B) {
		return A.NodePosX < B.NodePosX;
		});

	bool bGraphModified = false;

	// 2. Alignment Pass
	for (UEdGraphNode* CurrentNode : LayoutNodes)
	{
		if (!CurrentNode) continue;

		UEdGraphNode* ParentToAlignTo = nullptr;
		UEdGraphPin* MyInputPin = nullptr;
		UEdGraphPin* ParentOutputPin = nullptr;

		// Find the most "Important" connection (Execution pins > Data pins)
		for (UEdGraphPin* Pin : CurrentNode->Pins)
		{
			if (Pin->Direction != EGPD_Input || Pin->LinkedTo.Num() == 0) continue;

			UEdGraphPin* LinkedPin = Pin->LinkedTo[0];
			UEdGraphNode* PossibleParent = LinkedPin->GetOwningNode();

			// Only align to parents that are to our LEFT
			// (Prevents circular logic loops flipping out)
			if (PossibleParent && PossibleParent->NodePosX < CurrentNode->NodePosX)
			{
				bool bIsExec = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);

				// Always prioritize Exec pins, or take the first data pin if we have nothing else
				if (ParentToAlignTo == nullptr || bIsExec)
				{
					ParentToAlignTo = PossibleParent;
					MyInputPin = Pin;
					ParentOutputPin = LinkedPin;

					if (bIsExec) break; // Found the best one, stop looking
				}
			}
		}

		// Check if we found a valid connection
		if (ParentToAlignTo && MyInputPin && ParentOutputPin)
		{
			// --- AGGRESSIVE ALIGNMENT ---

			// 1. X Alignment (Spacing)
			// Ensure we are at least [Spacing] px away from parent's right edge
			// We estimate node width (approx 150-200) since we don't have widget geometry here.
			int32 EstimatedParentWidth = 150;
			// Check if NodeWidth is stored (sometimes valid for comments/custom nodes)
			if (ParentToAlignTo->NodeWidth > 0) EstimatedParentWidth = ParentToAlignTo->NodeWidth;

			int32 TargetX = ParentToAlignTo->NodePosX + EstimatedParentWidth + (int32)HorizontalSpacing;

			// Only push forward, never pull back (prevents crushing)
			if (CurrentNode->NodePosX < TargetX)
			{
				CurrentNode->NodePosX = TargetX;
				bGraphModified = true;
			}

			// 2. Y Alignment (Straight Wires)
			// Goal: MyInputPin WorldY == ParentOutputPin WorldY
			// Problem: We don't know pin offsets exactly.
			// Heuristic: Exec pins are usually top (~20px offset). Data pins vary.
			//
			// Simple Align: Match Top of Nodes.
			int32 TargetY = ParentToAlignTo->NodePosY;

			// Refined Align: If both are Exec, keep the tops aligned. 
			// If connecting data-to-data, we might want to center? 
			// Let's stick to Top Alignment as it's cleaner for Blueprint "Chains".

			CurrentNode->NodePosY = TargetY;
			bGraphModified = true;
		}

		// Snap resulting position to grid
		CurrentNode->SnapToGrid(16);
	}

	// 3. Collision Pass (Simple Vertical Separtion)
	// If two nodes are overlapping, push the right-most (or bottom-most) one down.
	// We iterate multiple times to resolve cascades.
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (int32 i = 0; i < LayoutNodes.Num(); ++i)
		{
			for (int32 j = i + 1; j < LayoutNodes.Num(); ++j)
			{
				UEdGraphNode* A = LayoutNodes[i];
				UEdGraphNode* B = LayoutNodes[j];

				// Simple AABB overlap check
				// Since we don't have exact heights, interpret a standard height of 100 units
				int32 PaddingX = 20;
				int32 PaddingY = 20;
				int32 Width = 200;
				int32 Height = 100;

				bool bOverlapX = FMath::Abs(A->NodePosX - B->NodePosX) < (Width + PaddingX);
				bool bOverlapY = FMath::Abs(A->NodePosY - B->NodePosY) < (Height + PaddingY);

				if (bOverlapX && bOverlapY)
				{
					// Collision! Push the lower/further one down.
					UEdGraphNode* NodeToMove = (A->NodePosY >= B->NodePosY) ? A : B;
					UEdGraphNode* StaticNode = (NodeToMove == A) ? B : A;

					NodeToMove->NodePosY = StaticNode->NodePosY + Height + PaddingY;
					NodeToMove->SnapToGrid(16);
					bGraphModified = true;
				}
			}
		}
	}

	if (bGraphModified)
	{
		GraphContext->NotifyGraphChanged();
	}
}

UEdGraphNode* FBlueLineFormatter::FindAnchorNode(const TArray<UEdGraphNode*>& SortedNodes)
{
	return (SortedNodes.Num() > 0) ? SortedNodes[0] : nullptr;
}

#undef LOCTEXT_NAMESPACE