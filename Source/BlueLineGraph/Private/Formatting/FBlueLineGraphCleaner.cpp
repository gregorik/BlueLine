// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "Formatting/FBlueLineGraphCleaner.h"
#include "BlueLineLog.h"
#include "Routing/FBlueLineManhattanRouter.h"
#include "Analysis/FBlueLineGraphAnalyzer.h"  // Now in BlueLineCore
#include "BlueLineCore/Public/Settings/UBlueLineEditorSettings.h"
#include "Utils/BlueLineContextUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "ScopedTransaction.h"
#include "K2Node_Knot.h"
#include "Misc/DateTime.h"  // For deterministic RNG seeding
#include "EdGraphSchema_K2.h"

#define LOCTEXT_NAMESPACE "BlueLineGraphCleaner"

namespace
{
	constexpr int32 MinNodeWidthEstimate = 200;
	constexpr int32 MinNodeHeightEstimate = 90;

	bool ShouldCleanLayoutNode(const UEdGraphNode* Node)
	{
		return Node && !Node->IsA<UEdGraphNode_Comment>() && !Node->IsA<UK2Node_Knot>();
	}

	bool IsExecPin(const UEdGraphPin* Pin)
	{
		return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
	}

	bool IsExecNode(const UEdGraphNode* Node)
	{
		if (!Node) return false;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (IsExecPin(Pin))
			{
				return true;
			}
		}
		return false;
	}

	int32 GetNodeEffectiveWidth(const UEdGraphNode* Node)
	{
		return FMath::Max(Node ? Node->NodeWidth : 0, MinNodeWidthEstimate);
	}

	int32 GetNodeEffectiveHeight(const UEdGraphNode* Node)
	{
		return FMath::Max(Node ? Node->NodeHeight : 0, MinNodeHeightEstimate);
	}

	void CollectNeighborsThroughKnots(
		UEdGraphPin* Pin,
		TArray<UEdGraphNode*>& OutNeighbors,
		TSet<UK2Node_Knot*>& OutTraversedKnots,
		TSet<UEdGraphPin*>& VisitedPins)
	{
		if (!Pin || VisitedPins.Contains(Pin)) return;
		VisitedPins.Add(Pin);

		for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			if (!LinkedPin) continue;
			UEdGraphNode* OwningNode = LinkedPin->GetOwningNode();
			if (!OwningNode) continue;

			if (UK2Node_Knot* Knot = Cast<UK2Node_Knot>(OwningNode))
			{
				OutTraversedKnots.Add(Knot);
				for (UEdGraphPin* KnotPin : Knot->Pins)
				{
					if (KnotPin && KnotPin->Direction != LinkedPin->Direction)
					{
						CollectNeighborsThroughKnots(KnotPin, OutNeighbors, OutTraversedKnots, VisitedPins);
					}
				}
			}
			else if (ShouldCleanLayoutNode(OwningNode))
			{
				OutNeighbors.AddUnique(OwningNode);
			}
		}
	}
}

void FBlueLineGraphCleaner::CleanActiveGraph()
{
    UEdGraph* ActiveGraph = GetActiveGraph();
    if (ActiveGraph)
    {
        CleanGraph(ActiveGraph);
    }
}

void FBlueLineGraphCleaner::CleanGraph(UEdGraph* Graph)
{
	if (!Graph) return;

	const UBlueLineEditorSettings* Settings = GetDefault<UBlueLineEditorSettings>();
	if (!Settings || !Settings->bEnableBlueLine)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("CleanGraphTrans", "BlueLine: Clean Graph"));
	Graph->Modify();

	// Clean up dead reroutes first
	FBlueLineManhattanRouter::CleanupOrphanedRerouteNodes(Graph);

	FBlueLineGraphAnalyzer::FAnalysisResult Analysis = FBlueLineGraphAnalyzer::AnalyzeGraph(Graph);

	// Snapshot original comment memberships before any node moves
	struct FCommentSnapshot
	{
		UEdGraphNode_Comment* CommentNode = nullptr;
		TArray<UEdGraphNode*> EnclosedNodes;
	};
	TArray<FCommentSnapshot> CommentSnapshots;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node))
		{
			FCommentSnapshot Snapshot;
			Snapshot.CommentNode = Comment;
			const FSlateRect CommentRect(
				(float)Comment->NodePosX,
				(float)Comment->NodePosY,
				(float)(Comment->NodePosX + Comment->NodeWidth),
				(float)(Comment->NodePosY + Comment->NodeHeight)
			);

			for (UEdGraphNode* InnerNode : Graph->Nodes)
			{
				if (ShouldCleanLayoutNode(InnerNode))
				{
					const float InnerW = (float)GetNodeEffectiveWidth(InnerNode);
					const float InnerH = (float)GetNodeEffectiveHeight(InnerNode);
					if (InnerNode->NodePosX >= CommentRect.Left &&
						(InnerNode->NodePosX + InnerW) <= CommentRect.Right &&
						InnerNode->NodePosY >= CommentRect.Top &&
						(InnerNode->NodePosY + InnerH) <= CommentRect.Bottom)
					{
						Snapshot.EnclosedNodes.Add(InnerNode);
					}
				}
			}

			if (Snapshot.EnclosedNodes.Num() > 0)
			{
				CommentSnapshots.Add(Snapshot);
			}
		}
	}

	// 1. Identify Connected Components (Islands) traversing through knots
	struct FIslandData
	{
		TArray<UEdGraphNode*> Nodes;
		TSet<UK2Node_Knot*> Knots;
		int32 OriginalMinX = 0;
		int32 OriginalMinY = 0;
	};

	TArray<FIslandData> Islands;
	TSet<UEdGraphNode*> ProcessedNodes;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!ShouldCleanLayoutNode(Node) || ProcessedNodes.Contains(Node)) continue;

		FIslandData Island;
		TArray<UEdGraphNode*> Stack;
		Stack.Push(Node);

		int32 MinX = Node->NodePosX;
		int32 MinY = Node->NodePosY;

		while (Stack.Num() > 0)
		{
			UEdGraphNode* Current = Stack.Pop();
			if (!ShouldCleanLayoutNode(Current) || ProcessedNodes.Contains(Current)) continue;

			Island.Nodes.Add(Current);
			ProcessedNodes.Add(Current);

			MinX = FMath::Min(MinX, Current->NodePosX);
			MinY = FMath::Min(MinY, Current->NodePosY);

			for (UEdGraphPin* Pin : Current->Pins)
			{
				if (!Pin) continue;
				TArray<UEdGraphNode*> Neighbors;
				TSet<UEdGraphPin*> VisitedPins;
				CollectNeighborsThroughKnots(Pin, Neighbors, Island.Knots, VisitedPins);

				for (UEdGraphNode* Neighbor : Neighbors)
				{
					if (ShouldCleanLayoutNode(Neighbor) && !ProcessedNodes.Contains(Neighbor))
					{
						Stack.Push(Neighbor);
					}
				}
			}
		}

		Island.OriginalMinX = MinX;
		Island.OriginalMinY = MinY;
		Islands.Add(Island);
	}

	const float HorizontalSpacing = Settings ? Settings->HorizontalSpacing : 300.0f;
	const float VerticalSpacing = Settings ? Settings->VerticalSpacing : 120.0f;
	const int32 GridSnapSize = Settings ? Settings->GridSnapSize : 16;
	const bool bSkipGA = Analysis.TotalNodes > 500;

	// Track occupied island bounding boxes to prevent overlapping multiple islands
	TArray<FBox2D> PlacedIslandBounds;

	// 2. Process each Island
	for (FIslandData& Island : Islands)
	{
		if (Island.Nodes.Num() == 0) continue;

		TSet<UEdGraphNode*> IslandNodeSet(Island.Nodes);

		// Separate Execution and Pure nodes
		TArray<UEdGraphNode*> ExecRoots;
		TArray<UEdGraphNode*> PureRoots;

		for (UEdGraphNode* Node : Island.Nodes)
		{
			if (IsExecNode(Node))
			{
				bool bHasIncomingExec = false;
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (IsExecPin(Pin) && Pin->Direction == EGPD_Input)
					{
						TArray<UEdGraphNode*> UpstreamExecNodes;
						TSet<UK2Node_Knot*> UnusedKnots;
						TSet<UEdGraphPin*> VisitedPins;
						CollectNeighborsThroughKnots(Pin, UpstreamExecNodes, UnusedKnots, VisitedPins);

						for (UEdGraphNode* Upstream : UpstreamExecNodes)
						{
							if (IslandNodeSet.Contains(Upstream))
							{
								bHasIncomingExec = true;
								break;
							}
						}
						if (bHasIncomingExec) break;
					}
				}

				if (!bHasIncomingExec)
				{
					ExecRoots.Add(Node);
				}
			}
			else
			{
				// Pure node: check if it has any incoming data connections within the island
				bool bHasIncomingData = false;
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Input)
					{
						TArray<UEdGraphNode*> UpstreamDataNodes;
						TSet<UK2Node_Knot*> UnusedKnots;
						TSet<UEdGraphPin*> VisitedPins;
						CollectNeighborsThroughKnots(Pin, UpstreamDataNodes, UnusedKnots, VisitedPins);

						for (UEdGraphNode* Upstream : UpstreamDataNodes)
						{
							if (IslandNodeSet.Contains(Upstream))
							{
								bHasIncomingData = true;
								break;
							}
						}
						if (bHasIncomingData) break;
					}
				}

				if (!bHasIncomingData)
				{
					PureRoots.Add(Node);
				}
			}
		}

		// Fallback root selection
		if (ExecRoots.Num() == 0 && PureRoots.Num() == 0)
		{
			ExecRoots.Add(Island.Nodes[0]);
		}

		// --- TWO-PHASE TOPOLOGICAL RANKING ---
		TMap<UEdGraphNode*, int32> NodeRanks;

		// Phase 1: Rank the Execution Spine
		TArray<UEdGraphNode*> ExecQueue;
		for (UEdGraphNode* ExecRoot : ExecRoots)
		{
			NodeRanks.Add(ExecRoot, 0);
			ExecQueue.Add(ExecRoot);
		}

		TSet<UEdGraphNode*> VisitedExec;
		while (ExecQueue.Num() > 0)
		{
			UEdGraphNode* Current = ExecQueue[0];
			ExecQueue.RemoveAt(0);
			VisitedExec.Add(Current);
			const int32 CurrentRank = NodeRanks[Current];

			for (UEdGraphPin* Pin : Current->Pins)
			{
				if (IsExecPin(Pin) && Pin->Direction == EGPD_Output)
				{
					TArray<UEdGraphNode*> DownstreamNodes;
					TSet<UK2Node_Knot*> UnusedKnots;
					TSet<UEdGraphPin*> VisitedPins;
					CollectNeighborsThroughKnots(Pin, DownstreamNodes, UnusedKnots, VisitedPins);

					for (UEdGraphNode* Downstream : DownstreamNodes)
					{
						if (!IslandNodeSet.Contains(Downstream)) continue;

						int32& DownstreamRank = NodeRanks.FindOrAdd(Downstream, CurrentRank + 1);
						DownstreamRank = FMath::Max(DownstreamRank, CurrentRank + 1);

						if (!VisitedExec.Contains(Downstream))
						{
							ExecQueue.AddUnique(Downstream);
						}
					}
				}
			}
		}

		// Phase 2: Rank Pure Data Nodes (Reverse Dependency Flow)
		// Pure nodes sit immediately to the left of the node that consumes their outputs.
		TArray<UEdGraphNode*> PureNodesToRank;
		for (UEdGraphNode* Node : Island.Nodes)
		{
			if (!IsExecNode(Node) || !NodeRanks.Contains(Node))
			{
				PureNodesToRank.Add(Node);
			}
		}

		int32 MaxPasses = PureNodesToRank.Num() + 5;
		bool bChanged = true;
		while (bChanged && MaxPasses-- > 0)
		{
			bChanged = false;
			for (UEdGraphNode* PureNode : PureNodesToRank)
			{
				int32 MinConsumerRank = TNumericLimits<int32>::Max();

				for (UEdGraphPin* Pin : PureNode->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Output)
					{
						TArray<UEdGraphNode*> Consumers;
						TSet<UK2Node_Knot*> UnusedKnots;
						TSet<UEdGraphPin*> VisitedPins;
						CollectNeighborsThroughKnots(Pin, Consumers, UnusedKnots, VisitedPins);

						for (UEdGraphNode* Consumer : Consumers)
						{
							if (const int32* CRank = NodeRanks.Find(Consumer))
							{
								MinConsumerRank = FMath::Min(MinConsumerRank, *CRank);
							}
						}
					}
				}

				if (MinConsumerRank != TNumericLimits<int32>::Max())
				{
					const int32 DesiredRank = MinConsumerRank - 1;
					int32* ExistingRank = NodeRanks.Find(PureNode);
					if (!ExistingRank || *ExistingRank != DesiredRank)
					{
						NodeRanks.Add(PureNode, DesiredRank);
						bChanged = true;
					}
				}
				else if (!NodeRanks.Contains(PureNode))
				{
					int32 MaxProducerRank = -1;
					for (UEdGraphPin* Pin : PureNode->Pins)
					{
						if (Pin && Pin->Direction == EGPD_Input)
						{
							TArray<UEdGraphNode*> Producers;
							TSet<UK2Node_Knot*> UnusedKnots;
							TSet<UEdGraphPin*> VisitedPins;
							CollectNeighborsThroughKnots(Pin, Producers, UnusedKnots, VisitedPins);

							for (UEdGraphNode* Producer : Producers)
							{
								if (const int32* PRank = NodeRanks.Find(Producer))
								{
									MaxProducerRank = FMath::Max(MaxProducerRank, *PRank);
								}
							}
						}
					}

					if (MaxProducerRank != -1)
					{
						NodeRanks.Add(PureNode, MaxProducerRank + 1);
						bChanged = true;
					}
					else if (PureRoots.Contains(PureNode))
					{
						NodeRanks.Add(PureNode, 0);
						bChanged = true;
					}
				}
			}
		}

		// Ensure all nodes in the island have a rank
		for (UEdGraphNode* Node : Island.Nodes)
		{
			if (!NodeRanks.Contains(Node))
			{
				NodeRanks.Add(Node, 0);
			}
		}

		// Normalize ranks so minimum rank is 0
		int32 MinRankValue = 0;
		for (const auto& Pair : NodeRanks)
		{
			MinRankValue = FMath::Min(MinRankValue, Pair.Value);
		}

		if (MinRankValue < 0)
		{
			const int32 Offset = -MinRankValue;
			for (auto& Pair : NodeRanks)
			{
				Pair.Value += Offset;
			}
		}

		// Group nodes by Rank
		TMap<int32, TArray<UEdGraphNode*>> RankGroups;
		for (UEdGraphNode* Node : Island.Nodes)
		{
			RankGroups.FindOrAdd(NodeRanks[Node]).Add(Node);
		}

		// Reorder within ranks using Evolutionary Minimizer (preserving execution hierarchy)
		if (!bSkipGA && Island.Nodes.Num() > 2)
		{
			EvolutionaryCrossingMinimizer(RankGroups, Graph);
		}

		// Calculate Island Target Origin (Preserve relative canvas space, avoid smashing to 0,0)
		int32 TargetOriginX = Island.OriginalMinX;
		int32 TargetOriginY = Island.OriginalMinY;

		TArray<int32> SortedRanks;
		RankGroups.GetKeys(SortedRanks);
		SortedRanks.Sort();

		// Calculate relative positions within the island
		TMap<UEdGraphNode*, FIntPoint> NodeLocalPositions;
		int32 IslandHeight = 0;

		for (int32 Rank : SortedRanks)
		{
			TArray<UEdGraphNode*>& NodesInRank = RankGroups[Rank];
			const int32 ColumnX = FMath::RoundToInt(Rank * HorizontalSpacing);
			int32 CurrentY = 0;

			for (int32 i = 0; i < NodesInRank.Num(); ++i)
			{
				UEdGraphNode* Node = NodesInRank[i];
				int32 TargetY = CurrentY;

				// Execution pin horizontal alignment
				bool bAligned = false;
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin && IsExecPin(Pin) && Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() > 0)
					{
						TArray<UEdGraphNode*> UpstreamNodes;
						TSet<UK2Node_Knot*> UnusedKnots;
						TSet<UEdGraphPin*> VisitedPins;
						CollectNeighborsThroughKnots(Pin, UpstreamNodes, UnusedKnots, VisitedPins);

						for (UEdGraphNode* UpstreamNode : UpstreamNodes)
						{
							if (const FIntPoint* UpstreamPos = NodeLocalPositions.Find(UpstreamNode))
							{
								const FVector2D UpstreamPinPos = FBlueLineManhattanRouter::GetPinPos(Pin->LinkedTo[0]);
								const FVector2D MyPinPos = FBlueLineManhattanRouter::GetPinPos(Pin);
								const int32 PinOffsetY = FMath::RoundToInt(MyPinPos.Y - (float)Node->NodePosY);
								const int32 UpstreamPinOffsetY = FMath::RoundToInt(UpstreamPinPos.Y - (float)UpstreamNode->NodePosY);

								TargetY = UpstreamPos->Y + UpstreamPinOffsetY - PinOffsetY;
								bAligned = true;
								break;
							}
						}
						if (bAligned) break;
					}
				}

				// Data pin alignment if not exec-aligned
				if (!bAligned)
				{
					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin && Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0)
						{
							TArray<UEdGraphNode*> ConsumerNodes;
							TSet<UK2Node_Knot*> UnusedKnots;
							TSet<UEdGraphPin*> VisitedPins;
							CollectNeighborsThroughKnots(Pin, ConsumerNodes, UnusedKnots, VisitedPins);

							for (UEdGraphNode* Consumer : ConsumerNodes)
							{
								if (const FIntPoint* ConsumerPos = NodeLocalPositions.Find(Consumer))
								{
									const FVector2D ConsumerPinPos = FBlueLineManhattanRouter::GetPinPos(Pin->LinkedTo[0]);
									const FVector2D MyPinPos = FBlueLineManhattanRouter::GetPinPos(Pin);
									const int32 PinOffsetY = FMath::RoundToInt(MyPinPos.Y - (float)Node->NodePosY);
									const int32 ConsumerPinOffsetY = FMath::RoundToInt(ConsumerPinPos.Y - (float)Consumer->NodePosY);

									TargetY = ConsumerPos->Y + ConsumerPinOffsetY - PinOffsetY;
									bAligned = true;
									break;
								}
							}
							if (bAligned) break;
						}
					}
				}

				TargetY = FMath::GridSnap(TargetY, GridSnapSize);
				if (TargetY < CurrentY)
				{
					TargetY = CurrentY;
				}

				NodeLocalPositions.Add(Node, FIntPoint(ColumnX, TargetY));
				CurrentY = TargetY + GetNodeEffectiveHeight(Node) + FMath::RoundToInt(VerticalSpacing * 0.4f);
				CurrentY = FMath::GridSnap(CurrentY, GridSnapSize);
				IslandHeight = FMath::Max(IslandHeight, CurrentY);
			}
		}

		// Ensure island does not collide with previously placed islands
		const int32 IslandWidth = FMath::RoundToInt((SortedRanks.Num() > 0 ? SortedRanks.Last() : 0) * HorizontalSpacing) + 300;
		FBox2D IslandBox(
			FVector2D((float)TargetOriginX, (float)TargetOriginY),
			FVector2D((float)(TargetOriginX + IslandWidth), (float)(TargetOriginY + IslandHeight))
		);

		for (const FBox2D& PlacedBox : PlacedIslandBounds)
		{
			if (IslandBox.Intersect(PlacedBox))
			{
				TargetOriginY = FMath::RoundToInt(PlacedBox.Max.Y + VerticalSpacing);
				TargetOriginY = FMath::GridSnap(TargetOriginY, GridSnapSize);
				IslandBox.Min.Y = (float)TargetOriginY;
				IslandBox.Max.Y = (float)(TargetOriginY + IslandHeight);
			}
		}
		PlacedIslandBounds.Add(IslandBox);

		// Apply world positions to nodes
		for (UEdGraphNode* Node : Island.Nodes)
		{
			if (const FIntPoint* LocalPos = NodeLocalPositions.Find(Node))
			{
				Node->Modify();
				Node->NodePosX = FMath::GridSnap(TargetOriginX + LocalPos->X, GridSnapSize);
				Node->NodePosY = FMath::GridSnap(TargetOriginY + LocalPos->Y, GridSnapSize);
			}
		}

		// Neatly position intermediate reroute knots along their connecting wires
		for (UK2Node_Knot* Knot : Island.Knots)
		{
			if (!Knot) continue;

			UEdGraphPin* InPin = Knot->GetInputPin();
			UEdGraphPin* OutPin = Knot->GetOutputPin();

			if (InPin && InPin->LinkedTo.Num() > 0 && OutPin && OutPin->LinkedTo.Num() > 0)
			{
				UEdGraphNode* SourceNode = InPin->LinkedTo[0]->GetOwningNode();
				UEdGraphNode* TargetNode = OutPin->LinkedTo[0]->GetOwningNode();

				// Only center solitary reroute knots between two non-knot nodes.
				// For multi-knot chains (such as Manhattan routed corners), leave positions intact to prevent collapsing corners.
				if (SourceNode && TargetNode && !SourceNode->IsA<UK2Node_Knot>() && !TargetNode->IsA<UK2Node_Knot>())
				{
					const FVector2D SourcePinPos = FBlueLineManhattanRouter::GetPinPos(InPin->LinkedTo[0]);
					const FVector2D TargetPinPos = FBlueLineManhattanRouter::GetPinPos(OutPin->LinkedTo[0]);

					Knot->Modify();
					Knot->NodePosX = FMath::GridSnap(FMath::RoundToInt32((SourcePinPos.X + TargetPinPos.X) * 0.5f) - 16, GridSnapSize);
					Knot->NodePosY = FMath::GridSnap(FMath::RoundToInt32((SourcePinPos.Y + TargetPinPos.Y) * 0.5f) - 16, GridSnapSize);
				}
			}
		}
	}

	// Update comment boxes that enclosed cleaned nodes
	const float CommentPadding = Settings ? Settings->CommentBoxPadding : 40.0f;
	for (const FCommentSnapshot& Snapshot : CommentSnapshots)
	{
		if (!Snapshot.CommentNode || Snapshot.EnclosedNodes.Num() == 0) continue;

		int32 MinX = TNumericLimits<int32>::Max();
		int32 MinY = TNumericLimits<int32>::Max();
		int32 MaxX = TNumericLimits<int32>::Min();
		int32 MaxY = TNumericLimits<int32>::Min();

		for (UEdGraphNode* Node : Snapshot.EnclosedNodes)
		{
			if (!Node) continue;
			MinX = FMath::Min(MinX, Node->NodePosX);
			MinY = FMath::Min(MinY, Node->NodePosY);
			MaxX = FMath::Max(MaxX, Node->NodePosX + GetNodeEffectiveWidth(Node));
			MaxY = FMath::Max(MaxY, Node->NodePosY + GetNodeEffectiveHeight(Node));
		}

		if (MinX < MaxX && MinY < MaxY)
		{
			Snapshot.CommentNode->Modify();
			Snapshot.CommentNode->NodePosX = FMath::GridSnap(FMath::RoundToInt32((float)MinX - CommentPadding), GridSnapSize);
			Snapshot.CommentNode->NodePosY = FMath::GridSnap(FMath::RoundToInt32((float)MinY - CommentPadding - 35.0f), GridSnapSize);
			Snapshot.CommentNode->NodeWidth = FMath::GridSnap(FMath::RoundToInt32((float)(MaxX - MinX) + CommentPadding * 2.0f), GridSnapSize);
			Snapshot.CommentNode->NodeHeight = FMath::GridSnap(FMath::RoundToInt32((float)(MaxY - MinY) + CommentPadding * 2.0f + 35.0f), GridSnapSize);
		}
	}

	Graph->NotifyGraphChanged();

	if (bSkipGA)
	{
		UE_LOG(LogBlueLineCore, Warning, TEXT("BlueLine: Cleaned %d nodes (Bypassed Genetic Algorithm due to size)."), Analysis.TotalNodes);
	}
	else
	{
		UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine: Cleaned %d nodes in %d islands with optimized topological flow."), Analysis.TotalNodes, Islands.Num());
	}
}

void FBlueLineGraphCleaner::EvolutionaryCrossingMinimizer(TMap<int32, TArray<UEdGraphNode*>>& RankGroups, UEdGraph* Graph)
{
	const int32 PopulationSize = 24;
	const int32 MaxGenerations = 30;
	const float MutationRate = 0.2f;

	uint32 Seed = 0;
	if (Graph && Graph->Nodes.Num() > 0)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
			{
				Seed = Seed * 31 + (uint32)Node->NodePosX;
				Seed = Seed * 31 + (uint32)Node->NodePosY;
			}
		}
		Seed ^= (uint32)Graph->Nodes.Num() * 0x9e3779b9;
	}

	if (Seed == 0)
	{
		Seed = (uint32)FDateTime::Now().GetTicks();
	}

	FMath::RandInit(Seed);

	struct FIndividual
	{
		TMap<int32, TArray<UEdGraphNode*>> Chromosome;
		int32 Fitness = 0;

		void CalculateFitness()
		{
			Fitness = 0;
			TArray<int32> Ranks;
			Chromosome.GetKeys(Ranks);
			Ranks.Sort();

			// Precompute vertical rank indices and node rank map
			TMap<UEdGraphNode*, int32> NodeIndexMap;
			TMap<UEdGraphNode*, int32> NodeRankMap;
			for (int32 r : Ranks)
			{
				const TArray<UEdGraphNode*>& Nodes = Chromosome[r];
				for (int32 i = 0; i < Nodes.Num(); ++i)
				{
					NodeIndexMap.Add(Nodes[i], i);
					NodeRankMap.Add(Nodes[i], r);
				}
			}

			// Evaluate wire crossings across all ranks
			struct FEdge
			{
				int32 RankA;
				int32 IndexA;
				int32 RankB;
				int32 IndexB;
				bool bIsExec;
			};
			TArray<FEdge> Edges;

			for (int32 r : Ranks)
			{
				const TArray<UEdGraphNode*>& Nodes = Chromosome[r];
				for (UEdGraphNode* NodeA : Nodes)
				{
					if (!NodeA) continue;
					const int32 IndexA = NodeIndexMap[NodeA];

					for (UEdGraphPin* Pin : NodeA->Pins)
					{
						if (!Pin || Pin->Direction != EGPD_Output) continue;

						const bool bIsExec = IsExecPin(Pin);
						for (UEdGraphPin* LP : Pin->LinkedTo)
						{
							if (!LP) continue;
							if (UEdGraphNode* NodeB = LP->GetOwningNode())
							{
								if (const int32* IndexB = NodeIndexMap.Find(NodeB))
								{
									if (const int32* RankB = NodeRankMap.Find(NodeB))
									{
										Edges.Add({ r, IndexA, *RankB, *IndexB, bIsExec });
									}
								}
							}
						}
					}
				}
			}

			for (int32 j = 0; j < Edges.Num(); ++j)
			{
				for (int32 k = j + 1; k < Edges.Num(); ++k)
				{
					const FEdge& E1 = Edges[j];
					const FEdge& E2 = Edges[k];

					if (E1.RankA == E2.RankA && E1.RankB == E2.RankB)
					{
						if ((E1.IndexA < E2.IndexA && E1.IndexB > E2.IndexB) ||
							(E1.IndexA > E2.IndexA && E1.IndexB < E2.IndexB))
						{
							Fitness += (E1.bIsExec || E2.bIsExec) ? 10 : 2;
						}
					}
				}
			}
		}
	};

	TArray<FIndividual> Population;

	// Initial Individual: Sort by primary execution status and average parent position
	FIndividual Initial;
	Initial.Chromosome = RankGroups;
	for (auto& KVP : Initial.Chromosome)
	{
		KVP.Value.Sort([](const UEdGraphNode& A, const UEdGraphNode& B) {
			const bool bAIsExec = IsExecNode(&A);
			const bool bBIsExec = IsExecNode(&B);
			if (bAIsExec != bBIsExec)
			{
				return bAIsExec; // Keep execution backbone at the top
			}
			return A.NodePosY < B.NodePosY;
		});
	}
	Initial.CalculateFitness();
	Population.Add(Initial);

	// Generate variants
	for (int32 i = 1; i < PopulationSize; ++i)
	{
		FIndividual Ind = Initial;
		for (auto& KVP : Ind.Chromosome)
		{
			// Only permute non-primary nodes to preserve the execution spine
			if (KVP.Value.Num() > 2)
			{
				const int32 StartIdx = IsExecNode(KVP.Value[0]) ? 1 : 0;
				for (int32 j = KVP.Value.Num() - 1; j > StartIdx; --j)
				{
					const int32 k = FMath::RandRange(StartIdx, j);
					KVP.Value.Swap(j, k);
				}
			}
		}
		Ind.CalculateFitness();
		Population.Add(Ind);
	}

	// Evolution Loop
	for (int32 Gen = 0; Gen < MaxGenerations; ++Gen)
	{
		Population.Sort([](const FIndividual& A, const FIndividual& B) { return A.Fitness < B.Fitness; });
		if (Population[0].Fitness == 0) break;

		TArray<FIndividual> NewPopulation;
		NewPopulation.Add(Population[0]);
		NewPopulation.Add(Population[1]);

		while (NewPopulation.Num() < PopulationSize)
		{
			const int32 i1 = FMath::RandRange(0, PopulationSize / 2);
			const int32 i2 = FMath::RandRange(0, PopulationSize / 2);
			const FIndividual& Parent1 = (Population[i1].Fitness < Population[i2].Fitness) ? Population[i1] : Population[i2];
			const FIndividual& Parent2 = Population[FMath::RandRange(0, PopulationSize / 2)];

			FIndividual Child = Parent1;
			for (auto& KVP : Child.Chromosome)
			{
				if (FMath::RandBool())
				{
					if (const TArray<UEdGraphNode*>* P2Nodes = Parent2.Chromosome.Find(KVP.Key))
					{
						KVP.Value = *P2Nodes;
					}
				}

				if (FMath::FRand() < MutationRate && KVP.Value.Num() > 2)
				{
					const int32 StartIdx = IsExecNode(KVP.Value[0]) ? 1 : 0;
					if (KVP.Value.Num() - 1 > StartIdx)
					{
						const int32 IdxA = FMath::RandRange(StartIdx, KVP.Value.Num() - 1);
						const int32 IdxB = FMath::RandRange(StartIdx, KVP.Value.Num() - 1);
						KVP.Value.Swap(IdxA, IdxB);
					}
				}
			}

			Child.CalculateFitness();
			NewPopulation.Add(Child);
		}
		Population = NewPopulation;
	}

	Population.Sort([](const FIndividual& A, const FIndividual& B) { return A.Fitness < B.Fitness; });
	RankGroups = Population[0].Chromosome;
}

UEdGraph* FBlueLineGraphCleaner::GetActiveGraph()
{
    return FBlueLineContextUtils::GetCurrentGraphFromFocus();
}

#undef LOCTEXT_NAMESPACE
