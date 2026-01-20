// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"

// Note: Connection Policy hook removed for UE 5.7+ compliance.
class FBlueLineGraphPanelFactory : public FGraphPanelNodeFactory
{
public:
	virtual ~FBlueLineGraphPanelFactory() {}
};