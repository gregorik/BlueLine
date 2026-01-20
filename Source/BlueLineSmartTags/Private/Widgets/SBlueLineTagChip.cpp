// Copyright YourTeamName. All Rights Reserved.

#include "Widgets/SBlueLineTagChip.h"
#include "Debug/BlueLineDebugLib.h"
#include "Styling/AppStyle.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "SBlueLineTagChip"

void SBlueLineTagChip::Construct(const FArguments& InArgs)
{
	TagAttribute = InArgs._Tag;
	bShowFullTagName = InArgs._ShowFullTagName;
	OnClickedDelegate = InArgs._OnClicked;

	// Text Block
	TSharedRef<SWidget> TextContent = SNew(STextBlock)
		.Text(this, &SBlueLineTagChip::GetTagText)
		.TextStyle(FAppStyle::Get(), "SmallText")
		.ColorAndOpacity(FLinearColor::Black)
		.ShadowOffset(FVector2D::ZeroVector);

	// The Visuals
	TSharedRef<SWidget> ChipVisual = SNew(SBorder)
		.Padding(InArgs._Padding)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(this, &SBlueLineTagChip::GetChipColor)
		[
			TextContent
		];

	// Interactivity
	if (OnClickedDelegate.IsBound())
	{
		ChildSlot
			[
				SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "NoBorder")
					.ContentPadding(0.0f)
					.OnClicked(OnClickedDelegate)
					.ToolTipText(LOCTEXT("ChipTooltip", "Click to perform action"))
					[
						ChipVisual
					]
			];
	}
	else
	{
		ChildSlot[ChipVisual];
	}

	// FIX: Use modern Attribute binding syntax for visibility
	// TAttribute<T>::FGetter::CreateSP creates a shared-pointer-safe delegate.
	SetVisibility(TAttribute<EVisibility>::Create(
		TAttribute<EVisibility>::FGetter::CreateSP(this, &SBlueLineTagChip::GetChipVisibility)
	));
}

FGameplayTag SBlueLineTagChip::GetGameplayTag() const
{
	return TagAttribute.Get();
}

FSlateColor SBlueLineTagChip::GetChipColor() const
{
	FGameplayTag CurrentTag = GetGameplayTag();
	if (CurrentTag.IsValid())
	{
		return FSlateColor(UBlueLineDebugLib::GetColorForTag(CurrentTag));
	}
	return FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
}

FText SBlueLineTagChip::GetTagText() const
{
	FGameplayTag CurrentTag = GetGameplayTag();
	if (!CurrentTag.IsValid()) return LOCTEXT("None", "None");

	FString FullTagName = CurrentTag.ToString();
	if (bShowFullTagName)
	{
		return FText::FromString(FullTagName);
	}
	else
	{
		int32 LastDotIndex = -1;
		if (FullTagName.FindLastChar('.', LastDotIndex))
		{
			return FText::FromString(FullTagName.Mid(LastDotIndex + 1));
		}
		return FText::FromString(FullTagName);
	}
}

EVisibility SBlueLineTagChip::GetChipVisibility() const
{
	return EVisibility::Visible;
}

#undef LOCTEXT_NAMESPACE