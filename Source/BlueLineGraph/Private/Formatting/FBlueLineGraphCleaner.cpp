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

#define LOCTEXT_NAMESPACE "BlueLineGraphCleaner"

namespace
{
    bool ShouldCleanLayoutNode(const UEdGraphNode* Node)
    {
        return Node && !Node->IsA<UEdGraphNode_Comment>();
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
    
    // Clean up clutter first
    FBlueLineManhattanRouter::CleanupOrphanedRerouteNodes(Graph);

    // 1. Analyze
    FBlueLineGraphAnalyzer::FAnalysisResult Analysis = FBlueLineGraphAnalyzer::AnalyzeGraph(Graph);
    
    // 2. Identify Connected Components (Islands)
    // We treat execution wires as the primary backbone, data wires as secondary.
    TArray<TArray<UEdGraphNode*>> Islands;
    TSet<UEdGraphNode*> ProcessedNodes;

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!ShouldCleanLayoutNode(Node) || ProcessedNodes.Contains(Node)) continue;

        TArray<UEdGraphNode*> Island;
        TArray<UEdGraphNode*> Stack;
        Stack.Push(Node);

        while (Stack.Num() > 0)
        {
            UEdGraphNode* Current = Stack.Pop();
            if (!ShouldCleanLayoutNode(Current) || ProcessedNodes.Contains(Current)) continue;

            Island.Add(Current);
            ProcessedNodes.Add(Current);

            for (UEdGraphPin* Pin : Current->Pins)
            {
                for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    if (UEdGraphNode* Neighbor = LinkedPin ? LinkedPin->GetOwningNode() : nullptr)
                    {
                        if (ShouldCleanLayoutNode(Neighbor) && !ProcessedNodes.Contains(Neighbor))
                        {
                            Stack.Push(Neighbor);
                        }
                    }
                }
            }
        }
        Islands.Add(Island);
    }

    const float HorizontalSpacing = Settings ? Settings->HorizontalSpacing : 300.0f;
    const float VerticalSpacing = Settings ? Settings->VerticalSpacing : 120.0f;

    float CurrentIslandY = 0.0f;

    bool bSkipGA = Analysis.TotalNodes > 500;

    // 3. Process each Island
    for (const TArray<UEdGraphNode*>& Island : Islands)
    {
        if (Island.Num() == 0) continue;

        // Find "Root" nodes for this island (inputs or nodes with no incoming execution wires)
        TArray<UEdGraphNode*> Roots;
        for (UEdGraphNode* Node : Island)
        {
            bool bHasIncomingExec = false;
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == TEXT("exec") && Pin->LinkedTo.Num() > 0)
                {
                    bHasIncomingExec = true;
                    break;
                }
            }
            if (!bHasIncomingExec) Roots.Add(Node);
        }
        
        // If no clear roots (e.g. data loop or pure isolated nodes), pick the leftmost one
        if (Roots.Num() == 0) Roots.Add(Island[0]);

        // Rank Assignment. Each node is ranked once so cyclic graphs converge.
        TMap<UEdGraphNode*, int32> NodeRanks;
        TArray<UEdGraphNode*> Queue;
        for (UEdGraphNode* Root : Roots)
        {
            if (ShouldCleanLayoutNode(Root) && !NodeRanks.Contains(Root))
            {
                Queue.Add(Root);
                NodeRanks.Add(Root, 0);
            }
        }

        while (Queue.Num() > 0)
        {
            UEdGraphNode* Current = Queue[0];
            Queue.RemoveAt(0);
            int32 Rank = NodeRanks[Current];

            for (UEdGraphPin* Pin : Current->Pins)
            {
                if (Pin->Direction == EGPD_Output)
                {
                    for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                    {
                        if (UEdGraphNode* Neighbor = LinkedPin ? LinkedPin->GetOwningNode() : nullptr)
                        {
                            if (ShouldCleanLayoutNode(Neighbor) && !NodeRanks.Contains(Neighbor))
                            {
                                NodeRanks.Add(Neighbor, Rank + 1);
                                Queue.Add(Neighbor);
                            }
                        }
                    }
                }
            }
        }

        // Layout within Island
        TMap<int32, TArray<UEdGraphNode*>> RankGroups;
        for (UEdGraphNode* Node : Island)
        {
            int32 Rank = NodeRanks.Contains(Node) ? NodeRanks[Node] : 0;
            RankGroups.FindOrAdd(Rank).Add(Node);
        }

        // --- UPGRADE: Genetic Algorithm for Crossing Minimization ---
        if (!bSkipGA && Island.Num() > 1)
        {
            EvolutionaryCrossingMinimizer(RankGroups, Graph);
        }

        float IslandMaxHeight = 0.0f;
        
        TArray<int32> Ranks;
        RankGroups.GetKeys(Ranks);
        Ranks.Sort();

        for (int32 r = 0; r < Ranks.Num(); ++r)
        {
            int32 Rank = Ranks[r];
            TArray<UEdGraphNode*>& NodesInRank = RankGroups[Rank];

            float CurrentY = CurrentIslandY;
            for (int32 i = 0; i < NodesInRank.Num(); ++i)
            {
                UEdGraphNode* Node = NodesInRank[i];
                float TargetY = CurrentY;
                
                // Feature: Strict Node Alignment
                // If it has an execution input, strictly align to it horizontally
                bool bAlignedToExec = false;
                for (UEdGraphPin* Pin : Node->Pins)
                {
                    if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == TEXT("exec") && Pin->LinkedTo.Num() > 0)
                    {
                            if (UEdGraphNode* ParentNode = Pin->LinkedTo[0] ? Pin->LinkedTo[0]->GetOwningNode() : nullptr)
                            {
                                if (ShouldCleanLayoutNode(ParentNode))
                                {
                                    TargetY = ParentNode->NodePosY; // Strictly match parent's Y
                                    bAlignedToExec = true;
                                    break;
                                }
                            }
                        }
                    }

                if (!bAlignedToExec)
                {
                    // Align to data inputs if no exec
                    float SumY = 0.0f;
                    int32 Count = 0;
                    for (UEdGraphPin* Pin : Node->Pins)
                    {
                        if (Pin->Direction == EGPD_Input)
                        {
                            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                            {
                                if (UEdGraphNode* ParentNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr)
                                {
                                    if (ShouldCleanLayoutNode(ParentNode))
                                    {
                                        SumY += ParentNode->NodePosY;
                                        Count++;
                                    }
                                }
                            }
                        }
                    }
                    if (Count > 0)
                    {
                        TargetY = SumY / Count;
                    }
                }

                // Snap TargetY to a 16 unit grid to ensure neat rows (Unreal's default grid is 16)
                TargetY = FMath::RoundToFloat(TargetY / 16.0f) * 16.0f;

                // Enforce minimum vertical spacing to avoid overlaps
                if (TargetY < CurrentY)
                {
                    TargetY = CurrentY;
                }

                Node->Modify();
                Node->NodePosX = Rank * HorizontalSpacing;
                Node->NodePosY = TargetY;
                
                CurrentY = TargetY + VerticalSpacing;
                IslandMaxHeight = FMath::Max(IslandMaxHeight, TargetY - CurrentIslandY + VerticalSpacing);
            }
        }

        CurrentIslandY += IslandMaxHeight + (VerticalSpacing * 2.0f);
    }

    Graph->NotifyGraphChanged();
    
    if (bSkipGA)
    {
        UE_LOG(LogBlueLineCore, Warning, TEXT("BlueLine: Cleaned %d nodes (Bypassed Genetic Algorithm due to size)."), Analysis.TotalNodes);
    }
    else
    {
        UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine: Cleaned %d nodes in %d islands using Evolutionary Optimization."), Analysis.TotalNodes, Islands.Num());
    }
}

void FBlueLineGraphCleaner::EvolutionaryCrossingMinimizer(TMap<int32, TArray<UEdGraphNode*>>& RankGroups, UEdGraph* Graph)
{
    // Genetic Algorithm Parameters
    const int32 PopulationSize = 30;
    const int32 MaxGenerations = 40;
    const float MutationRate = 0.15f;

    // SAFETY: Seed random number generator for deterministic results based on graph state
    // This ensures the same graph produces the same layout, while different graphs get different seeds
    uint32 Seed = 0;
    if (Graph && Graph->Nodes.Num() > 0)
    {
        // Create a seed based on graph characteristics
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node)
            {
                Seed = Seed * 31 + (uint32)Node->NodePosX;
                Seed = Seed * 31 + (uint32)Node->NodePosY;
            }
        }
        // Mix in node count for additional uniqueness
        Seed ^= (uint32)Graph->Nodes.Num() * 0x9e3779b9;
    }
    
    // If no valid graph data, use time-based seed
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

            // Check crossings between adjacent ranks
            for (int32 i = 0; i < Ranks.Num() - 1; ++i)
            {
                const TArray<UEdGraphNode*>& RankA = Chromosome[Ranks[i]];
                const TArray<UEdGraphNode*>& RankB = Chromosome[Ranks[i+1]];

                TMap<UEdGraphNode*, int32> PosA;
                for (int32 j = 0; j < RankA.Num(); ++j) PosA.Add(RankA[j], j);
                
                TMap<UEdGraphNode*, int32> PosB;
                for (int32 j = 0; j < RankB.Num(); ++j) PosB.Add(RankB[j], j);

                // Find all edges between RankA and RankB
                struct FEdge { int32 u; int32 v; };
                TArray<FEdge> Edges;

                for (UEdGraphNode* NodeA : RankA)
                {
                    for (UEdGraphPin* Pin : NodeA->Pins)
                    {
                        if (Pin->Direction == EGPD_Output)
                        {
                            for (UEdGraphPin* LP : Pin->LinkedTo)
                            {
                                if (UEdGraphNode* NodeB = LP ? LP->GetOwningNode() : nullptr)
                                {
                                    if (PosB.Contains(NodeB))
                                    {
                                        Edges.Add({PosA[NodeA], PosB[NodeB]});
                                    }
                                }
                            }
                        }
                    }
                }

                // Count crossings
                for (int32 j = 0; j < Edges.Num(); ++j)
                {
                    for (int32 k = j + 1; k < Edges.Num(); ++k)
                    {
                        const FEdge& E1 = Edges[j];
                        const FEdge& E2 = Edges[k];

                        if ((E1.u < E2.u && E1.v > E2.v) || (E1.u > E2.u && E1.v < E2.v))
                        {
                            Fitness++;
                        }
                    }
                }
            }
        }
    };

    TArray<FIndividual> Population;

    // 1. Initial Population
    // First individual is the Barycenter result (good starting point)
    FIndividual Initial;
    Initial.Chromosome = RankGroups;
    for (auto& KVP : Initial.Chromosome)
    {
        KVP.Value.Sort([&](const UEdGraphNode& A, const UEdGraphNode& B) {
            auto GetAvgParentY = [&](const UEdGraphNode& Node) {
                float SumY = 0.0f; int32 Count = 0;
                for (UEdGraphPin* Pin : Node.Pins) {
                    // SAFETY: Check Pin validity before accessing
                    if (Pin && Pin->Direction == EGPD_Input) {
                        for (UEdGraphPin* LP : Pin->LinkedTo) {
                            // SAFETY: Check linked pin and owning node validity
                            if (LP && LP->GetOwningNode()) {
                                SumY += LP->GetOwningNode()->NodePosY; 
                                Count++;
                            }
                        }
                    }
                }
                return Count > 0 ? SumY / Count : 0.0f;
            };
            return GetAvgParentY(A) < GetAvgParentY(B);
        });
    }
    Initial.CalculateFitness();
    Population.Add(Initial);

    // Fill rest with random permutations
    for (int32 i = 1; i < PopulationSize; ++i)
    {
        FIndividual Ind;
        Ind.Chromosome = RankGroups;
        for (auto& KVP : Ind.Chromosome)
        {
            // Fisher-Yates shuffle
            for (int32 j = KVP.Value.Num() - 1; j > 0; --j)
            {
                int32 k = FMath::RandRange(0, j);
                KVP.Value.Swap(j, k);
            }
        }
        Ind.CalculateFitness();
        Population.Add(Ind);
    }

    // 2. Evolution Loop
    for (int32 Gen = 0; Gen < MaxGenerations; ++Gen)
    {
        Population.Sort([](const FIndividual& A, const FIndividual& B) { return A.Fitness < B.Fitness; });
        
        // If we hit 0 crossings, we're done
        if (Population[0].Fitness == 0) break;

        TArray<FIndividual> NewPopulation;
        // Elitism: Keep top 2
        NewPopulation.Add(Population[0]);
        NewPopulation.Add(Population[1]);

        while (NewPopulation.Num() < PopulationSize)
        {
            // Selection (Tournament)
            auto Select = [&]() -> const FIndividual& {
                int32 i1 = FMath::RandRange(0, PopulationSize / 2);
                int32 i2 = FMath::RandRange(0, PopulationSize / 2);
                return (Population[i1].Fitness < Population[i2].Fitness) ? Population[i1] : Population[i2];
            };

            const FIndividual& Parent1 = Select();
            const FIndividual& Parent2 = Select();

            // Crossover (Uniform Rank Crossover)
            FIndividual Child;
            Child.Chromosome = RankGroups;
            for (auto& KVP : RankGroups)
            {
                int32 Rank = KVP.Key;
                // Randomly take rank ordering from Parent1 or Parent2
                Child.Chromosome[Rank] = (FMath::RandBool()) ? Parent1.Chromosome[Rank] : Parent2.Chromosome[Rank];
            }

            // Mutation (Swap Mutation)
            if (FMath::FRand() < MutationRate)
            {
                TArray<int32> RankKeys;
                Child.Chromosome.GetKeys(RankKeys);
                int32 RandRank = RankKeys[FMath::RandRange(0, RankKeys.Num() - 1)];
                TArray<UEdGraphNode*>& Nodes = Child.Chromosome[RandRank];
                if (Nodes.Num() > 1)
                {
                    Nodes.Swap(FMath::RandRange(0, Nodes.Num() - 1), FMath::RandRange(0, Nodes.Num() - 1));
                }
            }

            Child.CalculateFitness();
            NewPopulation.Add(Child);
        }
        Population = NewPopulation;
    }

    // Apply best result
    Population.Sort([](const FIndividual& A, const FIndividual& B) { return A.Fitness < B.Fitness; });
    RankGroups = Population[0].Chromosome;
}

UEdGraph* FBlueLineGraphCleaner::GetActiveGraph()
{
    return FBlueLineContextUtils::GetCurrentGraphFromFocus();
}

#undef LOCTEXT_NAMESPACE
