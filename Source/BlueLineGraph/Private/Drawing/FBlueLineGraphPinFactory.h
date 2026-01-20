// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"

class FBlueLineGraphPinFactory : public FGraphPanelPinFactory
{
public:
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override;
};