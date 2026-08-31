// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "FBlueLineSmartTagAnalyzer.h"
#include "BlueLineLog.h"
#include "Analysis/FBlueLineGraphAnalyzer.h"  // Now in BlueLineCore
#include "Settings/UBlueLineEditorSettings.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Timeline.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Knot.h"
#include "EdGraphNode_Comment.h"
#include "Misc/ScopeExit.h"
#include "ScopedTransaction.h"
#include "UBlueLineTagCommentNode.h"

#define LOCTEXT_NAMESPACE "BlueLineSmartTagAnalyzer"

TArray<FBlueLineSmartTagSuggestion> FBlueLineSmartTagAnalyzer::SuggestTagsForGraph(UEdGraph* Graph)
{
    TArray<FBlueLineSmartTagSuggestion> Suggestions;
    if (!Graph) return Suggestions;

    FSemanticContext Context;
    const UBlueLineEditorSettings* Settings = UBlueLineEditorSettings::Get();
    const float ClusterWeightMultiplier = Settings ? Settings->ClusterWeightMultiplier : 1.0f;
    const float ConfidenceThreshold = Settings ? Settings->SemanticConfidenceThreshold : 1.0f;

    // 1. Cluster-based Analysis
    TArray<FBlueLineGraphAnalyzer::FNodeCluster> Clusters = FBlueLineGraphAnalyzer::DetectNodeClusters(Graph);
    
    for (const auto& Cluster : Clusters)
    {
        FSemanticContext ClusterContext;
        for (UEdGraphNode* Node : Cluster.Nodes)
        {
            AnalyzeNodeSemantics(Node, ClusterContext);
        }

        // Merge cluster context into global context with cluster-size weighting
        float ClusterMultiplier = FMath::Sqrt((float)Cluster.Nodes.Num()) * ClusterWeightMultiplier;
        for (auto& It : ClusterContext.Weights)
        {
            Context.AddWeight(It.Key, It.Value * ClusterMultiplier);
        }
    }

    // 2. Fallback to individual node analysis for unclustered logic
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        AnalyzeNodeSemantics(Node, Context);
    }

    // 3. Convert weights to suggestions
    for (auto& It : Context.Weights)
    {
        if (It.Value >= ConfidenceThreshold)
        {
            FBlueLineSmartTagSuggestion Suggestion;
            Suggestion.Tag = MapSemanticToTag(It.Key);
            Suggestion.SuggestedColor = GetColorForSemantic(It.Key);
            
            // Normalize confidence (rough heuristic)
            Suggestion.Confidence = FMath::Clamp(It.Value / 20.0f, 0.1f, 0.95f);
            Suggestion.Reason = GetReasonForSemantic(It.Key, Suggestion.Confidence);
            
            Suggestions.Add(Suggestion);
        }
    }

    Suggestions.Sort();
    return Suggestions;
}

TArray<FBlueLineSmartTagSuggestion> FBlueLineSmartTagAnalyzer::SuggestTagsForNode(UEdGraphNode* Node)
{
    TArray<FBlueLineSmartTagSuggestion> Suggestions;
    if (!Node) return Suggestions;

    FSemanticContext Context;
    AnalyzeNodeSemantics(Node, Context);
    const UBlueLineEditorSettings* Settings = UBlueLineEditorSettings::Get();
    const float ConfidenceThreshold = Settings ? FMath::Max(0.5f, Settings->SemanticConfidenceThreshold * 0.5f) : 0.5f;

    for (auto& It : Context.Weights)
    {
        if (It.Value < ConfidenceThreshold)
        {
            continue;
        }

        FBlueLineSmartTagSuggestion Suggestion;
        Suggestion.Tag = MapSemanticToTag(It.Key);
        Suggestion.SuggestedColor = GetColorForSemantic(It.Key);
        Suggestion.Confidence = FMath::Clamp(It.Value / 5.0f, 0.1f, 1.0f);
        Suggestion.Reason = GetReasonForSemantic(It.Key, Suggestion.Confidence);
        Suggestions.Add(Suggestion);
    }

    Suggestions.Sort();
    return Suggestions;
}

void FBlueLineSmartTagAnalyzer::AutoTagGraph(UEdGraph* Graph, const TArray<UEdGraphNode*>& SelectedNodes)
{
    if (!Graph) return;
    
    // Guard against re-entrancy (prevents infinite windows if command is triggered rapidly)
    static bool bIsAutoTagging = false;
    if (bIsAutoTagging) return;
    bIsAutoTagging = true;
    
    ON_SCOPE_EXIT
    {
        bIsAutoTagging = false;
    };

    const FScopedTransaction Transaction(LOCTEXT("AutoTagTransaction", "Auto-Tag Graph"));
    Graph->Modify();
    const UBlueLineEditorSettings* Settings = UBlueLineEditorSettings::Get();
    if (!Settings || !Settings->bEnableBlueLine || !Settings->bEnableSmartTags || !Settings->bEnableAutoTagCommand)
    {
        return;
    }

    const bool bHasUserSelection = SelectedNodes.Num() > 0;

    // 1. Detect Clusters using the Graph Analyzer
    TArray<FBlueLineGraphAnalyzer::FNodeCluster> Clusters = FBlueLineGraphAnalyzer::DetectNodeClusters(Graph, SelectedNodes);

    for (const auto& Cluster : Clusters)
    {
        // "IQ" Check: Don't tag clusters that are too small or likely trivial
        // BUT: If user explicitly selected nodes, respect their selection regardless of size
        int32 MinClusterSizeSelection = Settings ? Settings->MinClusterSizeSelection : 2;
        int32 MinClusterSizeAuto = Settings ? Settings->MinClusterSizeAuto : 3;
        
        if (Cluster.Nodes.Num() < MinClusterSizeSelection) continue;
        if (!bHasUserSelection && Cluster.Nodes.Num() < MinClusterSizeAuto) continue;

        // 2. Analyze Cluster Semantics
        FSemanticContext Context;
        for (UEdGraphNode* Node : Cluster.Nodes)
        {
            AnalyzeNodeSemantics(Node, Context);
        }

        // Find the strongest semantic mapping
        ENodeSemantic BestSemantic = ENodeSemantic::Unknown;
        float BestWeight = 0.0f;
        for (auto& It : Context.Weights)
        {
            if (It.Value > BestWeight)
            {
                BestWeight = It.Value;
                BestSemantic = It.Key;
            }
        }

        // 3. Threshold check: Only tag if we are reasonably sure
        // For user selections, lower the threshold and allow generic comments
        bool bShouldCreateComment = false;
        FGameplayTag Tag;
        FLinearColor Color = FLinearColor::Gray; // Default color if not set
        
        const float EffectiveWeight = BestWeight * (Settings ? Settings->ClusterWeightMultiplier : 1.0f);
        const float ConfidenceThreshold = Settings ? Settings->SemanticConfidenceThreshold : 3.0f;

        if (BestSemantic != ENodeSemantic::Unknown && EffectiveWeight >= ConfidenceThreshold)
        {
            // High confidence semantic match
            Tag = MapSemanticToTag(BestSemantic);
            Color = GetColorForSemantic(BestSemantic);
            bShouldCreateComment = true;
        }
        else if (bHasUserSelection && Cluster.Nodes.Num() >= 2)
        {
            // User explicitly selected these nodes - create a generic comment box
            Tag = Settings ? Settings->DefaultClusterTag : FGameplayTag();
            if (!Tag.IsValid())
            {
                Tag = FGameplayTag::RequestGameplayTag(FName("BlueLine.Type.Unknown"), false);
            }
            if (!Tag.IsValid())
            {
                // If BlueLine tags don't exist at all, use a default empty tag
                Tag = FGameplayTag();
            }
            Color = FLinearColor(0.4f, 0.4f, 0.4f); // Gray for generic selection
            bShouldCreateComment = true;
        }

        if (bShouldCreateComment)
        {
            // Calculate bounds with padding - use smaller padding for tighter fit
            FBox2D Bounds = Cluster.Bounds;
            float Padding = Settings ? Settings->CommentBoxPadding : 40.0f;
            
            UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine: Creating comment box for cluster with %d nodes"), Cluster.Nodes.Num());
            UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine: Bounds Min(%f, %f) Max(%f, %f) Size(%f, %f)"), 
                Bounds.Min.X, Bounds.Min.Y, Bounds.Max.X, Bounds.Max.Y,
                Bounds.GetSize().X, Bounds.GetSize().Y);
            
            // Create the comment node - position it to wrap around the nodes
            // In UE graph coordinates: X increases right, Y increases down
            // Bounds.Min is top-left of the leftmost/topmost node
            // Bounds.Max is bottom-right of the rightmost/bottommost node
            UBlueLineTagCommentNode* CommentNode = NewObject<UBlueLineTagCommentNode>(Graph);
            
            // Position with padding - subtract padding to expand outward
            int32 CommentX = FMath::FloorToInt(Bounds.Min.X - Padding);
            int32 CommentY = FMath::FloorToInt(Bounds.Min.Y - Padding);
            
            // Size includes the full bounds plus padding on both sides
            float CommentWidth = Bounds.GetSize().X + (Padding * 2.0f);
            float CommentHeight = Bounds.GetSize().Y + (Padding * 2.0f);
            
            CommentNode->NodePosX = CommentX;
            CommentNode->NodePosY = CommentY;
            CommentNode->NodeWidth = CommentWidth;
            CommentNode->NodeHeight = CommentHeight;
            
            UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine: Comment box at (%d, %d) size (%f, %f)"), 
                CommentX, CommentY, CommentWidth, CommentHeight);
            UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine: Should cover from (%d, %d) to (%f, %f)"),
                CommentX, CommentY, CommentX + CommentWidth, CommentY + CommentHeight);
            
            // Format title based on whether this was a user selection or auto-detected
            if (bHasUserSelection && !Tag.IsValid())
            {
                // Generic comment for user selection without semantic match
                CommentNode->NodeComment = FString::Printf(TEXT("📦 Selected Nodes"));
            }
            else
            {
                // Semantic-based comment
                FString TagName = Tag.ToString();
                int32 LastDot;
                if (TagName.FindLastChar('.', LastDot)) TagName = TagName.Mid(LastDot + 1);
                
                if (TagName.IsEmpty() || TagName == "None")
                {
                    CommentNode->NodeComment = FString::Printf(TEXT("📦 Selected Nodes"));
                }
                else
                {
                    CommentNode->NodeComment = FString::Printf(TEXT("✨ %s Logic"), *TagName);
                }
            }
            
            if (Settings->bAllowBlueprintColorEdits)
            {
                CommentNode->CommentColor = Color;
            }
            CommentNode->bCommentBubbleVisible = false;
            CommentNode->bCommentBubblePinned = false;
            CommentNode->BlueLineSemanticTag = Tag;
            CommentNode->bBlueLineAutoGenerated = true;

            Graph->AddNode(CommentNode, true, false);
            CommentNode->PostPlacedNewNode();
            CommentNode->AllocateDefaultPins();
        }
    }
}

void FBlueLineSmartTagAnalyzer::AnalyzeNodeSemantics(UEdGraphNode* Node, FSemanticContext& Context)
{
    if (!Node || Node->IsA<UK2Node_Knot>()) return;

    FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
    FString NodeClass = Node->GetClass()->GetName();

    // 1. Title/Name Heuristics (Expanded)
    if (NodeTitle.Contains(TEXT("Location")) || NodeTitle.Contains(TEXT("Rotation")) || 
        NodeTitle.Contains(TEXT("Velocity")) || NodeTitle.Contains(TEXT("Movement")) ||
        NodeTitle.Contains(TEXT("Transform")) || NodeTitle.Contains(TEXT("Physics")))
    {
        Context.AddWeight(ENodeSemantic::Movement, 1.5f);
    }

    if (NodeTitle.Contains(TEXT("Widget")) || NodeTitle.Contains(TEXT("UI")) || 
        NodeTitle.Contains(TEXT("Viewport")) || NodeTitle.Contains(TEXT("HUD")) ||
        NodeTitle.Contains(TEXT("Canvas")) || NodeTitle.Contains(TEXT("Slate")))
    {
        Context.AddWeight(ENodeSemantic::UI, 2.5f);
    }

    if (NodeTitle.Contains(TEXT("Damage")) || NodeTitle.Contains(TEXT("Health")) || 
        NodeTitle.Contains(TEXT("Attack")) || NodeTitle.Contains(TEXT("Weapon")) ||
        NodeTitle.Contains(TEXT("Projectile")) || NodeTitle.Contains(TEXT("Combat")))
    {
        Context.AddWeight(ENodeSemantic::Combat, 2.0f);
    }

    // 2. Variable Access Analysis
    if (UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node))
    {
        FString VarName = VarNode->VariableReference.GetMemberName().ToString();
        if (VarName.Contains(TEXT("Health")) || VarName.Contains(TEXT("HP")) || VarName.Contains(TEXT("Stamina")))
        {
            Context.AddWeight(ENodeSemantic::Combat, 1.2f);
        }
        else if (VarName.Contains(TEXT("Score")) || VarName.Contains(TEXT("Inventory")) || VarName.Contains(TEXT("Data")))
        {
            Context.AddWeight(ENodeSemantic::Data, 1.5f);
        }
    }

    // 3. Pin Type / Math Density Analysis
    int32 FloatPins = 0;
    int32 VectorPins = 0;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Real || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Float) FloatPins++;
        if (Pin->PinType.PinSubCategoryObject == TBaseStructure<FVector>::Get()) VectorPins++;
    }

    if (NodeClass.Contains(TEXT("K2Node_Math")) || NodeClass.Contains(TEXT("K2Node_CallFunction_Math")))
    {
        Context.AddWeight(ENodeSemantic::Logic, 0.8f);
        if (VectorPins > 0) Context.AddWeight(ENodeSemantic::Movement, 0.5f);
    }

    // --- HEURISTIC: Specific Demo Nodes ---
    if (NodeClass.Contains(TEXT("K2Node_KingSafety")))
    {
        Context.AddWeight(ENodeSemantic::Combat, 5.0f);
        Context.AddWeight(ENodeSemantic::AI, 3.0f);
    }
    if (NodeClass.Contains(TEXT("K2Node_AWSTag")))
    {
        Context.AddWeight(ENodeSemantic::Data, 5.0f);
        Context.AddWeight(ENodeSemantic::Networking, 3.0f);
    }

    // 4. Node Class Specific Logic
    if (Node->IsA<UK2Node_Timeline>())
    {
        Context.AddWeight(ENodeSemantic::Visuals, 2.0f);
        Context.AddWeight(ENodeSemantic::Movement, 1.0f); // Timelines are often for movement
    }

    if (UK2Node_CallFunction* CallFunc = Cast<UK2Node_CallFunction>(Node))
    {
        if (UFunction* Func = CallFunc->GetTargetFunction())
        {
            FString FuncName = Func->GetName();
            
            // Audio detection
            if (FuncName.Contains(TEXT("Sound")) || FuncName.Contains(TEXT("Audio")) || FuncName.Contains(TEXT("Acoustic")))
            {
                Context.AddWeight(ENodeSemantic::Audio, 4.0f);
            }
            
            // Networking detection
            if (FuncName.Contains(TEXT("Server")) || FuncName.Contains(TEXT("Multicast")) || 
                FuncName.Contains(TEXT("Reliable")) || FuncName.Contains(TEXT("Rep_")))
            {
                Context.AddWeight(ENodeSemantic::Networking, 5.0f);
            }

            // AI / Behavior detection
            if (FuncName.Contains(TEXT("AI")) || FuncName.Contains(TEXT("Behavior")) || 
                FuncName.Contains(TEXT("Blackboard")) || FuncName.Contains(TEXT("PawnSensing")))
            {
                Context.AddWeight(ENodeSemantic::AI, 3.5f);
            }
        }
    }

    // 5. Context-Aware (Neighbor Analysis)
    // If we are connected to a node that is strongly Combat, we might be combat too
    for (UEdGraphPin* Pin : Node->Pins)
    {
        for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
        {
            if (LinkedPin)
            {
                if (UEdGraphNode* Neighbor = LinkedPin->GetOwningNode())
                {
                    FString NeighborTitle = Neighbor->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
                    if (NeighborTitle.Contains(TEXT("Damage")) || NeighborTitle.Contains(TEXT("Kill")))
                    {
                        Context.AddWeight(ENodeSemantic::Combat, 0.5f);
                    }
                }
            }
        }
    }
}

FGameplayTag FBlueLineSmartTagAnalyzer::MapSemanticToTag(ENodeSemantic Semantic)
{
    // These should ideally match your project's tag hierarchy.
    // If they don't exist, we might want to suggest creating them.
    switch (Semantic)
    {
        case ENodeSemantic::Movement:   return FGameplayTag::RequestGameplayTag(FName("BlueLine.Type.Movement"), false);
        case ENodeSemantic::Combat:     return FGameplayTag::RequestGameplayTag(FName("BlueLine.Type.Combat"), false);
        case ENodeSemantic::UI:         return FGameplayTag::RequestGameplayTag(FName("BlueLine.Type.UI"), false);
        case ENodeSemantic::Input:      return FGameplayTag::RequestGameplayTag(FName("BlueLine.Type.Input"), false);
        case ENodeSemantic::Networking: return FGameplayTag::RequestGameplayTag(FName("BlueLine.Type.Networking"), false);
        case ENodeSemantic::Audio:      return FGameplayTag::RequestGameplayTag(FName("BlueLine.Type.Audio"), false);
        case ENodeSemantic::Visuals:    return FGameplayTag::RequestGameplayTag(FName("BlueLine.Type.Visuals"), false);
        case ENodeSemantic::AI:         return FGameplayTag::RequestGameplayTag(FName("BlueLine.Type.AI"), false);
        case ENodeSemantic::Logic:      return FGameplayTag::RequestGameplayTag(FName("BlueLine.Type.Logic"), false);
        case ENodeSemantic::Data:       return FGameplayTag::RequestGameplayTag(FName("BlueLine.Type.Data"), false);
        default:                        return FGameplayTag::RequestGameplayTag(FName("BlueLine.Type.Unknown"), false);
    }
}

FLinearColor FBlueLineSmartTagAnalyzer::GetColorForSemantic(ENodeSemantic Semantic)
{
    switch (Semantic)
    {
        case ENodeSemantic::Movement:   return FLinearColor(0.0f, 0.8f, 1.0f); // Cyan/Sky
        case ENodeSemantic::Combat:     return FLinearColor(0.9f, 0.1f, 0.1f); // Red
        case ENodeSemantic::UI:         return FLinearColor(0.7f, 0.0f, 0.8f); // Purple/Magenta
        case ENodeSemantic::Input:      return FLinearColor(0.1f, 0.8f, 0.2f); // Green/Emerald
        case ENodeSemantic::Networking: return FLinearColor(1.0f, 0.5f, 0.0f); // Orange
        case ENodeSemantic::Audio:      return FLinearColor(1.0f, 0.9f, 0.0f); // Yellow
        case ENodeSemantic::Visuals:    return FLinearColor(1.0f, 0.2f, 0.6f); // Hot Pink
        case ENodeSemantic::AI:         return FLinearColor(0.1f, 0.2f, 0.8f); // Deep Blue
        case ENodeSemantic::Logic:      return FLinearColor(0.4f, 0.4f, 0.4f); // Gray
        case ENodeSemantic::Data:       return FLinearColor(0.8f, 0.8f, 0.8f); // White/Silver
        default:                        return FLinearColor::Gray;
    }
}

FText FBlueLineSmartTagAnalyzer::GetReasonForSemantic(ENodeSemantic Semantic, float Confidence)
{
    FString ConfidenceStr = (Confidence > 0.8f) ? TEXT("high") : (Confidence > 0.5f ? TEXT("moderate") : TEXT("low"));
    
    switch (Semantic)
    {
        case ENodeSemantic::Movement:   return FText::Format(LOCTEXT("ReasonMovement", "Detected spatial or velocity manipulation ({0} confidence)."), FText::FromString(ConfidenceStr));
        case ENodeSemantic::Combat:     return FText::Format(LOCTEXT("ReasonCombat", "Identified health or damage-related patterns ({0} confidence)."), FText::FromString(ConfidenceStr));
        case ENodeSemantic::UI:         return FText::Format(LOCTEXT("ReasonUI", "Nodes interact with the viewport or widgets ({0} confidence)."), FText::FromString(ConfidenceStr));
        case ENodeSemantic::Input:      return FText::Format(LOCTEXT("ReasonInput", "Cluster starts with user interaction events ({0} confidence)."), FText::FromString(ConfidenceStr));
        case ENodeSemantic::Networking: return FText::Format(LOCTEXT("ReasonNetworking", "Found network-replicated function calls ({0} confidence)."), FText::FromString(ConfidenceStr));
        case ENodeSemantic::AI:         return FText::Format(LOCTEXT("ReasonAI", "Logic involves behavior trees or pathfinding ({0} confidence)."), FText::FromString(ConfidenceStr));
        default:                        return LOCTEXT("ReasonGeneric", "Pattern matches established category markers.");
    }
}

#undef LOCTEXT_NAMESPACE
