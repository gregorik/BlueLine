// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "Demos/UK2Node_KingSafety.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "KismetCompiler.h"
#include "Settings/UBlueLineEditorSettings.h"

#define LOCTEXT_NAMESPACE "K2Node_KingSafety"

void UK2Node_KingSafety::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, TEXT("RookChecks"));
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, TEXT("QueenChecks"));
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, TEXT("KingDanger"));
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Boolean, TEXT("bHasShelter"));
	
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Real, TEXT("SafetyScore"));
}

FText UK2Node_KingSafety::GetNodeTitle(ENodeTitleType::Type TitleType) const { return LOCTEXT("Title", "King Safety Eval"); }
FText UK2Node_KingSafety::GetTooltipText() const { return LOCTEXT("Tooltip", "Evaluates king safety metrics."); }
FLinearColor UK2Node_KingSafety::GetNodeTitleColor() const { return FLinearColor(0.8f, 0.1f, 0.1f); }
FText UK2Node_KingSafety::GetMenuCategory() const { return LOCTEXT("Category", "BlueLine|Demos"); }

void UK2Node_KingSafety::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	const UBlueLineEditorSettings* Settings = UBlueLineEditorSettings::Get();
	if (!Settings || !Settings->bExposeDemoNodes)
	{
		return;
	}

	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

UEdGraphPin* UK2Node_KingSafety::GetInputPin() const
{
	return FindPin(UEdGraphSchema_K2::PN_Execute);
}

UEdGraphPin* UK2Node_KingSafety::GetThenPin() const
{
	return FindPin(UEdGraphSchema_K2::PN_Then);
}

void UK2Node_KingSafety::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UEdGraphPin* ExecPin = GetInputPin();
	UEdGraphPin* ThenPin = GetThenPin();

	if (ExecPin && ThenPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*ExecPin, *ThenPin);
	}

	BreakAllNodeLinks();
}

#undef LOCTEXT_NAMESPACE
