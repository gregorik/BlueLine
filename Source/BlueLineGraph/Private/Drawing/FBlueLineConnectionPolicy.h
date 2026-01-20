// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConnectionDrawingPolicy.h"
#include "Data/UBlueLineThemeData.h"
#include "Layout/SlateRect.h" 

class UBlueLineEditorSettings;
class FSlateWindowElementList;

/**
 * FBlueLineConnectionPolicy
 *
 * Standalone connection drawing logic.
 * Usage: Return a new instance of this from your custom Schema's CreateConnectionDrawingPolicy().
 */
class FBlueLineConnectionPolicy : public FConnectionDrawingPolicy
{
public:
	typedef FConnectionDrawingPolicy Super;

	/**
	 * UE 5.7 Constructor.
	 * Argument list matches standard schema call, plus our custom Settings.
	 */
	FBlueLineConnectionPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, const UBlueLineEditorSettings* InSettings);

	// Interface Overrides
	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params) override;
	virtual void DrawConnection(int32 LayerId, const FVector2f& Start, const FVector2f& End, const FConnectionParams& Params) override;
	virtual void DrawSplineWithArrow(const FVector2f& StartPoint, const FVector2f& EndPoint, const FConnectionParams& Params) override;

private:
	void ComputeManhattanPath(const FVector2f& Start, const FVector2f& End, TArray<FVector2f>& OutPoints) const;

	const UBlueLineEditorSettings* Settings;
	const UBlueLineThemeData* ThemeData;
	FBlueLineWireSettings WireSettings;

	// Caching these locally to ensure access during draw calls
	float LocalZoomFactor = 1.0f;
	int32 CachedLayerId = 0;
};