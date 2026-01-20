// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "GameplayTagContainer.h"

/**
 * SBlueLineTagChip
 * 
 * A reusable Slate widget that renders a Gameplay Tag as a "Chip" (Pill shape)
 * with the background color determined by the active BlueLine Theme Data.
 * 
 * Usage:
 * SNew(SBlueLineTagChip)
 *     .Tag(this, &MyClass::GetMyTag) // Can bind to a function
 *     .ShowFullTagName(false)         // Optional: Show "Burn" instead of "Status.Fire.Burn"
 */
class SBlueLineTagChip : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueLineTagChip)
		: _Tag()
		, _ShowFullTagName(true)
		, _Padding(FMargin(6.0f, 2.0f))
	{}

		/** The Gameplay Tag to display. Can be bound to a delegate. */
		SLATE_ATTRIBUTE(FGameplayTag, Tag)

		/** If true, displays "Parent.Child". If false, displays only "Child". */
		SLATE_ARGUMENT(bool, ShowFullTagName)

		/** Padding inside the chip border. */
		SLATE_ATTRIBUTE(FMargin, Padding)

		/** Optional callback when the chip is clicked (e.g. Find References). */
		SLATE_EVENT(FOnClicked, OnClicked)

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

private:
	/** Result of the Tag attribute binding */
	FGameplayTag GetGameplayTag() const; // Renamed from GetTag

	/** Colors and Text resolution */
	FSlateColor GetChipColor() const;
	FText GetTagText() const;
	EVisibility GetChipVisibility() const;

	/** State Storage */
	TAttribute<FGameplayTag> TagAttribute;
	bool bShowFullTagName;
	
	FOnClicked OnClickedDelegate;
};