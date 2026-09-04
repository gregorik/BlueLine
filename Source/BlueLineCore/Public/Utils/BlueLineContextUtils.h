// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SGraphPanel;
class SGraphEditor;
class UEdGraph;
class UEdGraphNode;

/**
 * Utility functions for finding editor context (graphs, viewports, etc.)
 * Centralizes the widget traversal logic and provides type-safe access.
 */
class BLUELINECORE_API FBlueLineContextUtils
{
public:
    /**
     * Finds the SGraphPanel from the currently focused widget.
     * Uses configurable max search depth.
     * 
     * @return Valid graph panel pointer, or nullptr if not found
     */
    static TSharedPtr<SGraphPanel> GetFocusedGraphPanel();

    /**
     * Finds the SGraphEditor from the currently focused widget.
     * 
     * @return Valid graph editor pointer, or nullptr if not found
     */
    static TSharedPtr<SGraphEditor> GetFocusedGraphEditor();

    /**
     * Gets the current graph from focus.
     * Convenience wrapper around GetFocusedGraphPanel().
     * 
     * @return Graph object, or nullptr if not in a graph context
     */
    static UEdGraph* GetCurrentGraphFromFocus();

    /**
     * Gets the selected nodes from the currently focused graph.
     * 
     * @param OutSelectedNodes Array to populate with selected nodes
     * @return Number of selected nodes found
     */
    static int32 GetSelectedNodesFromFocus(TArray<UEdGraphNode*>& OutSelectedNodes);

    /**
     * Checks if the current focus is within a Blueprint graph editor.
     */
    static bool IsInBlueprintGraphContext();

private:
    /**
     * Fallback method to find the current graph from the active window/tab.
     * Used when keyboard focus detection fails (e.g., during hotkey processing).
     */
    static UEdGraph* GetCurrentGraphFromActiveTab();

private:
    /** Maximum widget search depth - can be overridden for testing */
    static int32 GetMaxSearchDepth() { return 50; }
};
