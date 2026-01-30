// Copyright YourTeamName. All Rights Reserved.

#include "Routing/FBlueLineManhattanRouter.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node_Knot.h"
#include "ScopedTransaction.h"
#include "Framework/Application/SlateApplication.h"
#include "SGraphPanel.h" 
#include "EdGraphUtilities.h"
#include "BlueLineLog.h"

FBlueLineManhattanRouter::FConfig FBlueLineManhattanRouter::Config;

// Helper struct to persist pin identity across reconstructions
struct FPersistentPin
{
	UEdGraphNode* Node;
	FName PinName;
	EEdGraphPinDirection Direction;

	FPersistentPin(UEdGraphPin* Pin)
	{
		if (Pin)
		{
			Node = Pin->GetOwningNode();
			PinName = Pin->PinName;
			Direction = Pin->Direction;
		}
		else
		{
			Node = nullptr;
		}
	}

	UEdGraphPin* Get() const
	{
		if (!Node) return nullptr;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->PinName == PinName && Pin->Direction == Direction) return Pin;
		}
		return nullptr;
	}
};

void FBlueLineManhattanRouter::RigidifySelectedConnections()
{
	// 1. Context Search
	TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	if (!FocusedWidget.IsValid()) return;

	TSharedPtr<SGraphPanel> GraphPanel;
	TSharedPtr<SWidget> CurrentWidget = FocusedWidget;

	int32 Depth = 0;
	while (CurrentWidget.IsValid() && Depth < 50)
	{
		if (CurrentWidget->GetType().ToString().Contains(TEXT("GraphPanel")))
		{
			GraphPanel = StaticCastSharedPtr<SGraphPanel>(CurrentWidget);
			break;
		}
		CurrentWidget = CurrentWidget->GetParentWidget();
		Depth++;
	}

	if (!GraphPanel.IsValid()) return;

	const FGraphPanelSelectionSet& Selection = GraphPanel->SelectionManager.GetSelectedNodes();
	if (Selection.Num() < 2) return;

	UEdGraph* Graph = nullptr;
	TArray<UEdGraphNode*> SelectedNodes;
	for (UObject* Obj : Selection)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(Obj))
		{
			SelectedNodes.Add(Node);
			if (!Graph) Graph = Node->GetGraph();
		}
	}

	if (!Graph) return;

	// 2. Collection
	struct FConnectionReq { FPersistentPin Out; FPersistentPin In; };
	TArray<FConnectionReq> Requests;

	for (UEdGraphNode* Source : SelectedNodes)
	{
		for (UEdGraphPin* OutputPin : Source->Pins)
		{
			if (OutputPin->Direction != EGPD_Output) continue;

			for (UEdGraphPin* InputPin : OutputPin->LinkedTo)
			{
				UEdGraphNode* Target = InputPin->GetOwningNode();
				if (SelectedNodes.Contains(Target))
				{
					// Left-to-Right only
					if (Target->NodePosX > Source->NodePosX + 150)
					{
						Requests.Add({ FPersistentPin(OutputPin), FPersistentPin(InputPin) });
					}
				}
			}
		}
	}

	if (Requests.Num() == 0) return;

	// 3. Execution
	const FScopedTransaction Transaction(NSLOCTEXT("BlueLine", "Rigidify", "Rigidify Wires"));

	// FIX: Explicitly modify the graph to capture state for Undo
	Graph->Modify();

	bool bGraphModified = false;

	for (const FConnectionReq& Req : Requests)
	{
		// Refresh pointers immediately before use
		UEdGraphPin* SafeOut = Req.Out.Get();
		UEdGraphPin* SafeIn = Req.In.Get();

		if (SafeOut && SafeIn)
		{
			if (RouteConnection(SafeOut, SafeIn, Graph))
			{
				bGraphModified = true;
			}
		}
	}

	if (bGraphModified)
	{
		Graph->NotifyGraphChanged();
	}
}

bool FBlueLineManhattanRouter::RouteConnection(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, UEdGraph* Graph)
{
	// Store persistent handles
	FPersistentPin SafeOut(OutputPin);
	FPersistentPin SafeIn(InputPin);

	FVector2D Start = GetPinPos(OutputPin);
	FVector2D End = GetPinPos(InputPin);

	TArray<FVector2D> PathPoints;
	CalculateManhattanPath(Start, End, PathPoints);

	if (PathPoints.Num() < 3) return false;

	// Capture Pin Type info before potential reconstruction invalidates OutputPin
	const FEdGraphPinType ConnectionType = OutputPin->PinType;

	// 1. Create Knots
	TArray<UK2Node_Knot*> Knots;
	for (int32 i = 1; i < PathPoints.Num() - 1; ++i)
	{
		// FIX: Pass Type to Creation to ensure atomic state
		UK2Node_Knot* Knot = CreateRerouteNode(Graph, PathPoints[i], ConnectionType);
		if (Knot)
		{
			Knots.Add(Knot);
		}
	}

	if (Knots.Num() == 0) return false;

	const UEdGraphSchema* Schema = Graph->GetSchema();

	// 2. Internal Knot Wiring
	for (int32 i = 0; i < Knots.Num() - 1; ++i)
	{
		Schema->TryCreateConnection(Knots[i]->GetOutputPin(), Knots[i + 1]->GetInputPin());
	}

	// 3. Connect OUTPUT -> First Knot
	if (UEdGraphPin* CurrentOut = SafeOut.Get())
	{
		Schema->TryCreateConnection(CurrentOut, Knots[0]->GetInputPin());
	}
	else return false;

	// 4. Connect Last Knot -> INPUT
	if (UEdGraphPin* CurrentIn = SafeIn.Get())
	{
		Schema->TryCreateConnection(Knots.Last()->GetOutputPin(), CurrentIn);
	}
	else return false;

	// 5. Cleanup Old Connection
	UEdGraphPin* FinalOut = SafeOut.Get();
	UEdGraphPin* FinalIn = SafeIn.Get();

	if (FinalOut && FinalIn)
	{
		if (FinalOut->LinkedTo.Contains(FinalIn))
		{
			Schema->BreakSinglePinLink(FinalOut, FinalIn);
		}
	}

	return true;
}

void FBlueLineManhattanRouter::CalculateManhattanPath(const FVector2D& Start, const FVector2D& End, TArray<FVector2D>& OutPoints)
{
	OutPoints.Add(Start);

	// Standard "Z" shape Logic:
	// Output -> [Knot1] -> [Knot2] -> Input

	float MidX = (Start.X + End.X) * 0.5f;

	// FIX: More aggressive spacing.
	// The Knot must be at least 100 units to the right of the Start Pin.
	// This prevents the "Knot inside Node" visual bug shown in your screenshot.
	float MinX = Start.X + 100.0f;
	float MaxX = End.X - 80.0f;

	// If the nodes are squeezed too tight, prioritize the Source Output clearance
	// so the wire doesn't clip the source node text.
	if (MidX < MinX)
	{
		MidX = MinX;
	}

	// If fixing the Start clearance pushed us past the End node, 
	// we are too close to route cleanly. Just clamp to End minus buffer.
	if (MidX > MaxX)
	{
		// If nodes are extremely close (overlapping X), just go halfway
		if (MaxX < MinX)
		{
			MidX = (Start.X + End.X) * 0.5f;
		}
		else
		{
			MidX = MaxX;
		}
	}

	// Point 1 (Out, then vertical move)
	OutPoints.Add(FVector2D(MidX, Start.Y));

	// Point 2 (Vertical move finished, then In)
	OutPoints.Add(FVector2D(MidX, End.Y));

	OutPoints.Add(End);
}

UK2Node_Knot* FBlueLineManhattanRouter::CreateRerouteNode(UEdGraph* Graph, const FVector2D& Position, const FEdGraphPinType& PinType)
{
	FGraphNodeCreator<UK2Node_Knot> NodeCreator(*Graph);
	UK2Node_Knot* Knot = NodeCreator.CreateNode();

	Knot->NodePosX = (int32)Position.X;
	Knot->NodePosY = (int32)Position.Y;

	if (Config.bSnapToGrid)
	{
		Knot->SnapToGrid(Config.GridSnapSize);
	}

	Knot->AllocateDefaultPins();

	// FIX: Finalize first to generate Wildcards
	NodeCreator.Finalize();

	// FIX: IMMEDIATELY set the type so it is recorded in the transaction state correctly.
	// Doing this here ensures that when Undo restores the node, it has the specific type, 
	// preventing "Wildcard vs Concrete" mismatches upon re-connection.
	if (UEdGraphPin* InPin = Knot->GetInputPin())
	{
		InPin->PinType = PinType;
	}
	if (UEdGraphPin* OutPin = Knot->GetOutputPin())
	{
		OutPin->PinType = PinType;
	}

	return Knot;
}

void FBlueLineManhattanRouter::BreakSpecificLink(UEdGraphPin* Output, UEdGraphPin* Input)
{
	if (!Output || !Input) return;
	const UEdGraphSchema* Schema = Output->GetSchema();
	if (Schema) Schema->BreakSinglePinLink(Output, Input);
}

FVector2D FBlueLineManhattanRouter::GetPinPos(UEdGraphPin* Pin)
{
	if (!Pin) return FVector2D::ZeroVector;
	UEdGraphNode* Node = Pin->GetOwningNode();
	if (!Node) return FVector2D::ZeroVector;

	float XOffset = 0.0f;

	if (Pin->Direction == EGPD_Output)
	{
		// FIX: Use a larger minimum width heuristic.
		// Many Function Nodes report NodeWidth=0 until resized.
		// A preset of 160.0f clears most "Function Name" text ranges.
		float ReportedWidth = (float)Node->NodeWidth;
		XOffset = (ReportedWidth > 160.0f) ? ReportedWidth : 160.0f;
	}

	// Y-Offset heuristic (Header is usually ~45-50 units)
	// Just guessing center-ish helps the knots alignment.
	return FVector2D(Node->NodePosX + XOffset, Node->NodePosY + 48.0f);
}