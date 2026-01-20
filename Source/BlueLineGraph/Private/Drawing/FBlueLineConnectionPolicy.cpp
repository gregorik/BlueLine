// Copyright YourTeamName. All Rights Reserved.

#include "Drawing/FBlueLineConnectionPolicy.h"
#include "Settings/UBlueLineEditorSettings.h"
#include "Debug/BlueLineDebugLib.h"
#include "Rendering/DrawElements.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h" 
#include "Layout/SlateRect.h" 
#include "Layout/PaintGeometry.h" 

FBlueLineConnectionPolicy::FBlueLineConnectionPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, const UBlueLineEditorSettings* InSettings)
	: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
	, Settings(InSettings)
	, LocalZoomFactor(InZoomFactor)
{
	ThemeData = UBlueLineDebugLib::GetActiveThemeData();
	if (ThemeData)
	{
		WireSettings = ThemeData->WireSettings;
	}
	else
	{
		WireSettings.WireThickness = 2.5f;
		WireSettings.BubbleSize = FVector2D(10.f, 10.f);
	}
}

void FBlueLineConnectionPolicy::DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, FConnectionParams& Params)
{
	Super::DetermineWiringStyle(OutputPin, InputPin, Params);
	Params.WireThickness = WireSettings.WireThickness;
}

void FBlueLineConnectionPolicy::DrawConnection(int32 LayerId, const FVector2f& Start, const FVector2f& End, const FConnectionParams& Params)
{
	CachedLayerId = LayerId;
	Super::DrawConnection(LayerId, Start, End, Params);
}

void FBlueLineConnectionPolicy::DrawSplineWithArrow(const FVector2f& StartPoint, const FVector2f& EndPoint, const FConnectionParams& Params)
{
	if (Settings && !Settings->bEnableManhattanRouting)
	{
		Super::DrawSplineWithArrow(StartPoint, EndPoint, Params);
		return;
	}

	TArray<FVector2f> PathPoints;
	ComputeManhattanPath(StartPoint, EndPoint, PathPoints);

	if (PathPoints.Num() < 2) return;

	FSlateDrawElement::MakeLines(
		DrawElementsList,
		CachedLayerId,
		FPaintGeometry(),
		PathPoints,
		ESlateDrawEffect::None,
		Params.WireColor,
		true,
		(float)(Params.WireThickness * LocalZoomFactor)
	);

	if (Params.bDrawBubbles)
	{
		const FVector2f LastPoint = PathPoints.Last();
		const FVector2f PrevPoint = PathPoints[PathPoints.Num() - 2];

		FVector2f Dir = (LastPoint - PrevPoint).GetSafeNormal();

		FVector2f ArrowPos = LastPoint - (Dir * (10.0f * LocalZoomFactor));

		const FSlateBrush* Brush = ArrowImage;
		if (!Brush) return;

		FVector2f BrushSize((float)Brush->ImageSize.X, (float)Brush->ImageSize.Y);
		FVector2f ScaledSize = BrushSize * LocalZoomFactor;

		// UE 5.7 Strict Geometry
		FPaintGeometry ArrowGeometry(
			ArrowPos,
			ScaledSize,
			1.0f
		);

		FSlateDrawElement::MakeRotatedBox(
			DrawElementsList,
			CachedLayerId,
			ArrowGeometry,
			Brush,
			ESlateDrawEffect::None,
			(float)FMath::Atan2(Dir.Y, Dir.X),
			TOptional<FVector2f>(),
			FSlateDrawElement::RelativeToElement,
			Params.WireColor
		);
	}
}

void FBlueLineConnectionPolicy::ComputeManhattanPath(const FVector2f& Start, const FVector2f& End, TArray<FVector2f>& OutPoints) const
{
	OutPoints.Reset();
	OutPoints.Add(Start);

	const float StubLength = 20.0f * LocalZoomFactor;
	FVector2f StartStub = Start + FVector2f(StubLength, 0.0f);
	FVector2f EndStub = End - FVector2f(StubLength, 0.0f);

	OutPoints.Add(StartStub);

	if (StartStub.X < EndStub.X)
	{
		float MidX = (StartStub.X + EndStub.X) * 0.5f;
		OutPoints.Add(FVector2f(MidX, StartStub.Y));
		OutPoints.Add(FVector2f(MidX, EndStub.Y));
	}
	else
	{
		float MidY = FMath::Max(StartStub.Y, EndStub.Y) + (50.0f * LocalZoomFactor);
		OutPoints.Add(FVector2f(StartStub.X, MidY));
		float BacktrackX = EndStub.X - (StubLength * 2.0f);
		OutPoints.Add(FVector2f(BacktrackX, MidY));
		OutPoints.Add(FVector2f(BacktrackX, EndStub.Y));
	}

	OutPoints.Add(EndStub);
	OutPoints.Add(End);
}