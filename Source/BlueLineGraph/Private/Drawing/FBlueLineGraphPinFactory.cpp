// Copyright YourTeamName. All Rights Reserved.

#include "Drawing/FBlueLineGraphPinFactory.h"
#include "Drawing/SBlueLineGraphPin.h"
#include "Drawing/SBlueLineGraphPinEnhanced.h" 
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"       
#include "GameplayTagContainer.h" 
// REMOVED: #include "GameplayTags/Classes/GameplayTagContainer.h" (This caused the error)

TSharedPtr<SGraphPin> FBlueLineGraphPinFactory::CreatePin(UEdGraphPin* InPin) const
{
	if (!InPin) return nullptr;

	// 1. Check for Gameplay Tag (Priority) - Uses colored SBlueLineGraphPin
	if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		if (UScriptStruct* PinStruct = Cast<UScriptStruct>(InPin->PinType.PinSubCategoryObject.Get()))
		{
			if (PinStruct->IsChildOf(FGameplayTag::StaticStruct()))
			{
				return SNew(SBlueLineGraphPin, InPin);
			}
		}
	}

	// 2. Apply Enhanced Visualization (Stubs/Count) to ALL other pins
	return SNew(SBlueLineGraphPinEnhanced, InPin);
}