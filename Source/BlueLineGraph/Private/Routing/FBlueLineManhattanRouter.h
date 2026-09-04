// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h" 

class UEdGraphNode;
class UEdGraph;
class UK2Node_Knot;

class FBlueLineManhattanRouter
{
public:
	// Shift+R Entry Point
	static void RigidifySelectedConnections();

	// Menu Entry Point (Public)
	static bool RouteConnection(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, UEdGraph* Graph, float StaggerOffset = 0.0f);

	// Cleanup Entry Point (Public)
	static int32 CleanupOrphanedRerouteNodes(UEdGraph* Graph);

	static FVector2D GetPinPos(UEdGraphPin* Pin);

private:
	// Internal Helpers
	static void CalculateManhattanPath(const FVector2D& Start, const FVector2D& End, TArray<FVector2D>& OutPoints, float StaggerOffset = 0.0f);
	static UK2Node_Knot* CreateRerouteNode(UEdGraph* Graph, const FVector2D& Position, const FEdGraphPinType& PinType);
	static void BreakSpecificLink(UEdGraphPin* Output, UEdGraphPin* Input);
};
