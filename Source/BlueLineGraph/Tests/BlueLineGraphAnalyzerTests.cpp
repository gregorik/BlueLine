// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Analysis/FBlueLineGraphAnalyzer.h"
#include "Formatting/BlueLineFormatter.h"
#include "Formatting/FBlueLineGraphCleaner.h"
#include "Routing/FBlueLineManhattanRouter.h"
#include "Settings/UBlueLineEditorSettings.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Knot.h"
#include "Misc/ScopeExit.h"

/**
 * Automation tests for BlueLine Core Graph Systems:
 * - Analyzer metrics, clustering, bounds, crossings
 * - Magnet Formatter execution alignment
 * - Evolutionary Graph Cleaner cycle protection, comment preservation, and pure-node layout
 * - Manhattan Router Knot geometry and centering math
 *
 * Run these tests via: Automation Tool -> BlueLine -> Graph
 */
namespace
{
	UEdGraphNode* AddExecTestNode(UEdGraph* Graph, const FVector2D& Position, const TCHAR* NodeName)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph);
		Node->Rename(NodeName, Graph);
		Node->NodePosX = FMath::RoundToInt(Position.X);
		Node->NodePosY = FMath::RoundToInt(Position.Y);
		Node->NodeWidth = 200;
		Node->NodeHeight = 100;
		Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, TEXT("ExecIn"));
		Node->CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, TEXT("ExecOut"));
		Graph->Nodes.Add(Node);
		return Node;
	}

	UEdGraphNode* AddPureTestNode(UEdGraph* Graph, const FVector2D& Position, const TCHAR* NodeName)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph);
		Node->Rename(NodeName, Graph);
		Node->NodePosX = FMath::RoundToInt(Position.X);
		Node->NodePosY = FMath::RoundToInt(Position.Y);
		Node->NodeWidth = 150;
		Node->NodeHeight = 60;
		Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, TEXT("FloatIn"));
		Node->CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Real, TEXT("FloatOut"));
		Graph->Nodes.Add(Node);
		return Node;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueLineGraphAnalyzerTest, 
    "BlueLine.Graph.Analyzer.BasicMetrics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueLineGraphAnalyzerTest::RunTest(const FString& Parameters)
{
    // Create a test graph
    UEdGraph* TestGraph = NewObject<UEdGraph>();
    TestGraph->Schema = UEdGraphSchema_K2::StaticClass();
    
    // Test 1: Empty graph analysis
    {
        FBlueLineGraphAnalyzer::FAnalysisResult Result = FBlueLineGraphAnalyzer::AnalyzeGraph(TestGraph);
        TestEqual(TEXT("Empty graph should have 0 nodes"), Result.TotalNodes, 0);
        TestEqual(TEXT("Empty graph should have 0 connections"), Result.TotalConnections, 0);
        TestEqual(TEXT("Empty graph complexity should be 0"), Result.ComplexityScore, 0.0f);
    }
    
    // Test 2: Single node graph
    {
        UEdGraphNode* TestNode = NewObject<UEdGraphNode>(TestGraph);
        TestNode->NodePosX = 0;
        TestNode->NodePosY = 0;
        TestGraph->Nodes.Add(TestNode);
        
        FBlueLineGraphAnalyzer::FAnalysisResult Result = FBlueLineGraphAnalyzer::AnalyzeGraph(TestGraph);
        TestEqual(TEXT("Single node graph should have 1 node"), Result.TotalNodes, 1);
        TestEqual(TEXT("Single node graph should have 0 connections"), Result.TotalConnections, 0);
        
        // Cleanup
        TestGraph->Nodes.Empty();
    }
    
    return true;
}

/**
 * Test cluster detection functionality
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueLineClusterDetectionTest,
    "BlueLine.Graph.Analyzer.ClusterDetection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueLineClusterDetectionTest::RunTest(const FString& Parameters)
{
    UEdGraph* TestGraph = NewObject<UEdGraph>();
    TestGraph->Schema = UEdGraphSchema_K2::StaticClass();
    
    // Create 3 nodes in cluster 1 (connected together)
    UEdGraphNode* Node1A = NewObject<UEdGraphNode>(TestGraph);
    Node1A->NodePosX = 0;
    Node1A->NodePosY = 0;
    UEdGraphPin* OutPin1A = Node1A->CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, TEXT("Out"));
    
    UEdGraphNode* Node1B = NewObject<UEdGraphNode>(TestGraph);
    Node1B->NodePosX = 200;
    Node1B->NodePosY = 0;
    UEdGraphPin* InPin1B = Node1B->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, TEXT("In"));
    
    // Connect them
    OutPin1A->MakeLinkTo(InPin1B);
    
    TestGraph->Nodes.Add(Node1A);
    TestGraph->Nodes.Add(Node1B);
    
    // Create 1 isolated node (separate cluster)
    UEdGraphNode* Node2 = NewObject<UEdGraphNode>(TestGraph);
    Node2->NodePosX = 1000;
    Node2->NodePosY = 1000;
    TestGraph->Nodes.Add(Node2);
    
    TArray<FBlueLineGraphAnalyzer::FNodeCluster> Clusters = FBlueLineGraphAnalyzer::DetectNodeClusters(TestGraph);
    
    TestEqual(TEXT("Should detect 2 distinct clusters"), Clusters.Num(), 2);
    
    return true;
}

/**
 * Test bounding box calculation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueLineBoundsCalculationTest,
    "BlueLine.Graph.Analyzer.BoundsCalculation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueLineBoundsCalculationTest::RunTest(const FString& Parameters)
{
    UEdGraph* TestGraph = NewObject<UEdGraph>();
    TestGraph->Schema = UEdGraphSchema_K2::StaticClass();
    
    // Create two nodes forming a cluster
    UEdGraphNode* NodeA = NewObject<UEdGraphNode>(TestGraph);
    NodeA->NodePosX = 100;
    NodeA->NodePosY = 100;
    NodeA->NodeWidth = 200;
    NodeA->NodeHeight = 100;
    
    UEdGraphNode* NodeB = NewObject<UEdGraphNode>(TestGraph);
    NodeB->NodePosX = 400;
    NodeB->NodePosY = 300;
    NodeB->NodeWidth = 200;
    NodeB->NodeHeight = 100;
    
    // Connect them
    UEdGraphPin* PinA = NodeA->CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, TEXT("Out"));
    UEdGraphPin* PinB = NodeB->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, TEXT("In"));
    PinA->MakeLinkTo(PinB);
    
    TestGraph->Nodes.Add(NodeA);
    TestGraph->Nodes.Add(NodeB);
    
    TArray<FBlueLineGraphAnalyzer::FNodeCluster> Clusters = FBlueLineGraphAnalyzer::DetectNodeClusters(TestGraph);
    
    TestEqual(TEXT("Should find 1 cluster"), Clusters.Num(), 1);
    
    if (Clusters.Num() > 0)
    {
        const FBox2D& Bounds = Clusters[0].Bounds;
        TestEqual(TEXT("Bounds Min X should be 100"), (float)Bounds.Min.X, 100.0f);
        TestEqual(TEXT("Bounds Min Y should be 100"), (float)Bounds.Min.Y, 100.0f);
        TestEqual(TEXT("Bounds Max X should be 600"), (float)Bounds.Max.X, 600.0f);
        TestEqual(TEXT("Bounds Max Y should be 400"), (float)Bounds.Max.Y, 400.0f);
    }
    
    return true;
}

/**
 * Test wire crossing detection
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueLineWireCrossingTest,
    "BlueLine.Graph.Analyzer.WireCrossings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueLineWireCrossingTest::RunTest(const FString& Parameters)
{
    // Test intersecting lines (X shape)
    FVector2D A1(0, 0);
    FVector2D A2(100, 100);
    FVector2D B1(0, 100);
    FVector2D B2(100, 0);
    
    TestTrue(TEXT("Lines in X shape should intersect"), FBlueLineGraphAnalyzer::DoLinesIntersect(A1, A2, B1, B2));
    
    // Test parallel lines (no intersection)
    FVector2D C1(0, 0);
    FVector2D C2(100, 0);
    FVector2D D1(0, 50);
    FVector2D D2(100, 50);
    
    TestFalse(TEXT("Parallel lines should not intersect"), FBlueLineGraphAnalyzer::DoLinesIntersect(C1, C2, D1, D2));
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueLineFormatterExecAlignmentTest,
    "BlueLine.Graph.Formatter.ExecAlignment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueLineFormatterExecAlignmentTest::RunTest(const FString& Parameters)
{
    UBlueLineEditorSettings* Settings = GetMutableDefault<UBlueLineEditorSettings>();
    const bool bOriginalEnableAutoFormat = Settings->bEnableAutoFormat;
    const EBlueLineFormatStrategy OriginalStrategy = Settings->FormatStrategy;
    const float OriginalHorizontalSpacing = Settings->HorizontalSpacing;
    const float OriginalVerticalSpacing = Settings->VerticalSpacing;
    const bool bOriginalPreventNodeOverlap = Settings->bPreventNodeOverlap;
    const int32 OriginalGridSnapSize = Settings->GridSnapSize;

    ON_SCOPE_EXIT
    {
        Settings->bEnableAutoFormat = bOriginalEnableAutoFormat;
        Settings->FormatStrategy = OriginalStrategy;
        Settings->HorizontalSpacing = OriginalHorizontalSpacing;
        Settings->VerticalSpacing = OriginalVerticalSpacing;
        Settings->bPreventNodeOverlap = bOriginalPreventNodeOverlap;
        Settings->GridSnapSize = OriginalGridSnapSize;
    };

    Settings->bEnableAutoFormat = true;
    Settings->FormatStrategy = EBlueLineFormatStrategy::Flow;
    Settings->HorizontalSpacing = 300.0f;
    Settings->VerticalSpacing = 120.0f;
    Settings->bPreventNodeOverlap = true;
    Settings->GridSnapSize = 16;

    UEdGraph* TestGraph = NewObject<UEdGraph>();
    TestGraph->Schema = UEdGraphSchema_K2::StaticClass();

    UEdGraphNode* SourceNode = AddExecTestNode(TestGraph, FVector2D(0.0f, 0.0f), TEXT("FormatterSource"));
    UEdGraphNode* TargetNode = AddExecTestNode(TestGraph, FVector2D(250.0f, 180.0f), TEXT("FormatterTarget"));
    TestNotNull(TEXT("Source execution node should be created"), SourceNode);
    TestNotNull(TEXT("Target execution node should be created"), TargetNode);
    if (!SourceNode || !TargetNode)
    {
        return false;
    }

    SourceNode->Pins[1]->MakeLinkTo(TargetNode->Pins[0]);
    TestEqual(TEXT("The source exec pin should link to the target exec pin"), SourceNode->Pins[1]->LinkedTo.Num(), 1);

    TSet<UObject*> Selection;
    Selection.Add(SourceNode);
    Selection.Add(TargetNode);

    FBlueLineFormatter::AutoAlignSelectedNodes(Selection);

    const int32 ExpectedMinX = FMath::GridSnap(FMath::RoundToInt(200.0f + Settings->HorizontalSpacing), Settings->GridSnapSize);
    TestTrue(TEXT("Formatting should push the downstream node into the next flow column"), TargetNode->NodePosX >= ExpectedMinX);

    const FVector2D SourceExecPos = FBlueLineManhattanRouter::GetPinPos(SourceNode->Pins[1]);
    const FVector2D TargetExecPos = FBlueLineManhattanRouter::GetPinPos(TargetNode->Pins[0]);
    TestTrue(
        TEXT("Formatting should align the connected execution pins on the Y axis"),
        FMath::IsNearlyEqual(SourceExecPos.Y, TargetExecPos.Y, 1.0f));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueLineGraphCleanerCyclesAndCommentsTest,
    "BlueLine.Graph.Cleaner.CyclesAndComments",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueLineGraphCleanerCyclesAndCommentsTest::RunTest(const FString& Parameters)
{
    UBlueLineEditorSettings* Settings = GetMutableDefault<UBlueLineEditorSettings>();
    const bool bOriginalEnableBlueLine = Settings->bEnableBlueLine;
    const bool bOriginalEnableAutoFormat = Settings->bEnableAutoFormat;
    const float OriginalHorizontalSpacing = Settings->HorizontalSpacing;
    const float OriginalVerticalSpacing = Settings->VerticalSpacing;

    ON_SCOPE_EXIT
    {
        Settings->bEnableBlueLine = bOriginalEnableBlueLine;
        Settings->bEnableAutoFormat = bOriginalEnableAutoFormat;
        Settings->HorizontalSpacing = OriginalHorizontalSpacing;
        Settings->VerticalSpacing = OriginalVerticalSpacing;
    };

    Settings->bEnableBlueLine = true;
    Settings->bEnableAutoFormat = true;
    Settings->HorizontalSpacing = 300.0f;
    Settings->VerticalSpacing = 120.0f;

    UEdGraph* TestGraph = NewObject<UEdGraph>();
    TestGraph->Schema = UEdGraphSchema_K2::StaticClass();

    UEdGraphNode* NodeA = AddExecTestNode(TestGraph, FVector2D(0.0f, 0.0f), TEXT("CleanerCycleA"));
    UEdGraphNode* NodeB = AddExecTestNode(TestGraph, FVector2D(400.0f, 200.0f), TEXT("CleanerCycleB"));
    UEdGraphNode_Comment* CommentNode = NewObject<UEdGraphNode_Comment>(TestGraph);
    CommentNode->NodePosX = 900;
    CommentNode->NodePosY = 700;
    TestGraph->Nodes.Add(CommentNode);

    TestNotNull(TEXT("First cycle node should be created"), NodeA);
    TestNotNull(TEXT("Second cycle node should be created"), NodeB);
    if (!NodeA || !NodeB)
    {
        return false;
    }

    NodeA->Pins[1]->MakeLinkTo(NodeB->Pins[0]);
    NodeB->Pins[1]->MakeLinkTo(NodeA->Pins[0]);

    const int32 OriginalCommentX = CommentNode->NodePosX;
    const int32 OriginalCommentY = CommentNode->NodePosY;

    FBlueLineGraphCleaner::CleanGraph(TestGraph);

    TestEqual(TEXT("CleanGraph should leave comment X unchanged"), CommentNode->NodePosX, OriginalCommentX);
    TestEqual(TEXT("CleanGraph should leave comment Y unchanged"), CommentNode->NodePosY, OriginalCommentY);
    TestEqual(TEXT("The cyclic graph should still contain the two regular nodes and comment"), TestGraph->Nodes.Num(), 3);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueLineGraphCleanerPureNodeFlowTest,
    "BlueLine.Graph.Cleaner.PureNodeFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueLineGraphCleanerPureNodeFlowTest::RunTest(const FString& Parameters)
{
    UBlueLineEditorSettings* Settings = GetMutableDefault<UBlueLineEditorSettings>();
    const bool bOriginalEnableBlueLine = Settings->bEnableBlueLine;
    const float OriginalHorizontalSpacing = Settings->HorizontalSpacing;
    const float OriginalVerticalSpacing = Settings->VerticalSpacing;

    ON_SCOPE_EXIT
    {
        Settings->bEnableBlueLine = bOriginalEnableBlueLine;
        Settings->HorizontalSpacing = OriginalHorizontalSpacing;
        Settings->VerticalSpacing = OriginalVerticalSpacing;
    };

    Settings->bEnableBlueLine = true;
    Settings->HorizontalSpacing = 300.0f;
    Settings->VerticalSpacing = 120.0f;

    UEdGraph* TestGraph = NewObject<UEdGraph>();
    TestGraph->Schema = UEdGraphSchema_K2::StaticClass();

    UEdGraphNode* ExecA = AddExecTestNode(TestGraph, FVector2D(0.0f, 0.0f), TEXT("ExecRootA"));
    UEdGraphNode* ExecB = AddExecTestNode(TestGraph, FVector2D(100.0f, 0.0f), TEXT("ExecConsumerB"));
    UEdGraphPin* DataInPin = ExecB->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, TEXT("DataIn"));

    UEdGraphNode* PureNode = AddPureTestNode(TestGraph, FVector2D(-500.0f, 400.0f), TEXT("PureMath"));

    TestNotNull(TEXT("ExecA created"), ExecA);
    TestNotNull(TEXT("ExecB created"), ExecB);
    TestNotNull(TEXT("PureNode created"), PureNode);
    if (!ExecA || !ExecB || !PureNode)
    {
        return false;
    }

    ExecA->Pins[1]->MakeLinkTo(ExecB->Pins[0]);
    PureNode->Pins[1]->MakeLinkTo(DataInPin);

    FBlueLineGraphCleaner::CleanGraph(TestGraph);

    TestTrue(TEXT("ExecA should precede ExecB horizontally"), ExecA->NodePosX < ExecB->NodePosX);
    TestTrue(TEXT("PureNode should be placed to the left of its consumer ExecB"), PureNode->NodePosX < ExecB->NodePosX);

    const FVector2D ExecAPinPos = FBlueLineManhattanRouter::GetPinPos(ExecA->Pins[1]);
    const FVector2D ExecBPinPos = FBlueLineManhattanRouter::GetPinPos(ExecB->Pins[0]);
    TestTrue(TEXT("Execution wires should be aligned horizontally"), FMath::IsNearlyEqual(ExecAPinPos.Y, ExecBPinPos.Y, 1.0f));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueLineKnotGeometryAndPinPosTest,
    "BlueLine.Graph.Routing.KnotCenteringAndPinPos",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueLineKnotGeometryAndPinPosTest::RunTest(const FString& Parameters)
{
    UEdGraph* TestGraph = NewObject<UEdGraph>();
    TestGraph->Schema = UEdGraphSchema_K2::StaticClass();

    UEdGraphNode* RegularNode = AddExecTestNode(TestGraph, FVector2D(100.0f, 100.0f), TEXT("KnotSourceNode"));
    TestNotNull(TEXT("Regular node should be created"), RegularNode);

    UK2Node_Knot* KnotNode = NewObject<UK2Node_Knot>(TestGraph);
    KnotNode->NodePosX = 400;
    KnotNode->NodePosY = 120;
    KnotNode->AllocateDefaultPins();
    TestGraph->AddNode(KnotNode, false, false);

    TestTrue(TEXT("Knot node has at least one pin"), KnotNode->Pins.Num() > 0);
    if (RegularNode && KnotNode->Pins.Num() > 0)
    {
        RegularNode->Pins[1]->MakeLinkTo(KnotNode->Pins[0]);

        // 1. Verify Knot pin position is centered at (NodePosX + 16, NodePosY + 16)
        const FVector2D KnotPinPos = FBlueLineManhattanRouter::GetPinPos(KnotNode->Pins[0]);
        TestEqual(TEXT("Knot Pin X should be centered at NodePosX + 16"), (float)KnotPinPos.X, 416.0f);
        TestEqual(TEXT("Knot Pin Y should be centered at NodePosY + 16"), (float)KnotPinPos.Y, 136.0f);

        // 2. Verify Graph Analyzer detects Knot as 32x32 bounding box rather than full node (120x100)
        TArray<FBlueLineGraphAnalyzer::FNodeCluster> Clusters = FBlueLineGraphAnalyzer::DetectNodeClusters(TestGraph);
        TestEqual(TEXT("Connected graph forms 1 cluster"), Clusters.Num(), 1);
        if (Clusters.Num() == 1)
        {
            const FBox2D& Bounds = Clusters[0].Bounds;
            TestEqual(TEXT("Cluster Min X matches RegularNode PosX"), (float)Bounds.Min.X, 100.0f);
            TestEqual(TEXT("Cluster Min Y matches RegularNode PosY"), (float)Bounds.Min.Y, 100.0f);
            // With knot 32x32, Max.X is 400 + 32 = 432 (not 400 + 120 = 520)
            TestEqual(TEXT("Cluster Max X matches Knot NodePosX + 32"), (float)Bounds.Max.X, 432.0f);
            TestEqual(TEXT("Cluster Max Y matches RegularNode bottom"), (float)Bounds.Max.Y, 200.0f);
        }
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
