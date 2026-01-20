// Copyright YourTeamName. All Rights Reserved.

#include "Customization/FBlueLineTagCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h" 
#include "Debug/BlueLineDebugLib.h" 
#include "GameplayTagsEditorModule.h" 

#define LOCTEXT_NAMESPACE "BlueLineTagCustomization"

TSharedRef<IPropertyTypeCustomization> FBlueLineTagCustomization::MakeInstance()
{
	return MakeShareable(new FBlueLineTagCustomization());
}

void FBlueLineTagCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructPropertyHandle = PropertyHandle;

	// 1. Get Child Handles
	TSharedPtr<IPropertyHandle> TagHandle = PropertyHandle->GetChildHandle("Tag");
	TSharedPtr<IPropertyHandle> ColorHandle = PropertyHandle->GetChildHandle("Color");
	TSharedPtr<IPropertyHandle> ChildrenHandle = PropertyHandle->GetChildHandle("bApplyToChildren");

	if (!TagHandle.IsValid() || !ColorHandle.IsValid()) return;

	// 2. Prepare Data for the Widget
	// We need to pass the INITIAL value to the widget.
	TSharedPtr<FGameplayTag> InitialTag = MakeShareable(new FGameplayTag());
	void* Data = nullptr;
	if (TagHandle->GetValueData(Data) == FPropertyAccess::Success && Data)
	{
		*InitialTag = *static_cast<FGameplayTag*>(Data);
	}

	// 3. Define the SET Delegate
	// This function connects the Widget's "On Changed" event back to the Unreal Property System.
	FOnSetGameplayTag OnSetTag = FOnSetGameplayTag::CreateLambda([TagHandle](const FGameplayTag& NewTag)
		{
			if (TagHandle.IsValid())
			{
				// Using formatted string ensures Undo/Redo works and validates input
				TagHandle->SetValueFromFormattedString(NewTag.ToString());
			}
		});

	// 4. Create the Widget using the UE 5.7 API (Delegate-based)
	IGameplayTagsEditorModule& TagsEditor = IGameplayTagsEditorModule::Get();

	TSharedRef<SWidget> TagPickerWidget = TagsEditor.MakeGameplayTagWidget(
		OnSetTag,    // Delegate to call when tag changes
		InitialTag,  // Initial value (By Ptr)
		FString()    // Filter string (Empty allowed)
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
					ChildrenHandle->CreatePropertyValueWidget()
				]
		];
}

void FBlueLineTagCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Hide Default Values
}

FText FBlueLineTagCustomization::GetTagNameFromHandle(TSharedPtr<IPropertyHandle> TagHandle) const
{
	return FText::GetEmpty();
}

FSlateColor FBlueLineTagCustomization::GetColorFromProperty(TSharedPtr<IPropertyHandle> ColorHandle) const
{
	if (!ColorHandle.IsValid()) return FLinearColor::White;

	void* Data = nullptr;
	if (ColorHandle->GetValueData(Data) == FPropertyAccess::Success && Data)
	{
		FLinearColor* Color = (FLinearColor*)Data;
		if (Color) return FSlateColor(*Color);
	}
	return FLinearColor::Gray;
}

#undef LOCTEXT_NAMESPACE