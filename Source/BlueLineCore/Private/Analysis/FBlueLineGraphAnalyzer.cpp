// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "Analysis/FBlueLineGraphAnalyzer.h"
#include "BlueLineLog.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_FunctionTerminator.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Knot.h"
#include "Containers/Queue.h"

FBlueLineGraphAnalyzer::FAnalysisResult FBlueLineGraphAnalyzer::AnalyzeGraph(UEdGraph* Graph)
{
    FAnalysisResult Result;

    if (!Graph)
    {
        return Result;
    }

    // Basic metrics
    Result.TotalNodes = Graph->Nodes.Num();
    
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node) continue;
        
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Output)
            {
                Result.TotalConnections += Pin->LinkedTo.Num();
            }
        }
    }

    // Count isolated nodes
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node || Node->IsA<UK2Node_Knot>()) continue;
        
        bool bHasConnections = false;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->LinkedTo.Num() > 0)
            {
                bHasConnections = true;
                break;
            }
        }
        
        if (!bHasConnections)
        {
            Result.IsolatedNodes++;
        }
    }

    // Detect execution chains
    TArray<TArray<UEdGraphNode*>> Chains = FindExecutionChains(Graph);
    Result.ExecutionChains = Chains.Num();

    // Detect clusters
    TArray<FNodeCluster> Clusters = DetectNodeClusters(Graph);
    Result.DataClusters = Clusters.Num();

    // Quality metrics
    Result.WireCrossings = CountWireCrossings(Graph);
    
    // Calculate average connection length
    float TotalLength = 0.0f;
    int32 ConnectionCount = 0;
    
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node) continue;
        
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->Direction != EGPD_Output) continue;
            
            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                TotalLength += CalculateConnectionLength(Pin, LinkedPin);
                ConnectionCount++;
            }
        }
    }
    
    Result.AverageConnectionLength = ConnectionCount > 0 ? TotalLength / ConnectionCount : 0.0f;

    // Count long connections (>500 units is a better threshold for modern screens)
    const float LongWireThreshold = 500.0f;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node) continue;
        
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->Direction != EGPD_Output) continue;
            
            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                if (CalculateConnectionLength(Pin, LinkedPin) > LongWireThreshold)
                {
                    Result.LongConnections++;
                }
            }
        }
    }

    // Calculate density (connections per node)
    Result.GraphDensity = Result.TotalNodes > 0 ? 
        (float)Result.TotalConnections / (float)Result.TotalNodes : 0.0f;

    // Complexity score (0-100) - Logarithmic scale for nodes to avoid oversaturating on large graphs
    float NodeFactor = FMath::Log2((float)Result.TotalNodes + 1.0f) * 10.0f;
    float ConnFactor = (float)Result.TotalConnections * 0.5f;
    float ChainFactor = (float)Result.ExecutionChains * 5.0f;
    
    Result.ComplexityScore = FMath::Clamp(NodeFactor + ConnFactor + ChainFactor, 0.0f, 100.0f);

    // Readability score (0-100)
    float ReadabilityPenalty = 
        (Result.WireCrossings * 3.0f) +
        (Result.LongConnections * 2.0f) +
        (Result.IsolatedNodes * 5.0f) +
        (FMath::Max(0.0f, Result.AverageConnectionLength - 200.0f) * 0.05f);
    
    Result.ReadabilityScore = FMath::Clamp(100.0f - ReadabilityPenalty, 0.0f, 100.0f);

    // Detect issues and suggestions based on calculated metrics
    Result.Issues = DetectIssues(Result);
    Result.Suggestions = GenerateSuggestions(Result);

    return Result;
}

TMap<UEdGraphNode*, FBlueLineGraphAnalyzer::ENodeLayer> FBlueLineGraphAnalyzer::ClassifyNodeLayers(UEdGraph* Graph)
{
    TMap<UEdGraphNode*, ENodeLayer> Layers;
    if (!Graph) return Layers;

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node)
        {
            Layers.Add(Node, DetermineNodeLayer(Node));
        }
    }

    return Layers;
}

TArray<FBlueLineGraphAnalyzer::FNodeCluster> FBlueLineGraphAnalyzer::DetectNodeClusters(UEdGraph* Graph, const TArray<UEdGraphNode*>& SelectedNodes)
{
    TArray<FNodeCluster> Clusters;
    if (!Graph) return Clusters;

    TSet<UEdGraphNode*> ProcessedNodes;
    TSet<UEdGraphNode*> SelectedNodeSet(SelectedNodes);
    const bool bHasSelection = SelectedNodes.Num() > 0;

    UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine: DetectNodeClusters called with %d selected nodes"), SelectedNodes.Num());

    // If we have selected nodes, create a SINGLE cluster containing ONLY those nodes
    // This ensures the comment box wraps exactly what the user selected
    if (bHasSelection)
    {
        FNodeCluster SelectedCluster;
        
        for (UEdGraphNode* Node : SelectedNodes)
        {
            if (Node && !Node->IsA<UK2Node_Knot>())
            {
                SelectedCluster.Nodes.Add(Node);
                UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine: Adding node '%s' to cluster"), 
                    *Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
            }
            else if (Node)
            {
                UE_LOG(LogBlueLineCore, Warning, TEXT("BlueLine: Skipping knot node"));
            }
        }

        if (SelectedCluster.Nodes.Num() > 0)
        {
            CalculateClusterBounds(SelectedCluster);
            SelectedCluster.Layer = ENodeLayer::Processing; // Default for selection
            Clusters.Add(SelectedCluster);
            UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine: Created cluster with %d nodes, bounds Min(%f, %f) Max(%f, %f)"),
                SelectedCluster.Nodes.Num(), SelectedCluster.Bounds.Min.X, SelectedCluster.Bounds.Min.Y,
                SelectedCluster.Bounds.Max.X, SelectedCluster.Bounds.Max.Y);
        }
        else
        {
            UE_LOG(LogBlueLineCore, Warning, TEXT("BlueLine: No valid nodes in selection!"));
        }
        
        return Clusters; // Return early - we only want the selected nodes
    }

    // No selection - detect all clusters in the graph (original behavior)
    for (UEdGraphNode* StartNode : Graph->Nodes)
    {
        if (!StartNode || ProcessedNodes.Contains(StartNode) || StartNode->IsA<UK2Node_Knot>())
        {
            continue;
        }

        FNodeCluster Cluster;
        TArray<UEdGraphNode*> ToProcess;
        ToProcess.Add(StartNode);

        while (ToProcess.Num() > 0)
        {
            UEdGraphNode* Current = ToProcess.Pop();
            if (ProcessedNodes.Contains(Current)) continue;

            Cluster.Nodes.Add(Current);
            ProcessedNodes.Add(Current);

            for (UEdGraphPin* Pin : Current->Pins)
            {
                for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    if (UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr)
                    {
                        if (!ProcessedNodes.Contains(LinkedNode))
                        {
                            ToProcess.Add(LinkedNode);
                        }
                    }
                }
            }
        }

        if (Cluster.Nodes.Num() > 0)
        {
            CalculateClusterBounds(Cluster);
            Cluster.Layer = DetermineNodeLayer(StartNode);
            Clusters.Add(Cluster);
        }
    }

    return Clusters;
}

TArray<TArray<UEdGraphNode*>> FBlueLineGraphAnalyzer::FindExecutionChains(UEdGraph* Graph)
{
    TArray<TArray<UEdGraphNode*>> Chains;
    if (!Graph) return Chains;

    TSet<UEdGraphNode*> ProcessedNodes;

    for (UEdGraphNode* StartNode : Graph->Nodes)
    {
        if (!StartNode || ProcessedNodes.Contains(StartNode) || !IsInputNode(StartNode))
        {
            continue;
        }

        TArray<UEdGraphNode*> Chain;
        UEdGraphNode* Current = StartNode;

        while (Current && !ProcessedNodes.Contains(Current))
        {
            Chain.Add(Current);
            ProcessedNodes.Add(Current);

            UEdGraphNode* Next = nullptr;
            for (UEdGraphPin* Pin : Current->Pins)
            {
                if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->Direction == EGPD_Output)
                {
                    if (Pin->LinkedTo.Num() > 0)
                    {
                        Next = Pin->LinkedTo[0]->GetOwningNode();
                        break; 
                    }
                }
            }
            Current = Next;
        }

        if (Chain.Num() > 0)
        {
            Chains.Add(Chain);
        }
    }

    return Chains;
}

int32 FBlueLineGraphAnalyzer::CountWireCrossings(UEdGraph* Graph)
{
    if (!Graph) return 0;

    struct FWireSegment { FVector2D Start; FVector2D End; };
    TArray<FWireSegment> Wires;

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node) continue;
        FVector2D StartPos = GetNodePosition(Node);

        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Output)
            {
                for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    if (LinkedPin)
                    {
                        Wires.Add({StartPos, GetNodePosition(LinkedPin->GetOwningNode())});
                    }
                }
            }
        }
    }

    int32 CrossingCount = 0;
    
    // Cap wire crossing calculations to prevent O(W^2) stalls on very large graphs
    const int32 MaxWiresToAnalyze = 500;
    int32 WiresToProcess = FMath::Min(Wires.Num(), MaxWiresToAnalyze);
    
    for (int32 i = 0; i < WiresToProcess; ++i)
    {
        for (int32 j = i + 1; j < WiresToProcess; ++j)
        {
            if (DoLinesIntersect(Wires[i].Start, Wires[i].End, Wires[j].Start, Wires[j].End))
            {
                CrossingCount++;
            }
        }
    }
    
    // Extrapolate the result if we capped it to maintain a realistic complexity score
    if (Wires.Num() > MaxWiresToAnalyze)
    {
        float Ratio = (float)Wires.Num() / (float)MaxWiresToAnalyze;
        CrossingCount = FMath::RoundToInt((float)CrossingCount * Ratio * Ratio);
    }

    return CrossingCount;
}

FBox2D FBlueLineGraphAnalyzer::CalculateGraphBounds(UEdGraph* Graph)
{
    if (!Graph || Graph->Nodes.Num() == 0) return FBox2D(FVector2D::ZeroVector, FVector2D::ZeroVector);

    FVector2D Min(FLT_MAX, FLT_MAX);
    FVector2D Max(-FLT_MAX, -FLT_MAX);

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node)
        {
            FVector2D Pos = GetNodePosition(Node);
            Min = FVector2D::Min(Min, Pos);
            Max = FVector2D::Max(Max, Pos);
        }
    }

    return FBox2D(Min, Max);
}

TArray<FString> FBlueLineGraphAnalyzer::DetectIssues(const FAnalysisResult& Analysis)
{
    TArray<FString> Issues;

    if (Analysis.WireCrossings > 15) Issues.Add(FString::Printf(TEXT("Tangled wires detected (%d crossings)"), Analysis.WireCrossings));
    if (Analysis.LongConnections > 4) Issues.Add(TEXT("Excessive long-distance connections"));
    if (Analysis.IsolatedNodes > 0) Issues.Add(FString::Printf(TEXT("%d disconnected nodes found"), Analysis.IsolatedNodes));
    if (Analysis.ComplexityScore > 80.0f) Issues.Add(TEXT("Extremely high graph complexity"));
    if (Analysis.ReadabilityScore < 40.0f) Issues.Add(TEXT("Low visual readability"));

    return Issues;
}

TArray<FString> FBlueLineGraphAnalyzer::GenerateSuggestions(const FAnalysisResult& Analysis)
{
    TArray<FString> Suggestions;

    if (Analysis.WireCrossings > 5) Suggestions.Add(TEXT("Reorganize nodes to untangle wires"));
    if (Analysis.LongConnections > 0) Suggestions.Add(TEXT("Use Reroute nodes or Manhattan routing for distant pins"));
    if (Analysis.IsolatedNodes > 0) Suggestions.Add(TEXT("Cleanup or connect orphan nodes"));
    if (Analysis.ComplexityScore > 60.0f) Suggestions.Add(TEXT("Consider splitting logic into Functions or Macros"));
    if (Analysis.ReadabilityScore < 70.0f) Suggestions.Add(TEXT("Run BlueLine Clean Graph for optimal alignment"));

    return Suggestions;
}

// Private Helpers

FBlueLineGraphAnalyzer::ENodeLayer FBlueLineGraphAnalyzer::DetermineNodeLayer(UEdGraphNode* Node)
{
    if (IsInputNode(Node)) return ENodeLayer::Input;
    if (IsOutputNode(Node)) return ENodeLayer::Output;
    
    // Check if it's a pure function call
    if (UK2Node_CallFunction* CallFunc = Cast<UK2Node_CallFunction>(Node))
    {
        if (CallFunc->IsNodePure()) return ENodeLayer::Utility;
    }

    return ENodeLayer::Processing;
}

bool FBlueLineGraphAnalyzer::IsInputNode(UEdGraphNode* Node)
{
    return Node && (Node->IsA<UK2Node_Event>() || Node->IsA<UK2Node_FunctionEntry>());
}

bool FBlueLineGraphAnalyzer::IsOutputNode(UEdGraphNode* Node)
{
    return Node && (Node->IsA<UK2Node_FunctionResult>() || Node->IsA<UK2Node_FunctionTerminator>());
}

bool FBlueLineGraphAnalyzer::IsPureNode(UEdGraphNode* Node)
{
    if (!Node) return false;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) return false;
    }
    return true;
}

int32 FBlueLineGraphAnalyzer::CalculateNodeDepth(UEdGraphNode* Node, UEdGraph* Graph)
{
    if (!Node || !Graph) return 0;

    // BFS to find shortest path from any input node
    TQueue<UEdGraphNode*> Queue;
    TMap<UEdGraphNode*, int32> Distances;

    for (UEdGraphNode* GNode : Graph->Nodes)
    {
        if (IsInputNode(GNode))
        {
            Queue.Enqueue(GNode);
            Distances.Add(GNode, 0);
        }
    }

    while (!Queue.IsEmpty())
    {
        UEdGraphNode* Current;
        Queue.Dequeue(Current);
        int32 Dist = Distances[Current];

        if (Current == Node) return Dist;

        for (UEdGraphPin* Pin : Current->Pins)
        {
            if (Pin->Direction == EGPD_Output)
            {
                for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    if (UEdGraphNode* Neighbor = LinkedPin->GetOwningNode())
                    {
                        if (!Distances.Contains(Neighbor))
                        {
                            Distances.Add(Neighbor, Dist + 1);
                            Queue.Enqueue(Neighbor);
                        }
                    }
                }
            }
        }
    }

    return 0;
}

bool FBlueLineGraphAnalyzer::DoLinesIntersect(const FVector2D& A1, const FVector2D& A2, const FVector2D& B1, const FVector2D& B2)
{
    auto CCW = [](const FVector2D& A, const FVector2D& B, const FVector2D& C) {
        return (C.Y - A.Y) * (B.X - A.X) > (B.Y - A.Y) * (C.X - A.X);
    };
    return CCW(A1, B1, B2) != CCW(A2, B1, B2) && CCW(A1, A2, B1) != CCW(A1, A2, B2);
}

FVector2D FBlueLineGraphAnalyzer::GetNodePosition(UEdGraphNode* Node)
{
    return Node ? FVector2D((float)Node->NodePosX, (float)Node->NodePosY) : FVector2D::ZeroVector;
}

float FBlueLineGraphAnalyzer::CalculateConnectionLength(UEdGraphPin* OutputPin, UEdGraphPin* InputPin)
{
    if (!OutputPin || !InputPin) return 0.0f;
    return FVector2D::Distance(GetNodePosition(OutputPin->GetOwningNode()), GetNodePosition(InputPin->GetOwningNode()));
}

void FBlueLineGraphAnalyzer::CalculateClusterBounds(FNodeCluster& Cluster)
{
    if (Cluster.Nodes.Num() == 0) return;

    FVector2D Sum = FVector2D::ZeroVector;
    FVector2D Min(FLT_MAX, FLT_MAX);
    FVector2D Max(-FLT_MAX, -FLT_MAX);

    // Minimum node dimensions - K2 nodes are typically around 200x100
    const float MinNodeWidth = 200.0f;
    const float MinNodeHeight = 100.0f;

    for (UEdGraphNode* Node : Cluster.Nodes)
    {
        if (Node)
        {
            FVector2D Pos = GetNodePosition(Node);
            
            // Account for node width and height to get full bounds
            // Use max to ensure minimum dimensions (some nodes report 0 size)
            float NodeW = FMath::Max((float)Node->NodeWidth, MinNodeWidth);
            float NodeH = FMath::Max((float)Node->NodeHeight, MinNodeHeight);
            FVector2D BottomRight = Pos + FVector2D(NodeW, NodeH);
            
            Sum += Pos;
            Min = FVector2D::Min(Min, Pos);
            Max = FVector2D::Max(Max, BottomRight);
            
            UE_LOG(LogBlueLineCore, Verbose, TEXT("BlueLine: Node '%s' at (%d,%d) size (%d,%d) using (%.0f,%.0f)"),
                *Node->GetNodeTitle(ENodeTitleType::ListView).ToString(),
                Node->NodePosX, Node->NodePosY,
                Node->NodeWidth, Node->NodeHeight,
                NodeW, NodeH);
        }
    }

    Cluster.CenterPoint = Sum / (float)Cluster.Nodes.Num();
    Cluster.Bounds = FBox2D(Min, Max);
}
