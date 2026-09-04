// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "Routing/FBlueLineConnectionInterceptor.h"
#include "BlueLineLog.h"
#include "Routing/FBlueLineManhattanRouter.h"
#include "BlueLineCore/Public/Settings/UBlueLineEditorSettings.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_Knot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintUtilities.h" // For FEdGraphEditAction
#include "ScopedTransaction.h"
#include "UObject/UObjectIterator.h"

bool FBlueLineConnectionInterceptor::bIsEnabled = false;
TArray<FDelegateHandle> FBlueLineConnectionInterceptor::GraphChangedDelegateHandles;
static FDelegateHandle GlobalObjectModifiedDelegateHandle;

// Maximum number of connections to track (prevents unbounded memory growth)
const int32 FBlueLineConnectionInterceptor::MaxTrackedConnections = 10000;
TSet<FBlueLineConnectionInterceptor::FConnectionId> FBlueLineConnectionInterceptor::ProcessedConnections;
TSet<TWeakObjectPtr<UEdGraph>> FBlueLineConnectionInterceptor::BaselinedGraphs;

FBlueLineConnectionInterceptor::FConnectionId::FConnectionId(UEdGraph* InGraph, UEdGraphPin* InOutputPin, UEdGraphPin* InInputPin)
    : Graph(InGraph)
    , OutputPinName(NAME_None)
    , InputPinName(NAME_None)
    , Hash(0)
{
    // UEdGraphPin is not a UObject, so we store the owning node and pin name
    if (InGraph && InOutputPin && InInputPin)
    {
        OutputNode = InOutputPin->GetOwningNode();
        OutputPinName = InOutputPin->PinName;
        
        InputNode = InInputPin->GetOwningNode();
        InputPinName = InInputPin->PinName;
        
        // Create a deterministic hash using UniqueIDs
        uint32 GraphId = InGraph->GetUniqueID();
        uint32 OutNodeId = OutputNode.IsValid() ? OutputNode->GetUniqueID() : 0;
        uint32 InNodeId = InputNode.IsValid() ? InputNode->GetUniqueID() : 0;
        
        Hash = GetTypeHash(GraphId);
        Hash = HashCombine(Hash, GetTypeHash(OutNodeId));
        Hash = HashCombine(Hash, GetTypeHash(OutputPinName));
        Hash = HashCombine(Hash, GetTypeHash(InNodeId));
        Hash = HashCombine(Hash, GetTypeHash(InputPinName));
    }
}

bool FBlueLineConnectionInterceptor::FConnectionId::IsValid() const
{
    return Graph.IsValid() && 
           OutputNode.IsValid() && 
           InputNode.IsValid() && 
           !OutputPinName.IsNone() && 
           !InputPinName.IsNone();
}

bool FBlueLineConnectionInterceptor::FConnectionId::IsStillLinked() const
{
    if (!IsValid())
    {
        return false;
    }

    const UEdGraphNode* OutNode = OutputNode.Get();
    const UEdGraphNode* InNode = InputNode.Get();
    if (!OutNode || !InNode)
    {
        return false;
    }

    UEdGraphPin* OutputPin = OutNode->FindPin(OutputPinName, EGPD_Output);
    UEdGraphPin* InputPin = InNode->FindPin(InputPinName, EGPD_Input);
    return OutputPin && InputPin && OutputPin->LinkedTo.Contains(InputPin);
}

bool FBlueLineConnectionInterceptor::FConnectionId::operator==(const FConnectionId& Other) const
{
    return Hash == Other.Hash && 
           Graph == Other.Graph && 
           OutputNode == Other.OutputNode && 
           InputNode == Other.InputNode &&
           OutputPinName == Other.OutputPinName &&
           InputPinName == Other.InputPinName;
}

void FBlueLineConnectionInterceptor::Enable()
{
    if (bIsEnabled)
    {
        return;
    }

    // FBlueprintEditorUtils::OnGraphChanged does not exist as a static delegate.
    // We use OnObjectModified to detect graph changes. This is high-traffic, so efficient filtering is key.
    GlobalObjectModifiedDelegateHandle = FCoreUObjectDelegates::OnObjectModified.AddStatic(&FBlueLineConnectionInterceptor::OnObjectModified);
    
    bIsEnabled = true;
    
    // Clear processed connections when enabling to start fresh
    ProcessedConnections.Empty();
    BaselinedGraphs.Empty();

    for (TObjectIterator<UEdGraph> It; It; ++It)
    {
        BaselineGraph(*It);
    }
    
    UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine: Auto-routing enabled"));
}

void FBlueLineConnectionInterceptor::Disable()
{
    if (!bIsEnabled)
    {
        return;
    }

    FCoreUObjectDelegates::OnObjectModified.Remove(GlobalObjectModifiedDelegateHandle);
    GlobalObjectModifiedDelegateHandle.Reset();

    bIsEnabled = false;
    
    // Clean up tracked connections
    ProcessedConnections.Empty();
    BaselinedGraphs.Empty();
    
    UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine: Auto-routing disabled"));
}

bool FBlueLineConnectionInterceptor::HasProcessedConnection(const FConnectionId& ConnectionId)
{
    return ProcessedConnections.Contains(ConnectionId);
}

void FBlueLineConnectionInterceptor::MarkConnectionAsProcessed(const FConnectionId& ConnectionId)
{
    // Periodically clean up stale entries if we're approaching the limit
    if (ProcessedConnections.Num() >= MaxTrackedConnections)
    {
        CleanupStaleEntries();
    }
    
    ProcessedConnections.Add(ConnectionId);
}

void FBlueLineConnectionInterceptor::CleanupStaleEntries()
{
    // Remove entries with invalid weak pointers (graph or nodes have been destroyed)
    int32 RemovedCount = 0;
    for (auto It = ProcessedConnections.CreateIterator(); It; ++It)
    {
        if (!It->IsValid() || !It->IsStillLinked())
        {
            It.RemoveCurrent();
            ++RemovedCount;
        }
    }
    
    for (auto It = BaselinedGraphs.CreateIterator(); It; ++It)
    {
        if (!It->IsValid())
        {
            It.RemoveCurrent();
        }
    }
    
    // If still at capacity after cleanup, clear half the entries to make room
    // This uses a simple strategy: remove oldest entries (iteration order)
    if (ProcessedConnections.Num() >= MaxTrackedConnections)
    {
        int32 ToRemove = ProcessedConnections.Num() / 2;
        for (auto It = ProcessedConnections.CreateIterator(); It && ToRemove > 0; ++It, --ToRemove)
        {
            It.RemoveCurrent();
        }
    }
    
    UE_LOG(LogBlueLineCore, Verbose, TEXT("BlueLine: Cleaned up %d stale connection entries. Current count: %d"), 
        RemovedCount, ProcessedConnections.Num());
}

void FBlueLineConnectionInterceptor::BaselineGraph(UEdGraph* Graph)
{
    if (!Graph || BaselinedGraphs.Contains(Graph))
    {
        return;
    }

    const UBlueLineEditorSettings* Settings = GetDefault<UBlueLineEditorSettings>();
    const int32 EffectiveMaxNodes = Settings ? Settings->AutoRouteMaxNodes : 128;
    if (Graph->Nodes.Num() > EffectiveMaxNodes)
    {
        BaselinedGraphs.Add(Graph);
        return;
    }

    TArray<UEdGraphNode*> NodesCopy = Graph->Nodes;
    for (UEdGraphNode* Node : NodesCopy)
    {
        if (!Node || Node->IsA(UK2Node_Knot::StaticClass()))
        {
            continue;
        }

        TArray<UEdGraphPin*> PinsCopy = Node->Pins;
        for (UEdGraphPin* Pin : PinsCopy)
        {
            if (!Pin || Pin->Direction != EGPD_Output)
            {
                continue;
            }

            TArray<UEdGraphPin*> LinkedToCopy = Pin->LinkedTo;
            for (UEdGraphPin* LinkedPin : LinkedToCopy)
            {
                UEdGraphNode* OtherNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
                if (!OtherNode || OtherNode->IsA(UK2Node_Knot::StaticClass()))
                {
                    continue;
                }

                FConnectionId ConnectionId(Graph, Pin, LinkedPin);
                if (ConnectionId.IsValid())
                {
                    MarkConnectionAsProcessed(ConnectionId);
                }
            }
        }
    }

    BaselinedGraphs.Add(Graph);
}

void FBlueLineConnectionInterceptor::OnObjectModified(UObject* Object)
{
    // Re-entrancy guard: RouteConnection creates nodes/connections which trigger
    // more OnObjectModified callbacks. Without this guard, we get infinite recursion.
    static bool bIsProcessing = false;
    if (bIsProcessing)
    {
        return;
    }

    if (!bIsEnabled || !Object)
    {
        return;
    }

    // We only care about Graphs being modified
    UEdGraph* Graph = Cast<UEdGraph>(Object);
    if (!Graph)
    {
        return;
    }

    // OPTIMIZATION: Only process if auto-routing is enabled in settings
    const UBlueLineEditorSettings* Settings = GetDefault<UBlueLineEditorSettings>();
    if (!Settings || !Settings->bAutoRouteNewConnections)
    {
        return;
    }

    // SAFETY: Honor the user-configured AutoRouteMaxNodes ceiling (property-clamped to
    // [10, 1000]). Auto-routing is only valuable during interactive edits; above this
    // node count the global object-modified hook stops scanning to avoid memory churn.
    const int32 EffectiveMaxNodes = Settings->AutoRouteMaxNodes;
    if (Graph->Nodes.Num() > EffectiveMaxNodes)
    {
        return;
    }

    TGuardValue<bool> ProcessingGuard(bIsProcessing, true);
    CleanupStaleEntries();

    bool bNeedsBaseline = !BaselinedGraphs.Contains(Graph);
    if (bNeedsBaseline)
    {
        BaselineGraph(Graph);
        return;
    }

    // FIX: Make a copy of the nodes array to avoid modification during iteration
    // RouteConnection() can create new knot nodes which would invalidate the iterator
    TArray<UEdGraphNode*> NodesCopy = Graph->Nodes;
    
    // Track which connections we process in this pass to avoid duplicate processing
    TArray<FConnectionId> NewlyProcessed;
    
    // Scan the graph for direct non-knot-to-non-knot connections
    for (UEdGraphNode* Node : NodesCopy)
    {
        if (!Node || Node->IsA(UK2Node_Knot::StaticClass()))
        {
            continue;
        }

        // FIX: Also copy the pins array to avoid modification during iteration
        TArray<UEdGraphPin*> PinsCopy = Node->Pins;
        for (UEdGraphPin* Pin : PinsCopy)
        {
            if (!Pin || Pin->Direction != EGPD_Output)
            {
                continue;
            }

            TArray<UEdGraphPin*> LinkedToCopy = Pin->LinkedTo;
            for (UEdGraphPin* LinkedPin : LinkedToCopy)
            {
                // Safety check
                if (!LinkedPin) continue;

                UEdGraphNode* OtherNode = LinkedPin->GetOwningNode();
                if (!OtherNode || OtherNode->IsA(UK2Node_Knot::StaticClass()))
                {
                    continue;
                }

                // Found a direct connection between two non-knot nodes
                // Check if we've already processed this connection
                FConnectionId ConnectionId(Graph, Pin, LinkedPin);
                
                if (!ConnectionId.IsValid())
                {
                    continue;
                }
                
                if (HasProcessedConnection(ConnectionId))
                {
                    // Skip already processed connections
                    continue;
                }
                
                // Mark as processed BEFORE routing to prevent re-entrancy issues
                MarkConnectionAsProcessed(ConnectionId);
                
                NewlyProcessed.Add(ConnectionId);
                
                // Process the new connection
                OnPinConnectionCreated(Pin, LinkedPin);
            }
        }
    }
    
    if (NewlyProcessed.Num() > 0)
    {
        UE_LOG(LogBlueLineCore, Verbose, TEXT("BlueLine: Processed %d new connections"), NewlyProcessed.Num());
    }
}

void FBlueLineConnectionInterceptor::OnPinConnectionCreated(UEdGraphPin* PinA, UEdGraphPin* PinB)
{
    if (!bIsEnabled || !PinA || !PinB)
    {
        return;
    }

    const UBlueLineEditorSettings* Settings = GetDefault<UBlueLineEditorSettings>();
    if (!Settings || !Settings->bAutoRouteNewConnections)
    {
        return;
    }

    // Determine which is output/input
    UEdGraphPin* OutputPin = (PinA->Direction == EGPD_Output) ? PinA : PinB;
    UEdGraphPin* InputPin = (PinA->Direction == EGPD_Input) ? PinA : PinB;

    if (OutputPin->Direction != EGPD_Output || InputPin->Direction != EGPD_Input)
    {
        return; // Invalid connection
    }

    UEdGraphNode* OutputNode = OutputPin->GetOwningNode();
    UEdGraphNode* InputNode = InputPin->GetOwningNode();
    if (!OutputNode || !InputNode)
    {
        return;
    }

    // Do not auto-route if nodes are backwards or too close
    const float MinDist = Settings ? Settings->MinRigidifySpacing : 100.0f;
    if (InputNode->NodePosX < OutputNode->NodePosX + MinDist)
    {
        return;
    }
    
    UEdGraph* Graph = OutputNode->GetGraph();
    if (!Graph)
    {
        return;
    }

    const FScopedTransaction Transaction(NSLOCTEXT("BlueLine", "AutoRouteNewConnection", "BlueLine: Auto Route New Connection"));
    Graph->Modify();
    OutputNode->Modify();
    InputNode->Modify();

    const bool bRouted = FBlueLineManhattanRouter::RouteConnection(
        OutputPin,
        InputPin,
        Graph
    );

    if (bRouted)
    {
        Graph->NotifyGraphChanged();
        UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine: Auto-routed new connection"));
    }
}
