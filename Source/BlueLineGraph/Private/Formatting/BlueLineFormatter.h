// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UEdGraphNode;
class UEdGraph;

/**
 * FBlueLineFormatter
 *
 * Logic engine for the "Soft Format" command.
 */
class BLUELINEGRAPH_API FBlueLineFormatter
{
public:
	/**
	 * Main entry point. Called when the user presses Shift+Q.
	 * Formats the currently selected nodes in the active graph editor.
	 */
	static void FormatActiveGraphSelection();

	/**
	 * Internal logic.
	 */
	static void AutoAlignSelectedNodes(const TSet<UObject*>& SelectedNodes);

private:
	static UEdGraphNode* FindAnchorNode(const TArray<UEdGraphNode*>& SortedNodes);
};