// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "Customization/FBlueLineTagCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h" 
#include "Debug/BlueLineDebugLib.h" 
#include "GameplayTagsEditorModule.h" 
#include "FBlueLineSmartTagAnalyzer.h"
#include "Utils/BlueLineContextUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "GraphEditor.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "BlueLineTagCustomization"

TSharedRef<IPropertyTypeCustomization> FBlueLineTagCustomization::MakeInstance()
{
	return MakeShareable(new FBlueLineTagCustomization());
}

void FBlueLineTagCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructPropertyHandle = PropertyHandle;

	// 1. Get Child Handles
	TagHandle = PropertyHandle->GetChildHandle("Tag");
	ColorHandle = PropertyHandle->GetChildHandle("Color");
	TSharedPtr<IPropertyHandle> ChildrenHandle = PropertyHandle->GetChildHandle("bApplyToChildren");

	if (!TagHandle.IsValid() || !ColorHandle.IsValid()) return;

	// 2. Prepare Data for the Widget
	TSharedPtr<FGameplayTag> InitialTag = MakeShareable(new FGameplayTag());
	void* Data = nullptr;
	if (TagHandle->GetValueData(Data) == FPropertyAccess::Success && Data)
	{
		*InitialTag = *static_cast<FGameplayTag*>(Data);
	}

	// 3. Define the SET Delegate
	FOnSetGameplayTag OnSetTag = FOnSetGameplayTag::CreateLambda([this](const FGameplayTag& NewTag)
		{
			if (TagHandle.IsValid())
			{
				TagHandle->SetValueFromFormattedString(NewTag.ToString());
			}
		});

	// 4. Create the Widget using the UE 5.7 API
	IGameplayTagsEditorModule& TagsEditor = IGameplayTagsEditorModule::Get();

	TSharedRef<SWidget> TagPickerWidget = TagsEditor.MakeGameplayTagWidget(
		OnSetTag,
		InitialTag,
		FString()
	);

	// 5. Build Layout
	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(500.0f)
		[
			SNew(SHorizontalBox)

				// Color Strip
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				.VAlign(VAlign_Fill)
				[
					SNew(SBorder)
						.Padding(FMargin(4.0f, 0.0f))
						.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
						.BorderBackgroundColor(this, &FBlueLineTagCustomization::GetColorFromProperty, ColorHandle)
						.ToolTipText(LOCTEXT("ColorTip", "Color Preview"))
				]

			// The Picker
			+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					TagPickerWidget
				]

				// Smart Suggest Button
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SComboButton)
					.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
					.HasDownArrow(false)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("✨")))
						.ToolTipText(LOCTEXT("SuggestTip", "Smart Suggest Tags"))
					]
					.OnGetMenuContent(this, &FBlueLineTagCustomization::GenerateSuggestionMenu)
				]

				// Color Picker
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					ColorHandle->CreatePropertyValueWidget()
				]

				// Checkbox
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					ChildrenHandle.IsValid() ? ChildrenHandle->CreatePropertyValueWidget() : SNullWidget::NullWidget
				]
		];
}

void FBlueLineTagCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

FReply FBlueLineTagCustomization::OnSuggestTagsClicked()
{
	return FReply::Handled();
}

TSharedRef<SWidget> FBlueLineTagCustomization::GenerateSuggestionMenu() const
{
	FMenuBuilder MenuBuilder(true, nullptr);

	// Find the graph
	TArray<UObject*> OuterObjects;
	StructPropertyHandle->GetOuterObjects(OuterObjects);
	
	UEdGraph* Graph = nullptr;
	UEdGraphNode* Node = nullptr;

	for (UObject* Obj : OuterObjects)
	{
		if (UEdGraphNode* FoundNode = Cast<UEdGraphNode>(Obj))
		{
			Node = FoundNode;
			Graph = FoundNode->GetGraph();
			break;
		}
		
		// If on an actor (e.g. details panel of actor in level or defaults)
		if (AActor* Actor = Cast<AActor>(Obj))
		{
			if (UBlueprint* BP = Cast<UBlueprint>(Actor->GetClass()->ClassGeneratedBy))
			{
				Graph = FBlueprintEditorUtils::FindEventGraph(BP);
				break;
			}
		}

		// If editing Blueprint defaults directly
		if (UBlueprint* BP = Cast<UBlueprint>(Obj))
		{
			Graph = FBlueprintEditorUtils::FindEventGraph(BP);
			break;
		}
	}

	// LAST RESORT: If no context from property, look for any active graph editor
	if (!Graph)
	{
		Graph = FBlueLineContextUtils::GetCurrentGraphFromFocus();
	}

	if (!Node && !Graph)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("NoContext", "No Node or Graph Context Found"),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction()
		);
		return MenuBuilder.MakeWidget();
	}

	// Get Suggestions
	TArray<FBlueLineSmartTagSuggestion> Suggestions;
	if (Node)
	{
		Suggestions = FBlueLineSmartTagAnalyzer::SuggestTagsForNode(Node);
	}
	
	if (Graph)
	{
		// Merge graph suggestions too
		TArray<FBlueLineSmartTagSuggestion> GraphSuggestions = FBlueLineSmartTagAnalyzer::SuggestTagsForGraph(Graph);
		for (const auto& GS : GraphSuggestions)
		{
			bool bExists = false;
			for (auto& S : Suggestions)
			{
				if (S.Tag == GS.Tag)
				{
					S.Confidence = FMath::Max(S.Confidence, GS.Confidence);
					bExists = true;
					break;
				}
			}
			if (!bExists) Suggestions.Add(GS);
		}
	}

	Suggestions.Sort();

	if (Suggestions.Num() == 0)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("NoSuggestions", "No patterns recognized"),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction()
		);
	}

	for (const auto& Suggestion : Suggestions)
	{
		FText Label = FText::Format(LOCTEXT("SuggestLabel", "{0} ({1}%)"), 
			FText::FromName(Suggestion.Tag.GetTagName()), 
			FText::AsNumber(FMath::RoundToInt(Suggestion.Confidence * 100)));

		MenuBuilder.AddMenuEntry(
			Label,
			Suggestion.Reason,
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FBlueLineTagCustomization::ApplySuggestion, Suggestion.Tag, Suggestion.SuggestedColor))
		);
	}

	return MenuBuilder.MakeWidget();
}

void FBlueLineTagCustomization::ApplySuggestion(FGameplayTag Tag, FLinearColor Color) const
{
	if (TagHandle.IsValid())
	{
		TagHandle->SetValueFromFormattedString(Tag.ToString());
	}

	if (ColorHandle.IsValid())
	{
		// We can set the value directly if it's FLinearColor
		// Using a formatted string or direct data access depends on property type
		// But usually SetValue(FLinearColor) works for FLinearColor properties
		ColorHandle->SetValue(Color);
	}
}

FText FBlueLineTagCustomization::GetTagNameFromHandle(TSharedPtr<IPropertyHandle> InTagHandle) const
{
	if (!InTagHandle.IsValid()) return FText::GetEmpty();
	
	FString TagName;
	if (InTagHandle->GetValue(TagName) == FPropertyAccess::Success)
	{
		return FText::FromString(TagName);
	}
	return FText::GetEmpty();
}

FSlateColor FBlueLineTagCustomization::GetColorFromProperty(TSharedPtr<IPropertyHandle> InColorHandle) const
{
	if (!InColorHandle.IsValid()) return FLinearColor::White;

	void* Data = nullptr;
	if (InColorHandle->GetValueData(Data) == FPropertyAccess::Success && Data)
	{
		FLinearColor* Color = (FLinearColor*)Data;
		if (Color) return FSlateColor(*Color);
	}
	return FLinearColor::Gray;
}

#undef LOCTEXT_NAMESPACE
