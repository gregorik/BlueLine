// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintConnectionDrawingPolicy.h"
#include "Data/UBlueLineThemeData.h"
#include "Layout/SlateRect.h" 
#include "Runtime/Launch/Resources/Version.h"

#include "BlueLineCore/Public/Settings/UBlueLineEditorSettings.h"
class FSlateWindowElementList;
class UEdGraph;

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
using FBlueLineGraphPoint = FVector2f;
#else
using FBlueLineGraphPoint = FVector2D;
#endif

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
#define BLUELINE_HAS_SLICE_LINE_OVERRIDE 1
#else
#define BLUELINE_HAS_SLICE_LINE_OVERRIDE 0
#endif

struct FBlueLineRoutedWire
{
	FBlueLineGraphPoint StartPoint;
	FBlueLineGraphPoint EndPoint;
	FConnectionParams Params;
	TArray<FBlueLineGraphPoint> PathPoints;
};

struct FBlueLineSegment
{
	FBlueLineGraphPoint Start;
	FBlueLineGraphPoint End;
	bool bIsHorizontal;
	int32 WireIndex;
	int32 SegmentIndex;
};

/**
 * FBlueLineConnectionPolicy
 *
 * Standalone connection drawing logic.
 */
class FBlueLineConnectionPolicy : public FKismetConnectionDrawingPolicy
{
public:
	typedef FKismetConnectionDrawingPolicy Super;

	FBlueLineConnectionPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj, const UBlueLineEditorSettings* InSettings);

	virtual void Draw(TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries, FArrangedChildren& ArrangedNodes) override;
	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params) override;
	virtual void DrawConnection(int32 LayerId, const FBlueLineGraphPoint& Start, const FBlueLineGraphPoint& End, const FConnectionParams& Params) override;
	virtual void DrawSplineWithArrow(const FBlueLineGraphPoint& StartPoint, const FBlueLineGraphPoint& EndPoint, const FConnectionParams& Params) override;
#if BLUELINE_HAS_SLICE_LINE_OVERRIDE
	virtual bool DrawSliceLine() override;
#endif

private:
	void ComputeManhattanPath(const FBlueLineGraphPoint& Start, const FBlueLineGraphPoint& End, TArray<FBlueLineGraphPoint>& OutPoints) const;
	void DrawStableLineStrip(const TArray<FBlueLineGraphPoint>& Points, const FLinearColor& Color, float Thickness, int32 LayerId) const;
	void DrawWirePolylineWithGhosting(const TArray<FBlueLineGraphPoint>& Points, const FLinearColor& Color, float Thickness) const;
	void DrawGhostSegment(const FBlueLineGraphPoint& Start, const FBlueLineGraphPoint& End, const FLinearColor& Color, float Thickness) const;

	void ProcessAndDrawWires();
	void DrawSimpleManhattanWire(const TArray<FBlueLineGraphPoint>& PathPoints, const FConnectionParams& Params, bool bIsHovered);
	void DrawConnectionBadges();
	void DrawConnectionBadge(UEdGraphPin* Pin, const FBlueLineGraphPoint& AnchorPoint);
	void ApplyNodeAvoidance(TArray<FBlueLineGraphPoint>& PathPoints);
	void ApplyBusRouting();
	void DrawPathWithCrossings(const FBlueLineRoutedWire& Wire, bool bIsHovered);

	bool bIsCollectingWires = false;
	bool bUseSimplifiedRouting = false;
	TArray<FBlueLineRoutedWire> CollectedWires;
	TArray<FSlateRect> CachedNodeBounds;

	const UBlueLineEditorSettings* Settings;
	const UBlueLineThemeData* ThemeData;
	FBlueLineWireSettings WireSettings;

	float LocalZoomFactor = 1.0f;
	int32 CachedLayerId = 0;
};
