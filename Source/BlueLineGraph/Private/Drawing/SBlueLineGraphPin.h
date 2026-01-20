// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphPin.h"
#include "GameplayTagContainer.h"

class SComboButton;

class SBlueLineGraphPin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SBlueLineGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	virtual TSharedRef<SWidget> GetDefaultValueWidget() override;

private:
	// New Helper: Creates the Dropdown Content (The Tree)
	TSharedRef<SWidget> OnGetMenuContent();

	// Helper: Updates the Tag (Logic) and closes the menu
	void OnTagSelected(const FGameplayTag& NewTag);

	FGameplayTag GetTagFromPin() const;
	FText GetTagName() const;
	FSlateColor GetPinColor() const;

	TSharedPtr<SComboButton> TagComboButton;
};