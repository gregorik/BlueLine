// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "Drawing/FBlueLineGraphConnectionFactory.h"

#include "Drawing/FBlueLineConnectionPolicy.h"
#include "EdGraphSchema_K2.h"
#include "Settings/UBlueLineEditorSettings.h"

FConnectionDrawingPolicy* FBlueLineGraphConnectionFactory::CreateConnectionPolicy(
	const UEdGraphSchema* Schema,
	int32 InBackLayerID,
	int32 InFrontLayerID,
	float ZoomFactor,
	const FSlateRect& InClippingRect,
	FSlateWindowElementList& InDrawElements,
	UEdGraph* InGraphObj) const
{
	const UBlueLineEditorSettings* Settings = UBlueLineEditorSettings::Get();
	if (!Settings || !Settings->bEnableBlueLine)
	{
		return nullptr;
	}

	if (!Schema || !Schema->IsA(UEdGraphSchema_K2::StaticClass()))
	{
		return nullptr;
	}

	return new FBlueLineConnectionPolicy(
		InBackLayerID,
		InFrontLayerID,
		ZoomFactor,
		InClippingRect,
		InDrawElements,
		InGraphObj,
		Settings);
}
