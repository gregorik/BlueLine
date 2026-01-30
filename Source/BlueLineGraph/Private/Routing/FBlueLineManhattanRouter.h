// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h" // Needed for FEdGraphPinType

class UEdGraphNode;
class UEdGraph;
class UK2Node_Knot;

/**
 * FBlueLineManhattanRouter
 *
 * Inserts Reroute Nodes (Knots) to force wires into orthogonal "Manhattan" shapes.
 */
class FBlueLineManhattanRouter
{
public:
	/**
	 * Main Entry Point for Shift+R.
	 * Analyzes user selection, finds connections between selected nodes,
	 * and creates Manhattan routes for them via Reroute Nodes.
	 */
	static void RigidifySelectedConnections();

	/** Configuration for the routing algorithm */
	struct FConfig
	{
		float HorizontalStubLength = 50.0f;
		float VerticalOffset = 80.0f;
		float GridSnapSize = 16.0f;
		bool bSnapToGrid = true;
	};

	static FConfig Config;

private:
	// -- Routing Logic --
	static bool RouteConnection(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, UEdGraph* Graph);
	static void CalculateManhattanPath(const FVector2D& Start, const FVector2D& End, TArray<FVector2D>& OutPoints);

	// -- Helpers --
	static FVector2D GetPinPos(UEdGraphPin* Pin);

	// Updated Signature: Requires PinType to ensure undo-safe creation
	static UK2Node_Knot* CreateRerouteNode(UEdGraph* Graph, const FVector2D& Position, const FEdGraphPinType& PinType);

	static void BreakSpecificLink(UEdGraphPin* Output, UEdGraphPin* Input);
};