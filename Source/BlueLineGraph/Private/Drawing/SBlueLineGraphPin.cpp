// Copyright YourTeamName. All Rights Reserved.

#include "Drawing/SBlueLineGraphPin.h"
#include "GameplayTagsEditorModule.h"
#include "Debug/BlueLineDebugLib.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SComboButton.h"
#include "Styling/AppStyle.h"
#include "ScopedTransaction.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "GameplayTagsManager.h" // <--- REQUIRED for Safety Check

void SBlueLineGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget> SBlueLineGraphPin::GetDefaultValueWidget()
{
	return SAssignNew(TagComboButton, SComboButton)
		.OnGetMenuContent(this, &SBlueLineGraphPin::OnGetMenuContent)
		.ContentPadding(0.0f)
		.ButtonStyle(FAppStyle::Get(), "NoBorder")
		.ButtonContent()
		[
			SNew(SBorder)
				.Padding(FMargin(6.0f, 2.0f))
				.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
				.BorderBackgroundColor(this, &SBlueLineGraphPin::GetPinColor)
				[
					SNew(STextBlock)
						.Text(this, &SBlueLineGraphPin::GetTagName)
						.TextStyle(FAppStyle::Get(), "SmallText")
						.ColorAndOpacity(FLinearColor::Black)
				]
		];
}

TSharedRef<SWidget> SBlueLineGraphPin::OnGetMenuContent()
{
	TSharedPtr<FGameplayTag> InitialTag = MakeShareable(new FGameplayTag());
	*InitialTag = GetTagFromPin();

	FOnSetGameplayTag OnSetTag = FOnSetGameplayTag::CreateSP(this, &SBlueLineGraphPin::OnTagSelected);

	IGameplayTagsEditorModule& TagsEditor = IGameplayTagsEditorModule::Get();

	return TagsEditor.MakeGameplayTagWidget(
		OnSetTag,
		InitialTag,
		FString()
	);
}

void SBlueLineGraphPin::OnTagSelected(const FGameplayTag& NewTag)
{
	if (TagComboButton.IsValid())
	{
		TagComboButton->SetIsOpen(false);
	}

	if (!GraphPinObj) return;

	FString TagExportString = NewTag.ToString();
	const FScopedTransaction Transaction(NSLOCTEXT("BlueLine", "ChangeTagPin", "Change Gameplay Tag Pin"));
	GraphPinObj->Modify();
	GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, TagExportString);
}

FGameplayTag SBlueLineGraphPin::GetTagFromPin() const
{
	if (!GraphPinObj) return FGameplayTag();

	FString DefaultString = GraphPinObj->GetDefaultAsString();
	if (DefaultString.IsEmpty()) return FGameplayTag();

	// --- CRITICAL FIX START ---

	// Check if the Manager is allocated. 
	// Since we load PreDefault, this code might run before GameplayTags system is ready.
	// Accessing it blindly causes the crash.
	UGameplayTagsManager* TagManager = UGameplayTagsManager::GetIfAllocated();
	if (!TagManager)
	{
		return FGameplayTag(); // Return empty if engine is still booting up
	}

	// --- CRITICAL FIX END ---

	FGameplayTag NewTag;

	// Check format. If it starts with '(', it's an export string.
	if (DefaultString.StartsWith(TEXT("(")))
	{
		NewTag.FromExportString(DefaultString);
	}
	else
	{
		// It's likely just "Status.Fire" text. Request it directly.
		// "false" as second arg prevents error log spam if tag doesn't exist yet
		NewTag = FGameplayTag::RequestGameplayTag(FName(*DefaultString), false);
	}

	return NewTag;
}

FText SBlueLineGraphPin::GetTagName() const
{
	FGameplayTag MyTag = GetTagFromPin();
	if (MyTag.IsValid())
	{
		return FText::FromName(MyTag.GetTagName());
	}
	return NSLOCTEXT("BlueLine", "SelectTag", "Select Tag");
}

FSlateColor SBlueLineGraphPin::GetPinColor() const
{
	FGameplayTag MyTag = GetTagFromPin();
	if (MyTag.IsValid())
	{
		return FSlateColor(UBlueLineDebugLib::GetColorForTag(MyTag));
	}

	return FLinearColor(0.2f, 0.2f, 0.2f, 1.0f);
}