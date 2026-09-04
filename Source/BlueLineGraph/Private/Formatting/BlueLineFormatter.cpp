// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "Formatting/BlueLineFormatter.h"
#include "BlueLineCore/Public/Settings/UBlueLineEditorSettings.h"
#include "Utils/BlueLineContextUtils.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraph.h"
#include "ScopedTransaction.h"
#include "GraphEditor.h" 
#include "SGraphPanel.h" 
#include "Routing/FBlueLineManhattanRouter.h"
#include "EdGraphNode_Comment.h"
#include "K2Node_Knot.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "BlueLineFormatter"

namespace
{
	constexpr int32 MinLayoutNodeWidth = 200;
	constexpr int32 MinLayoutNodeHeight = 100;

	struct FBlueLineAlignmentPlan
	{
		UEdGraphNode* Node = nullptr;
		UEdGraphNode* ParentNode = nullptr;
		UEdGraphPin* InputPin = nullptr;
		UEdGraphPin* ParentOutputPin = nullptr;
		int32 OriginalNodePosY = 0;
		int32 SiblingIndex = 0;
		int32 SiblingCount = 1;
		bool bIsExecConnection = false;
	};

	int32 GetEffectiveNodeWidth(const UEdGraphNode* Node)
	{
		return FMath::Max(Node ? Node->NodeWidth : 0, MinLayoutNodeWidth);
	}

	int32 GetEffectiveNodeHeight(const UEdGraphNode* Node)
	{
		return FMath::Max(Node ? Node->NodeHeight : 0, MinLayoutNodeHeight);
	}

	void ShowFormatterNotification(const FText& Message, SNotificationItem::ECompletionState CompletionState)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}

		FNotificationInfo Info(Message);
		Info.bFireAndForget = true;
		Info.bUseLargeFont = false;
		Info.ExpireDuration = 2.5f;
		Info.FadeOutDuration = 0.2f;

		if (TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Notification->SetCompletionState(CompletionState);
		}
	}
}

void FBlueLineFormatter::FormatActiveGraphSelection()
{
	TArray<UEdGraphNode*> SelectedNodes;
	const int32 SelectedCount = FBlueLineContextUtils::GetSelectedNodesFromFocus(SelectedNodes);
	if (SelectedCount == 0)
	{
		ShowFormatterNotification(
			LOCTEXT("FormatterNoGraph", "BlueLine: no active Blueprint graph selection."),
			SNotificationItem::CS_Fail);
		return;
	}

	if (SelectedCount < 2)
	{
		ShowFormatterNotification(
			LOCTEXT("FormatterNeedMoreNodes", "BlueLine: select at least two nodes to arrange them."),
			SNotificationItem::CS_Pending);
		return;
	}

	TSet<UObject*> Selection;
	for (UEdGraphNode* Node : SelectedNodes)
	{
		Selection.Add(Node);
	}

	AutoAlignSelectedNodes(Selection);

	ShowFormatterNotification(
		FText::Format(
			LOCTEXT("FormatterAligned", "BlueLine: arranged {0} selected nodes."),
			FText::AsNumber(SelectedCount)),
		SNotificationItem::CS_Success);
}

void FBlueLineFormatter::AutoAlignSelectedNodes(const TSet<UObject*>& SelectedNodes)
{
	if (SelectedNodes.Num() < 2)
	{
		return;
	}

	const UBlueLineEditorSettings* Settings = GetDefault<UBlueLineEditorSettings>();
	
	// Check if auto-format is enabled
	if (!Settings || !Settings->bEnableAutoFormat)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("BlueLineAutoAlign", "BlueLine: Align Wires"));
	
	const float HorizontalSpacing = Settings ? Settings->HorizontalSpacing : 300.0f;
	const float VerticalSpacing = Settings ? Settings->VerticalSpacing : 120.0f;
	const EBlueLineFormatStrategy FormatStrategy = Settings ? Settings->FormatStrategy : EBlueLineFormatStrategy::Flow;
	const int32 GridSnapSize = Settings ? Settings->GridSnapSize : 16;
	const int32 CollisionPadding = Settings ? Settings->CollisionPadding : 20;

	// Separate Nodes from Comments/Other objects
	TArray<UEdGraphNode*> LayoutNodes;
	TSet<UEdGraphNode*> LayoutNodeSet;
	UEdGraph* GraphContext = nullptr;

	for (UObject* Obj : SelectedNodes)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(Obj))
		{
			if (Node->IsA<UEdGraphNode_Comment>())
			{
				continue;
			}

			LayoutNodes.Add(Node);
			LayoutNodeSet.Add(Node);
			if (!GraphContext) GraphContext = Node->GetGraph();
			Node->Modify();
		}
	}

	if (!GraphContext || LayoutNodes.Num() < 2)
	{
		return;
	}

	GraphContext->Modify();

	// Sort left-to-right so parents get placed before their downstream children.
	LayoutNodes.Sort([](const UEdGraphNode& A, const UEdGraphNode& B) {
		return (A.NodePosX == B.NodePosX) ? (A.NodePosY < B.NodePosY) : (A.NodePosX < B.NodePosX);
	});

	TMap<UEdGraphNode*, FBlueLineAlignmentPlan> AlignmentPlans;
	TMap<UEdGraphNode*, TArray<UEdGraphNode*>> ChildrenByParent;
	bool bGraphModified = false;

	// Build the parent/child alignment plan using the best selected incoming connection.
	for (UEdGraphNode* CurrentNode : LayoutNodes)
	{
		if (!CurrentNode || CurrentNode->IsA<UK2Node_Knot>())
		{
			continue;
		}

		UEdGraphNode* ParentToAlignTo = nullptr;
		UEdGraphPin* MyInputPin = nullptr;
		UEdGraphPin* ParentOutputPin = nullptr;
		bool bBestIsExec = false;
		float BestHorizontalDelta = TNumericLimits<float>::Max();
		float BestVerticalDelta = TNumericLimits<float>::Max();

		for (UEdGraphPin* Pin : CurrentNode->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || Pin->LinkedTo.Num() == 0)
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!LinkedPin)
				{
					continue;
				}

				UEdGraphNode* PossibleParent = LinkedPin->GetOwningNode();
				if (!PossibleParent || !LayoutNodeSet.Contains(PossibleParent) || PossibleParent == CurrentNode)
				{
					continue;
				}

				// Check if CurrentNode is already an ancestor of PossibleParent to prevent directed cycles
				auto IsAncestor = [&](UEdGraphNode* PotentialAncestor, UEdGraphNode* Descendant) -> bool {
					TSet<UEdGraphNode*> Visited;
					TArray<UEdGraphNode*> Stack;
					Stack.Add(Descendant);
					while (Stack.Num() > 0)
					{
						UEdGraphNode* Curr = Stack.Pop();
						if (Curr == PotentialAncestor) return true;
						if (Visited.Contains(Curr)) continue;
						Visited.Add(Curr);
						if (const FBlueLineAlignmentPlan* P = AlignmentPlans.Find(Curr))
						{
							if (P->ParentNode) Stack.Add(P->ParentNode);
						}
					}
					return false;
				};

				if (IsAncestor(CurrentNode, PossibleParent))
				{
					continue;
				}

				const bool bIsExec = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
				const float HorizontalDelta = (float)(CurrentNode->NodePosX - PossibleParent->NodePosX);
				const float VerticalDelta = FMath::Abs((float)(CurrentNode->NodePosY - PossibleParent->NodePosY));

				const bool bShouldReplace =
					ParentToAlignTo == nullptr ||
					(bIsExec && !bBestIsExec) ||
					(bIsExec == bBestIsExec && HorizontalDelta < BestHorizontalDelta) ||
					(bIsExec == bBestIsExec &&
					 FMath::IsNearlyEqual(HorizontalDelta, BestHorizontalDelta) &&
					 VerticalDelta < BestVerticalDelta);

				if (bShouldReplace)
				{
					ParentToAlignTo = PossibleParent;
					MyInputPin = Pin;
					ParentOutputPin = LinkedPin;
					bBestIsExec = bIsExec;
					BestHorizontalDelta = HorizontalDelta;
					BestVerticalDelta = VerticalDelta;
				}
			}
		}

		if (ParentToAlignTo && MyInputPin && ParentOutputPin)
		{
			FBlueLineAlignmentPlan& Plan = AlignmentPlans.FindOrAdd(CurrentNode);
			Plan.Node = CurrentNode;
			Plan.ParentNode = ParentToAlignTo;
			Plan.InputPin = MyInputPin;
			Plan.ParentOutputPin = ParentOutputPin;
			Plan.OriginalNodePosY = CurrentNode->NodePosY;
			Plan.bIsExecConnection = bBestIsExec;

			ChildrenByParent.FindOrAdd(ParentToAlignTo).Add(CurrentNode);
		}
	}

	for (TPair<UEdGraphNode*, TArray<UEdGraphNode*>>& Pair : ChildrenByParent)
	{
		TArray<UEdGraphNode*>& Children = Pair.Value;
		Children.Sort([&AlignmentPlans](const UEdGraphNode& A, const UEdGraphNode& B)
		{
			const FBlueLineAlignmentPlan* PlanA = AlignmentPlans.Find(const_cast<UEdGraphNode*>(&A));
			const FBlueLineAlignmentPlan* PlanB = AlignmentPlans.Find(const_cast<UEdGraphNode*>(&B));
			const int32 YA = PlanA ? PlanA->OriginalNodePosY : A.NodePosY;
			const int32 YB = PlanB ? PlanB->OriginalNodePosY : B.NodePosY;
			return (YA == YB) ? (A.NodePosX < B.NodePosX) : (YA < YB);
		});

		for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
		{
			if (FBlueLineAlignmentPlan* Plan = AlignmentPlans.Find(Children[ChildIndex]))
			{
				Plan->SiblingIndex = ChildIndex;
				Plan->SiblingCount = Children.Num();
			}
		}
	}

	for (UEdGraphNode* CurrentNode : LayoutNodes)
	{
		if (!CurrentNode)
		{
			continue;
		}

		const FBlueLineAlignmentPlan* Plan = AlignmentPlans.Find(CurrentNode);
		if (!Plan || !Plan->ParentNode || !Plan->InputPin || !Plan->ParentOutputPin)
		{
			CurrentNode->SnapToGrid(GridSnapSize);
			continue;
		}

		if (FormatStrategy == EBlueLineFormatStrategy::Minimal)
		{
			CurrentNode->SnapToGrid(GridSnapSize);
			continue;
		}

		const FVector2D ParentPinPos = FBlueLineManhattanRouter::GetPinPos(Plan->ParentOutputPin);
		const FVector2D InputPinPos = FBlueLineManhattanRouter::GetPinPos(Plan->InputPin);
		const float InputPinOffsetY = InputPinPos.Y - (float)CurrentNode->NodePosY;

		int32 TargetX = Plan->ParentNode->NodePosX + GetEffectiveNodeWidth(Plan->ParentNode) + FMath::RoundToInt(HorizontalSpacing);
		int32 TargetY = FMath::RoundToInt(ParentPinPos.Y - InputPinOffsetY);

		if (FormatStrategy == EBlueLineFormatStrategy::Columns)
		{
			TargetY = Plan->ParentNode->NodePosY + FMath::RoundToInt((Plan->SiblingIndex + 1) * VerticalSpacing);
		}
		else if (Plan->SiblingCount > 1)
		{
			if (Plan->bIsExecConnection)
			{
				// Keep primary execution spine straight (SiblingIndex 0 stays at TargetY)
				// Secondary execution branches offset downward
				TargetY += FMath::RoundToInt(Plan->SiblingIndex * VerticalSpacing);
			}
			else
			{
				const float CenterIndex = (Plan->SiblingCount - 1) * 0.5f;
				const float OffsetSlots = (float)Plan->SiblingIndex - CenterIndex;
				TargetY += FMath::RoundToInt(OffsetSlots * VerticalSpacing);
			}
		}

		if (CurrentNode->NodePosX != TargetX || CurrentNode->NodePosY != TargetY)
		{
			CurrentNode->NodePosX = TargetX;
			CurrentNode->NodePosY = TargetY;
			bGraphModified = true;
		}

		CurrentNode->SnapToGrid(GridSnapSize);
	}

	if (Settings ? Settings->bPreventNodeOverlap : true)
	{
		for (int32 Pass = 0; Pass < LayoutNodes.Num(); ++Pass)
		{
			bool bPassModified = false;

			for (int32 i = 0; i < LayoutNodes.Num(); ++i)
			{
				for (int32 j = i + 1; j < LayoutNodes.Num(); ++j)
				{
					UEdGraphNode* A = LayoutNodes[i];
					UEdGraphNode* B = LayoutNodes[j];
					if (!A || !B)
					{
						continue;
					}

					const int32 AWidth = GetEffectiveNodeWidth(A);
					const int32 AHeight = GetEffectiveNodeHeight(A);
					const int32 BWidth = GetEffectiveNodeWidth(B);
					const int32 BHeight = GetEffectiveNodeHeight(B);

					const bool bOverlapX =
						A->NodePosX < (B->NodePosX + BWidth + CollisionPadding) &&
						B->NodePosX < (A->NodePosX + AWidth + CollisionPadding);
					const bool bOverlapY =
						A->NodePosY < (B->NodePosY + BHeight + CollisionPadding) &&
						B->NodePosY < (A->NodePosY + AHeight + CollisionPadding);

					if (bOverlapX && bOverlapY)
					{
						UEdGraphNode* NodeToMove =
							(A->NodePosX > B->NodePosX || (A->NodePosX == B->NodePosX && A->NodePosY >= B->NodePosY)) ? A : B;
						UEdGraphNode* StaticNode = (NodeToMove == A) ? B : A;

						NodeToMove->NodePosY = StaticNode->NodePosY + GetEffectiveNodeHeight(StaticNode) + CollisionPadding;
						NodeToMove->SnapToGrid(GridSnapSize);
						bPassModified = true;
						bGraphModified = true;
					}
				}
			}

			if (!bPassModified)
			{
				break;
			}
		}
	}

	// Magnetic alignment: snap nodes that ended up within MagnetEvaluationDistance of a
	// perfectly straight connection to their parent onto that straight line, but only when
	// the snap does not reintroduce an overlap. Runs after collision resolution so it also
	// cleans up small misalignments that collision handling introduced.
	const float MagnetDistance = Settings ? Settings->MagnetEvaluationDistance : 100.0f;
	for (UEdGraphNode* CurrentNode : LayoutNodes)
	{
		const FBlueLineAlignmentPlan* Plan = CurrentNode ? AlignmentPlans.Find(CurrentNode) : nullptr;
		if (!Plan || !Plan->ParentOutputPin || !Plan->InputPin)
		{
			continue;
		}

		const FVector2D ParentPinPos = FBlueLineManhattanRouter::GetPinPos(Plan->ParentOutputPin);
		const FVector2D InputPinPos = FBlueLineManhattanRouter::GetPinPos(Plan->InputPin);
		const int32 InputPinOffsetY = FMath::RoundToInt(InputPinPos.Y - (float)CurrentNode->NodePosY);
		const int32 StraightY = FMath::RoundToInt(ParentPinPos.Y) - InputPinOffsetY;

		if (FMath::Abs(StraightY - CurrentNode->NodePosY) >= MagnetDistance)
		{
			continue;
		}

		const int32 PreviousY = CurrentNode->NodePosY;
		CurrentNode->NodePosY = StraightY;
		CurrentNode->SnapToGrid(GridSnapSize);

		bool bCausesOverlap = false;
		if (Settings ? Settings->bPreventNodeOverlap : true)
		{
			for (UEdGraphNode* Other : LayoutNodes)
			{
				if (!Other || Other == CurrentNode)
				{
					continue;
				}

				const bool bOverlapX =
					CurrentNode->NodePosX < (Other->NodePosX + GetEffectiveNodeWidth(Other) + CollisionPadding) &&
					Other->NodePosX < (CurrentNode->NodePosX + GetEffectiveNodeWidth(CurrentNode) + CollisionPadding);
				const bool bOverlapY =
					CurrentNode->NodePosY < (Other->NodePosY + GetEffectiveNodeHeight(Other) + CollisionPadding) &&
					Other->NodePosY < (CurrentNode->NodePosY + GetEffectiveNodeHeight(CurrentNode) + CollisionPadding);

				if (bOverlapX && bOverlapY)
				{
					bCausesOverlap = true;
					break;
				}
			}
		}

		if (bCausesOverlap)
		{
			CurrentNode->NodePosY = PreviousY;
			CurrentNode->SnapToGrid(GridSnapSize);
		}
		else if (CurrentNode->NodePosY != PreviousY)
		{
			bGraphModified = true;
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
