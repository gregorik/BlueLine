// Copyright YourTeamName. All Rights Reserved.

#include "Drawing/FBlueLineGraphPinFactory.h"
#include "Drawing/SBlueLineGraphPin.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"       
#include "GameplayTagContainer.h"

// Using LogTemp for guaranteed visibility (Yellow Text)
TSharedPtr<SGraphPin> FBlueLineGraphPinFactory::CreatePin(UEdGraphPin* InPin) const
{
	if (!InPin) return nullptr;

	// Check if this is a Struct Pin
	if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		UObject* SubObj = InPin->PinType.PinSubCategoryObject.Get();

		// If it's a struct, print WHAT struct it is
		if (SubObj)
		{
			// Un-comment this line if you want to see EVERY struct pin in the log (Spammy but useful)
			// UE_LOG(LogTemp, Warning, TEXT("BlueLine Debug: Saw Struct Pin: %s (Type: %s)"), *InPin->PinName.ToString(), *SubObj->GetName());

			if (UScriptStruct* PinStruct = Cast<UScriptStruct>(SubObj))
			{
				if (PinStruct->IsChildOf(FGameplayTag::StaticStruct()))
				{
					UE_LOG(LogTemp, Error, TEXT("BlueLine SUCCESS: Creating Color Pin for %s"), *InPin->PinName.ToString());
					return SNew(SBlueLineGraphPin, InPin);
				}
			}
		}
	}

	return nullptr;
}