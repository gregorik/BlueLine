// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class UEdGraph;
class UEdGraphPin;
class UEdGraphNode;

/**
 * FBlueLineConnectionInterceptor
 * 
 * Automatically applies Manhattan routing to new connections as they're created.
 * This provides real-time Manhattan routing without manual intervention.
 * 
 * Can be toggled on/off via settings.
 */
class FBlueLineConnectionInterceptor
{
public:
    /**
     * Enable automatic Manhattan routing for new connections.
     * Call this in module StartupModule() if auto-routing is enabled in settings.
     */
    static void Enable();

    /**
     * Disable automatic routing.
     * Call this in module ShutdownModule().
     */
    static void Disable();

    /**
     * Check if auto-routing is currently active.
     */
    static bool IsEnabled() { return bIsEnabled; }

private:
    // Unique identifier for a connection between two pins
    // Note: UEdGraphPin is not a UObject, so we store the owning node and pin name
    struct FConnectionId
    {
        TWeakObjectPtr<UEdGraph> Graph;
        TWeakObjectPtr<UEdGraphNode> OutputNode;
        FName OutputPinName;
        TWeakObjectPtr<UEdGraphNode> InputNode;
        FName InputPinName;
        uint32 Hash;

        FConnectionId() : OutputPinName(NAME_None), InputPinName(NAME_None), Hash(0) {}
        FConnectionId(UEdGraph* InGraph, UEdGraphPin* InOutputPin, UEdGraphPin* InInputPin);

        bool IsValid() const;
        bool IsStillLinked() const;
        bool operator==(const FConnectionId& Other) const;
        friend uint32 GetTypeHash(const FConnectionId& Id) { return Id.Hash; }
    };

    // Graph notification callbacks
    static void OnObjectModified(UObject* Object);
    
    // Handle new pin connections
    static void OnPinConnectionCreated(UEdGraphPin* PinA, UEdGraphPin* PinB);

    // Check if a connection has already been processed
    static bool HasProcessedConnection(const FConnectionId& ConnectionId);
    static void MarkConnectionAsProcessed(const FConnectionId& ConnectionId);

    // Record existing direct connections without routing them.
    static void BaselineGraph(UEdGraph* Graph);

    // Cleanup stale entries from the processed set
    static void CleanupStaleEntries();

    // State
    static bool bIsEnabled;
    static TArray<FDelegateHandle> GraphChangedDelegateHandles;
    
    // Track processed connections to avoid re-routing existing wires
    // Uses TSet for O(1) lookup performance
    static TSet<FConnectionId> ProcessedConnections;
    static const int32 MaxTrackedConnections;
    
    // Track which graphs have been baselined (all existing connections recorded)
    static TSet<TWeakObjectPtr<UEdGraph>> BaselinedGraphs;
};
