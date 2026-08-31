// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"

/**
 * Registers BlueLine's custom connection drawing policy for Blueprint graphs.
 */
class FBlueLineGraphConnectionFactory : public FGraphPanelPinConnectionFactory
{
public:
	virtual ~FBlueLineGraphConnectionFactory() override = default;

	virtual FConnectionDrawingPolicy* CreateConnectionPolicy(
		const UEdGraphSchema* Schema,
		int32 InBackLayerID,
		int32 InFrontLayerID,
		float ZoomFactor,
		const FSlateRect& InClippingRect,
		FSlateWindowElementList& InDrawElements,
		UEdGraph* InGraphObj) const override;
};
