// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "Drawing/FBlueLineConnectionPolicy.h"
#include "BlueLineCore/Public/Settings/UBlueLineEditorSettings.h"
#include "Debug/BlueLineDebugLib.h"
#include "Rendering/DrawElements.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h" 
#include "Layout/SlateRect.h" 
#include "Layout/PaintGeometry.h" 
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "K2Node_Knot.h"

namespace
{
	// Advanced routing does quadratic work across collected wires.
	// Fall back to a cheap per-wire path for large graphs to keep memory stable.
	constexpr int32 MaxAdvancedRoutingNodes = 150;
	constexpr int32 MaxAdvancedRoutingPins = 900;
	constexpr float StableSegmentMergeDistanceSq = 0.25f;

	bool IsFiniteGraphPoint(const FBlueLineGraphPoint& Point)
	{
		return FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y);
	}

	// Liang-Barsky clip of segment A->B against an axis-aligned rect.
	// Returns true and the [OutEnter, OutExit] parametric range (within [0,1]) that lies inside the rect.
	bool ClipSegmentToRect(const FBlueLineGraphPoint& A, const FBlueLineGraphPoint& B, const FSlateRect& Rect, float& OutEnter, float& OutExit)
	{
		float T0 = 0.0f;
		float T1 = 1.0f;
		const float Dx = (float)(B.X - A.X);
		const float Dy = (float)(B.Y - A.Y);

		const float P[4] = { -Dx, Dx, -Dy, Dy };
		const float Q[4] = { (float)(A.X - Rect.Left), (float)(Rect.Right - A.X), (float)(A.Y - Rect.Top), (float)(Rect.Bottom - A.Y) };

		for (int32 Edge = 0; Edge < 4; ++Edge)
		{
			if (FMath::IsNearlyZero(P[Edge]))
			{
				if (Q[Edge] < 0.0f)
				{
					return false; // Parallel to this edge and entirely outside it.
				}
			}
			else
			{
				const float R = Q[Edge] / P[Edge];
				if (P[Edge] < 0.0f)
				{
					T0 = FMath::Max(T0, R);
				}
				else
				{
					T1 = FMath::Min(T1, R);
				}
			}
		}

		if (T0 > T1)
		{
			return false;
		}

		OutEnter = T0;
		OutExit = T1;
		return true;
	}
}

FBlueLineConnectionPolicy::FBlueLineConnectionPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj, const UBlueLineEditorSettings* InSettings)
	: FKismetConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj)
	, Settings(InSettings)
	, LocalZoomFactor(InZoomFactor)
{
	CachedLayerId = WireLayerID;

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

void FBlueLineConnectionPolicy::Draw(TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries, FArrangedChildren& ArrangedNodes)
{
	CachedLayerId = WireLayerID;

	bUseSimplifiedRouting =
		Settings &&
		Settings->RoutingMethod != EBlueLineRoutingMethod::Curved &&
		(ArrangedNodes.Num() > MaxAdvancedRoutingNodes || InPinGeometries.Num() > MaxAdvancedRoutingPins);

	if (bUseSimplifiedRouting)
	{
		CachedNodeBounds.Reset();
		CollectedWires.Reset();
		bIsCollectingWires = false;
		Super::Draw(InPinGeometries, ArrangedNodes);
		return;
	}

	// Cache Node Bounds for Avoidance
	CachedNodeBounds.Reset();
	CachedNodeBounds.Reserve(ArrangedNodes.Num());
	for (int32 NodeIndex = 0; NodeIndex < ArrangedNodes.Num(); ++NodeIndex)
	{
		FArrangedWidget& Widget = ArrangedNodes[NodeIndex];
		CachedNodeBounds.Add(Widget.Geometry.GetRenderBoundingRect());
	}

	bIsCollectingWires = true;
	CollectedWires.Reset();
	CollectedWires.Reserve(FMath::Max(1, InPinGeometries.Num() / 2));

	// Let standard UE processing calculate pins and invoke our DrawSplineWithArrow override
	Super::Draw(InPinGeometries, ArrangedNodes);

	bIsCollectingWires = false;

	// Process routing intelligence
	ProcessAndDrawWires();
}

void FBlueLineConnectionPolicy::DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, FConnectionParams& Params)
{
	// Super (FKismetConnectionDrawingPolicy) sets Params.WireColor from the pin
	// type and Params.WireThickness to the engine's per-connection value
	// (thin data, thicker exec/containers). We keep the color untouched and only
	// re-map thickness according to the user's WireStyle preference.
	Super::DetermineWiringStyle(OutputPin, InputPin, Params);

	// Preserve the engine's type-based thickness so "Dynamic" can fall back to it.
	const float TypeBasedThickness = Params.WireThickness;

	float BaseThickness = WireSettings.WireThickness;
	if (Settings)
	{
		switch (Settings->WireStyle)
		{
		case EBlueLineWireStyle::Thin:
			BaseThickness = 1.5f;
			break;
		case EBlueLineWireStyle::Medium:
			BaseThickness = 2.5f;
			break;
		case EBlueLineWireStyle::Thick:
			BaseThickness = 4.0f;
			break;
		case EBlueLineWireStyle::Dynamic:
			// Type-based: honor the per-connection thickness the engine just
			// computed (data vs. execution vs. container wires) instead of a
			// flat constant. Color is already type-based via Super.
			BaseThickness = TypeBasedThickness;
			break;
		}
		BaseThickness *= Settings->WireThicknessMultiplier;
	}
	Params.WireThickness = BaseThickness;
}

void FBlueLineConnectionPolicy::DrawConnection(int32 LayerId, const FBlueLineGraphPoint& Start, const FBlueLineGraphPoint& End, const FConnectionParams& Params)
{
	CachedLayerId = LayerId;
	Super::DrawConnection(LayerId, Start, End, Params);
}

void FBlueLineConnectionPolicy::DrawStableLineStrip(const TArray<FBlueLineGraphPoint>& Points, const FLinearColor& Color, float Thickness, int32 LayerId) const
{
	if (Points.Num() < 2)
	{
		return;
	}

	TArray<FBlueLineGraphPoint> SanitizedPoints;
	SanitizedPoints.Reserve(Points.Num());

	for (const FBlueLineGraphPoint& Point : Points)
	{
		if (!IsFiniteGraphPoint(Point))
		{
			continue;
		}

		if (SanitizedPoints.Num() == 0 || (Point - SanitizedPoints.Last()).SizeSquared() > StableSegmentMergeDistanceSq)
		{
			SanitizedPoints.Add(Point);
		}
	}

	if (SanitizedPoints.Num() < 2)
	{
		return;
	}

	TArray<FBlueLineGraphPoint> SegmentPoints;
	SegmentPoints.SetNumUninitialized(2);

	// Slate line joins on anti-aliased polylines are unstable in graph space.
	// Draw each segment explicitly to keep orthogonal/circuit routes predictable.
	for (int32 PointIndex = 0; PointIndex < SanitizedPoints.Num() - 1; ++PointIndex)
	{
		const FBlueLineGraphPoint& SegmentStart = SanitizedPoints[PointIndex];
		const FBlueLineGraphPoint& SegmentEnd = SanitizedPoints[PointIndex + 1];
		if ((SegmentEnd - SegmentStart).SizeSquared() <= StableSegmentMergeDistanceSq)
		{
			continue;
		}

		SegmentPoints[0] = SegmentStart;
		SegmentPoints[1] = SegmentEnd;

		FSlateDrawElement::MakeLines(
			DrawElementsList,
			LayerId,
			FPaintGeometry(),
			SegmentPoints,
			ESlateDrawEffect::None,
			Color,
			false,
			Thickness
		);
	}
}

void FBlueLineConnectionPolicy::DrawGhostSegment(const FBlueLineGraphPoint& Start, const FBlueLineGraphPoint& End, const FLinearColor& Color, float Thickness) const
{
	if ((End - Start).SizeSquared() <= StableSegmentMergeDistanceSq)
	{
		return;
	}

	TArray<FBlueLineGraphPoint> SegmentPoints;
	SegmentPoints.Add(Start);
	SegmentPoints.Add(End);

	FSlateDrawElement::MakeLines(
		DrawElementsList,
		CachedLayerId,
		FPaintGeometry(),
		SegmentPoints,
		ESlateDrawEffect::None,
		Color,
		false,
		Thickness
	);
}

void FBlueLineConnectionPolicy::DrawWirePolylineWithGhosting(const TArray<FBlueLineGraphPoint>& Points, const FLinearColor& Color, float Thickness) const
{
	if (Points.Num() < 2)
	{
		return;
	}

	const float GhostOpacity = FMath::Clamp(WireSettings.GhostWireOpacity, 0.1f, 1.0f);

	// No dimming requested (or nothing to dim against): draw as a single plain strip.
	if (GhostOpacity >= 0.999f || CachedNodeBounds.Num() == 0)
	{
		DrawStableLineStrip(Points, Color, Thickness, CachedLayerId);
		return;
	}

	FLinearColor GhostColor = Color;
	GhostColor.A *= GhostOpacity;

	for (int32 PointIndex = 0; PointIndex < Points.Num() - 1; ++PointIndex)
	{
		const FBlueLineGraphPoint SegStart = Points[PointIndex];
		const FBlueLineGraphPoint SegEnd = Points[PointIndex + 1];

		if (!IsFiniteGraphPoint(SegStart) || !IsFiniteGraphPoint(SegEnd))
		{
			continue;
		}
		if ((SegEnd - SegStart).SizeSquared() <= StableSegmentMergeDistanceSq)
		{
			continue;
		}

		// Collect the parametric intervals of this segment that fall inside a node.
		TArray<TPair<float, float>, TInlineAllocator<8>> InsideIntervals;
		for (const FSlateRect& Bounds : CachedNodeBounds)
		{
			float Enter = 0.0f;
			float Exit = 0.0f;
			if (ClipSegmentToRect(SegStart, SegEnd, Bounds, Enter, Exit) && Exit > Enter + KINDA_SMALL_NUMBER)
			{
				InsideIntervals.Add(TPair<float, float>(FMath::Clamp(Enter, 0.0f, 1.0f), FMath::Clamp(Exit, 0.0f, 1.0f)));
			}
		}

		if (InsideIntervals.Num() == 0)
		{
			DrawGhostSegment(SegStart, SegEnd, Color, Thickness);
			continue;
		}

		InsideIntervals.Sort([](const TPair<float, float>& A, const TPair<float, float>& B)
		{
			return A.Key < B.Key;
		});

		// Merge overlapping inside-intervals so shared boundaries do not double-draw.
		TArray<TPair<float, float>, TInlineAllocator<8>> Merged;
		for (const TPair<float, float>& Interval : InsideIntervals)
		{
			if (Merged.Num() > 0 && Interval.Key <= Merged.Last().Value)
			{
				Merged.Last().Value = FMath::Max(Merged.Last().Value, Interval.Value);
			}
			else
			{
				Merged.Add(Interval);
			}
		}

		// Emit the outside portions at full opacity and the inside (behind-node) portions dimmed.
		float Cursor = 0.0f;
		for (const TPair<float, float>& Interval : Merged)
		{
			if (Interval.Key > Cursor + KINDA_SMALL_NUMBER)
			{
				DrawGhostSegment(
					SegStart + (SegEnd - SegStart) * Cursor,
					SegStart + (SegEnd - SegStart) * Interval.Key,
					Color, Thickness);
			}

			DrawGhostSegment(
				SegStart + (SegEnd - SegStart) * Interval.Key,
				SegStart + (SegEnd - SegStart) * Interval.Value,
				GhostColor, Thickness);

			Cursor = Interval.Value;
		}

		if (Cursor < 1.0f - KINDA_SMALL_NUMBER)
		{
			DrawGhostSegment(
				SegStart + (SegEnd - SegStart) * Cursor,
				SegEnd,
				Color, Thickness);
		}
	}
}

void FBlueLineConnectionPolicy::DrawSplineWithArrow(const FBlueLineGraphPoint& StartPoint, const FBlueLineGraphPoint& EndPoint, const FConnectionParams& Params)
{
	// Feature 3: Named Reroutes (Wireless Data Visual Trick)
	// If the pins belong to standard Knot nodes and share the exact same NodeComment,
	// we do not draw the wire, effectively creating "wireless" data routing.
	if (Params.AssociatedPin1 && Params.AssociatedPin2)
	{
		UK2Node_Knot* Knot1 = Cast<UK2Node_Knot>(Params.AssociatedPin1->GetOwningNode());
		UK2Node_Knot* Knot2 = Cast<UK2Node_Knot>(Params.AssociatedPin2->GetOwningNode());

		if (Knot1 && Knot2)
		{
			if (!Knot1->NodeComment.IsEmpty() && Knot1->NodeComment.Equals(Knot2->NodeComment, ESearchCase::IgnoreCase))
			{
				return; // Skip drawing this wire completely
			}
		}
	}
	
	if (Settings && Settings->RoutingMethod == EBlueLineRoutingMethod::Curved)
	{
		Super::DrawSplineWithArrow(StartPoint, EndPoint, Params);
		return;
	}

	const bool bIsHovered =
		(Params.AssociatedPin1 != nullptr && HoveredPins.Contains(Params.AssociatedPin1)) ||
		(Params.AssociatedPin2 != nullptr && HoveredPins.Contains(Params.AssociatedPin2));

	if (bUseSimplifiedRouting)
	{
		TArray<FBlueLineGraphPoint> PathPoints;
		PathPoints.Reserve(6);
		ComputeManhattanPath(StartPoint, EndPoint, PathPoints);
		DrawSimpleManhattanWire(PathPoints, Params, bIsHovered);
		return;
	}

	if (bIsCollectingWires)
	{
		FBlueLineRoutedWire Wire;
		Wire.StartPoint = StartPoint;
		Wire.EndPoint = EndPoint;
		Wire.Params = Params;
		ComputeManhattanPath(StartPoint, EndPoint, Wire.PathPoints);
		CollectedWires.Add(Wire);
		return;
	}

	// Defensive fallback for preview/relink paths or engine-side draw flow changes
	// that call the vector override outside BlueLine's collection pass.
	Super::DrawSplineWithArrow(StartPoint, EndPoint, Params);
}

void FBlueLineConnectionPolicy::DrawSimpleManhattanWire(const TArray<FBlueLineGraphPoint>& PathPoints, const FConnectionParams& Params, bool bIsHovered)
{
	if (PathPoints.Num() < 2)
	{
		return;
	}

	FLinearColor FinalColor = Params.WireColor;
	float FinalThickness = Params.WireThickness * LocalZoomFactor;

	if (Settings && Settings->bHighlightWiresOnHover && HoveredPins.Num() > 0)
	{
		if (!bIsHovered)
		{
			FinalColor.A *= 0.2f;
			FinalColor = FinalColor * 0.5f;
		}
		else
		{
			const float HighlightIntensity = FMath::Max(Settings->WireHighlightIntensity, 1.0f);
			FinalColor = FinalColor * HighlightIntensity;
			FinalThickness *= HighlightIntensity;
		}
	}

	DrawWirePolylineWithGhosting(PathPoints, FinalColor, FinalThickness);

	// Overdraw the pin-exit stubs (first and last routed segments) at StubThickness so the
	// lead-in/out from each pin reads distinctly from the wire body.
	if (Settings && PathPoints.Num() >= 2)
	{
		const float StubThickness = Settings->StubThickness * LocalZoomFactor;
		if (StubThickness > 0.0f && !FMath::IsNearlyEqual(StubThickness, FinalThickness))
		{
			const int32 LastIndex = PathPoints.Num() - 1;
			const TArray<FBlueLineGraphPoint> StartStub = { PathPoints[0], PathPoints[1] };
			const TArray<FBlueLineGraphPoint> EndStub = { PathPoints[LastIndex - 1], PathPoints[LastIndex] };
			DrawWirePolylineWithGhosting(StartStub, FinalColor, StubThickness);
			DrawWirePolylineWithGhosting(EndStub, FinalColor, StubThickness);
		}
	}

	if (!Params.bDrawBubbles || PathPoints.Num() < 2)
	{
		return;
	}

	const FBlueLineGraphPoint LastPoint = PathPoints.Last();
	const FBlueLineGraphPoint PrevPoint = PathPoints[PathPoints.Num() - 2];
	const FBlueLineGraphPoint Dir = (LastPoint - PrevPoint).GetSafeNormal();
	const FBlueLineGraphPoint ArrowPos = LastPoint - (Dir * (10.0f * LocalZoomFactor));

	const FSlateBrush* Brush = ArrowImage;
	if (!Brush)
	{
		return;
	}

	const FBlueLineGraphPoint BrushSize((float)WireSettings.BubbleSize.X, (float)WireSettings.BubbleSize.Y);
	const FBlueLineGraphPoint ScaledSize = BrushSize * LocalZoomFactor;

	FSlateDrawElement::MakeRotatedBox(
		DrawElementsList,
		ArrowLayerID,
		FPaintGeometry(ArrowPos, ScaledSize, 1.0f),
		Brush,
		ESlateDrawEffect::None,
		(float)FMath::Atan2(Dir.Y, Dir.X),
		TOptional<FVector2f>(),
		FSlateDrawElement::RelativeToElement,
		FinalColor
	);
}

void FBlueLineConnectionPolicy::ComputeManhattanPath(const FBlueLineGraphPoint& Start, const FBlueLineGraphPoint& End, TArray<FBlueLineGraphPoint>& OutPoints) const
{
	OutPoints.Reset();
	OutPoints.Add(Start);

	const float StubLength = (Settings ? Settings->StubLength : 20.0f) * LocalZoomFactor;
	const float VerticalOffset = (Settings ? Settings->VerticalOffset : 80.0f) * LocalZoomFactor;
	FBlueLineGraphPoint StartStub = Start + FBlueLineGraphPoint(StubLength, 0.0f);
	FBlueLineGraphPoint EndStub = End - FBlueLineGraphPoint(StubLength, 0.0f);

	OutPoints.Add(StartStub);

	if (StartStub.X < EndStub.X)
	{
		float MidX = (StartStub.X + EndStub.X) * 0.5f;
		OutPoints.Add(FBlueLineGraphPoint(MidX, StartStub.Y));
		OutPoints.Add(FBlueLineGraphPoint(MidX, EndStub.Y));
	}
	else
	{
		float MidY = FMath::Max(StartStub.Y, EndStub.Y) + VerticalOffset;
		OutPoints.Add(FBlueLineGraphPoint(StartStub.X, MidY));
		float BacktrackX = EndStub.X - (StubLength * 2.0f);
		OutPoints.Add(FBlueLineGraphPoint(BacktrackX, MidY));
		OutPoints.Add(FBlueLineGraphPoint(BacktrackX, EndStub.Y));
	}
	OutPoints.Add(EndStub);
	OutPoints.Add(End);
}

void FBlueLineConnectionPolicy::ProcessAndDrawWires()
{
	// The ApplyNodeAvoidance algorithm creates massive spaghetti detours for dense graphs.
	// Disable it entirely to keep all orthogonal routing styles clean and direct.
	bool bUseAvoidance = false; 

	if (bUseAvoidance)
	{
		for (FBlueLineRoutedWire& Wire : CollectedWires)
		{
			ApplyNodeAvoidance(Wire.PathPoints);
		}
	}

	// Always apply Bus Routing to separate overlapping wires!
	ApplyBusRouting();

	for (const FBlueLineRoutedWire& Wire : CollectedWires)
	{
		// Feature 5: Advanced Visual Polish & Flow Feedback
		// We can check if the wire is connected to a hovered pin for emphasis
		bool bIsHovered = false;
		if (Wire.Params.AssociatedPin1 != nullptr && HoveredPins.Contains(Wire.Params.AssociatedPin1)) bIsHovered = true;
		if (Wire.Params.AssociatedPin2 != nullptr && HoveredPins.Contains(Wire.Params.AssociatedPin2)) bIsHovered = true;

		DrawPathWithCrossings(Wire, bIsHovered);
	}

	DrawConnectionBadges();
}

void FBlueLineConnectionPolicy::DrawConnectionBadges()
{
	if (!Settings || !Settings->bShowConnectionCount)
	{
		return;
	}

	TSet<UEdGraphPin*> DrawnPins;

	for (const FBlueLineRoutedWire& Wire : CollectedWires)
	{
		if (Wire.Params.AssociatedPin1 && Wire.Params.AssociatedPin1->LinkedTo.Num() > 1 && !DrawnPins.Contains(Wire.Params.AssociatedPin1))
		{
			DrawConnectionBadge(Wire.Params.AssociatedPin1, Wire.StartPoint);
			DrawnPins.Add(Wire.Params.AssociatedPin1);
		}

		if (Wire.Params.AssociatedPin2 && Wire.Params.AssociatedPin2->LinkedTo.Num() > 1 && !DrawnPins.Contains(Wire.Params.AssociatedPin2))
		{
			DrawConnectionBadge(Wire.Params.AssociatedPin2, Wire.EndPoint);
			DrawnPins.Add(Wire.Params.AssociatedPin2);
		}
	}
}

void FBlueLineConnectionPolicy::DrawConnectionBadge(UEdGraphPin* Pin, const FBlueLineGraphPoint& AnchorPoint)
{
	if (!Pin)
	{
		return;
	}

	const FBlueLineGraphPoint BadgeSize(16.0f * LocalZoomFactor, 16.0f * LocalZoomFactor);
	const float HorizontalOffset = (Pin->Direction == EGPD_Output) ? 6.0f : -22.0f;
	const FBlueLineGraphPoint BadgePos = AnchorPoint + FBlueLineGraphPoint(HorizontalOffset * LocalZoomFactor, -8.0f * LocalZoomFactor);

	const FSlateBrush* BadgeBrush = FAppStyle::GetBrush("Graph.Node.Body");
	const UEdGraphSchema* Schema = Pin->GetSchema();
	const FLinearColor BadgeColor = Schema ? Schema->GetPinTypeColor(Pin->PinType) : FLinearColor::Gray;

	FSlateDrawElement::MakeBox(
		DrawElementsList,
		CachedLayerId + 5,
		FPaintGeometry(BadgePos, BadgeSize, 1.0f),
		BadgeBrush,
		ESlateDrawEffect::None,
		BadgeColor
	);

	const FString CountText = FString::FromInt(Pin->LinkedTo.Num());
	const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Bold", (uint16)FMath::Max(7.0f, 7.0f * LocalZoomFactor));

	FSlateDrawElement::MakeText(
		DrawElementsList,
		CachedLayerId + 6,
		FPaintGeometry(BadgePos + FBlueLineGraphPoint(5.0f * LocalZoomFactor, 2.0f * LocalZoomFactor), BadgeSize, 1.0f),
		CountText,
		FontInfo,
		ESlateDrawEffect::None,
		FLinearColor::Black
	);
}

#if BLUELINE_HAS_SLICE_LINE_OVERRIDE
bool FBlueLineConnectionPolicy::DrawSliceLine()
{
	// Feature 6: Technical UI Integration (Slice Line geometric intersections)
	if (SliceLine.IsValid())
	{
		TArray<FBlueLineGraphPoint> LinePoints;
		LinePoints.Add(SliceLine.StartPoint);
		LinePoints.Add(SliceLine.EndPoint);

		FSlateDrawElement::MakeLines(
			DrawElementsList,
			WireLayerID,
			FPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			FLinearColor::Red, // Slice line color
			true,
			2.0f * LocalZoomFactor
		);

		// Intersect our custom Manhattan points against the Slice Line
		for (const FBlueLineRoutedWire& Wire : CollectedWires)
		{
			bool bIntersects = false;
			for (int32 i = 0; i < Wire.PathPoints.Num() - 1; ++i)
			{
				FBlueLineGraphPoint P1 = Wire.PathPoints[i];
				FBlueLineGraphPoint P2 = Wire.PathPoints[i+1];

				FVector Intersect3D;
				if (FMath::SegmentIntersection2D(FVector(P1.X, P1.Y, 0.0), FVector(P2.X, P2.Y, 0.0), FVector(SliceLine.StartPoint.X, SliceLine.StartPoint.Y, 0.0), FVector(SliceLine.EndPoint.X, SliceLine.EndPoint.Y, 0.0), Intersect3D))
				{
					bIntersects = true;
					break;
				}
			}

			if (bIntersects)
			{
				// Register the intersection so the Engine knows to cut it
				if (Wire.Params.AssociatedPin1 != nullptr && Wire.Params.AssociatedPin2 != nullptr)
				{
					ConnectionsIntersectingSliceLine.Emplace(Wire.Params.AssociatedPin1, Wire.Params.AssociatedPin2);
				}
			}
		}
		return true; // We handled drawing it
	}
	return false;
}
#endif

void FBlueLineConnectionPolicy::ApplyNodeAvoidance(TArray<FBlueLineGraphPoint>& PathPoints){
	float Padding = 24.0f * LocalZoomFactor;

	bool bMoved = true;
	int32 MaxIters = 50;
	while (bMoved && MaxIters-- > 0)
	{
		bMoved = false;
		for (int32 i = 0; i < PathPoints.Num() - 1; ++i)
		{
			FBlueLineGraphPoint P1 = PathPoints[i];
			FBlueLineGraphPoint P2 = PathPoints[i+1];
			
			bool bIsHorizontal = FMath::IsNearlyEqual(P1.Y, P2.Y, 1.0f);
			bool bIsVertical = FMath::IsNearlyEqual(P1.X, P2.X, 1.0f);
			
			if (!bIsHorizontal && !bIsVertical) continue;

			for (const FSlateRect& Bounds : CachedNodeBounds)
			{
				FSlateRect PaddedBounds = Bounds.ExtendBy(Padding);
				
				if (bIsHorizontal)
				{
					float MinX = FMath::Min(P1.X, P2.X);
					float MaxX = FMath::Max(P1.X, P2.X);
					
					if (P1.Y > PaddedBounds.Top && P1.Y < PaddedBounds.Bottom &&
						MinX < PaddedBounds.Right && MaxX > PaddedBounds.Left)
					{
						float DistToTop = P1.Y - PaddedBounds.Top;
						float DistToBottom = PaddedBounds.Bottom - P1.Y;
						
						float DetourY = (DistToTop < DistToBottom) ? (PaddedBounds.Top - 1.0f) : (PaddedBounds.Bottom + 1.0f);
						
						float StartX = P1.X;
						float EndX = P2.X;
						float DirX = (EndX > StartX) ? 1.0f : -1.0f;
						
						float Cross1X = (DirX > 0) ? FMath::Max(StartX, PaddedBounds.Left) : FMath::Min(StartX, PaddedBounds.Right);
						float Cross2X = (DirX > 0) ? FMath::Min(EndX, PaddedBounds.Right) : FMath::Max(EndX, PaddedBounds.Left);
						
						if (FMath::Abs(Cross2X - Cross1X) > 1.0f)
						{
							PathPoints.Insert(FBlueLineGraphPoint(Cross1X, P1.Y), i + 1);
							PathPoints.Insert(FBlueLineGraphPoint(Cross1X, DetourY), i + 2);
							PathPoints.Insert(FBlueLineGraphPoint(Cross2X, DetourY), i + 3);
							PathPoints.Insert(FBlueLineGraphPoint(Cross2X, P1.Y), i + 4);
							bMoved = true;
							break;
						}
					}
				}
				else if (bIsVertical)
				{
					float MinY = FMath::Min(P1.Y, P2.Y);
					float MaxY = FMath::Max(P1.Y, P2.Y);
					
					if (P1.X > PaddedBounds.Left && P1.X < PaddedBounds.Right &&
						MinY < PaddedBounds.Bottom && MaxY > PaddedBounds.Top)
					{
						float DistToLeft = P1.X - PaddedBounds.Left;
						float DistToRight = PaddedBounds.Right - P1.X;
						
						float DetourX = (DistToLeft < DistToRight) ? (PaddedBounds.Left - 1.0f) : (PaddedBounds.Right + 1.0f);
						
						float StartY = P1.Y;
						float EndY = P2.Y;
						float DirY = (EndY > StartY) ? 1.0f : -1.0f;
						
						float Cross1Y = (DirY > 0) ? FMath::Max(StartY, PaddedBounds.Top) : FMath::Min(StartY, PaddedBounds.Bottom);
						float Cross2Y = (DirY > 0) ? FMath::Min(EndY, PaddedBounds.Bottom) : FMath::Max(EndY, PaddedBounds.Top);
						
						if (FMath::Abs(Cross2Y - Cross1Y) > 1.0f)
						{
							PathPoints.Insert(FBlueLineGraphPoint(P1.X, Cross1Y), i + 1);
							PathPoints.Insert(FBlueLineGraphPoint(DetourX, Cross1Y), i + 2);
							PathPoints.Insert(FBlueLineGraphPoint(DetourX, Cross2Y), i + 3);
							PathPoints.Insert(FBlueLineGraphPoint(P1.X, Cross2Y), i + 4);
							bMoved = true;
							break;
						}
					}
				}
			}
			if (bMoved) break;
		}
	}
	
	// Clean up collinear or duplicate points to ensure clean drawing
	for (int32 i = PathPoints.Num() - 2; i > 0; --i)
	{
		FBlueLineGraphPoint Prev = PathPoints[i-1];
		FBlueLineGraphPoint Curr = PathPoints[i];
		FBlueLineGraphPoint Next = PathPoints[i+1];
		
		FBlueLineGraphPoint Dir1 = (Curr - Prev).GetSafeNormal();
		FBlueLineGraphPoint Dir2 = (Next - Curr).GetSafeNormal();
		
		if (Dir1.Equals(Dir2, 0.05f) || (Curr - Prev).SizeSquared() < 1.0f || (Next - Curr).SizeSquared() < 1.0f)
		{
			PathPoints.RemoveAt(i);
		}
	}
}

void FBlueLineConnectionPolicy::ApplyBusRouting()
{
	float Spacing = 16.0f * LocalZoomFactor;

	bool bMoved = true;
	int32 MaxIterations = 50;
	while (bMoved && MaxIterations-- > 0)
	{
		bMoved = false;
		for (int32 i = 0; i < CollectedWires.Num(); ++i)
		{
			for (int32 j = i + 1; j < CollectedWires.Num(); ++j)
			{
				FBlueLineRoutedWire& WireA = CollectedWires[i];
				FBlueLineRoutedWire& WireB = CollectedWires[j];

				auto IsHorizontal = [](const FBlueLineGraphPoint& P1, const FBlueLineGraphPoint& P2) {
					return FMath::IsNearlyEqual(P1.Y, P2.Y, 1.0f);
				};
				auto IsVertical = [](const FBlueLineGraphPoint& P1, const FBlueLineGraphPoint& P2) {
					return FMath::IsNearlyEqual(P1.X, P2.X, 1.0f);
				};

				for (int32 sA = 1; sA < WireA.PathPoints.Num() - 2; ++sA)
				{
					FBlueLineGraphPoint& A1 = WireA.PathPoints[sA];
					FBlueLineGraphPoint& A2 = WireA.PathPoints[sA+1];

					if ((A1 - A2).SizeSquared() < 1.0f) continue;

					bool bAIsHoriz = IsHorizontal(A1, A2);
					bool bAIsVert = IsVertical(A1, A2);

					if (!bAIsHoriz && !bAIsVert) continue;

					for (int32 sB = 1; sB < WireB.PathPoints.Num() - 2; ++sB)
					{
						FBlueLineGraphPoint& B1 = WireB.PathPoints[sB];
						FBlueLineGraphPoint& B2 = WireB.PathPoints[sB+1];

						if ((B1 - B2).SizeSquared() < 1.0f) continue;

						bool bBIsHoriz = IsHorizontal(B1, B2);
						bool bBIsVert = IsVertical(B1, B2);

						if (!bBIsHoriz && !bBIsVert) continue;

						if (bAIsHoriz && bBIsHoriz)
						{
							if (FMath::Abs(A1.Y - B1.Y) < 2.0f)
							{
								float MinAX = FMath::Min(A1.X, A2.X);
								float MaxAX = FMath::Max(A1.X, A2.X);
								float MinBX = FMath::Min(B1.X, B2.X);
								float MaxBX = FMath::Max(B1.X, B2.X);

								if (MaxAX > MinBX && MaxBX > MinAX) // Overlap in X
								{
									bool bCanShiftB = IsVertical(WireB.PathPoints[sB-1], B1) && IsVertical(B2, WireB.PathPoints[sB+2]);
									bool bCanShiftA = IsVertical(WireA.PathPoints[sA-1], A1) && IsVertical(A2, WireA.PathPoints[sA+2]);
									
									if (bCanShiftB)
									{
										B1.Y += Spacing;
										B2.Y += Spacing;
										bMoved = true;
									}
									else if (bCanShiftA)
									{
										A1.Y += Spacing;
										A2.Y += Spacing;
										bMoved = true;
									}
								}
							}
						}
						else if (bAIsVert && bBIsVert)
						{
							if (FMath::Abs(A1.X - B1.X) < 2.0f)
							{
								float MinAY = FMath::Min(A1.Y, A2.Y);
								float MaxAY = FMath::Max(A1.Y, A2.Y);
								float MinBY = FMath::Min(B1.Y, B2.Y);
								float MaxBY = FMath::Max(B1.Y, B2.Y);

								if (MaxAY > MinBY && MaxBY > MinAY) // Overlap in Y
								{
									bool bCanShiftB = IsHorizontal(WireB.PathPoints[sB-1], B1) && IsHorizontal(B2, WireB.PathPoints[sB+2]);
									bool bCanShiftA = IsHorizontal(WireA.PathPoints[sA-1], A1) && IsHorizontal(A2, WireA.PathPoints[sA+2]);

									if (bCanShiftB)
									{
										B1.X += Spacing;
										B2.X += Spacing;
										bMoved = true;
									}
									else if (bCanShiftA)
									{
										A1.X += Spacing;
										A2.X += Spacing;
										bMoved = true;
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

void FBlueLineConnectionPolicy::DrawPathWithCrossings(const FBlueLineRoutedWire& Wire, bool bIsHovered)
{
	if (Wire.PathPoints.Num() < 2) return;

	float JumpRadius = 6.0f * LocalZoomFactor;
	float MaxCornerRadius = 12.0f * LocalZoomFactor;

	if (Settings)
	{
		if (Settings->RoutingMethod == EBlueLineRoutingMethod::Manhattan)
		{
			MaxCornerRadius = 3.0f * LocalZoomFactor;
			JumpRadius = 6.0f * LocalZoomFactor; 
		}
		else if (Settings->RoutingMethod == EBlueLineRoutingMethod::Circuit || Settings->RoutingMethod == EBlueLineRoutingMethod::Hybrid)
		{
			MaxCornerRadius = 12.0f * LocalZoomFactor;
		}
	}

	FLinearColor FinalColor = Wire.Params.WireColor;
	float FinalThickness = Wire.Params.WireThickness * LocalZoomFactor;

	if (Settings && Settings->bHighlightWiresOnHover && HoveredPins.Num() > 0)
	{
		if (!bIsHovered)
		{
			FinalColor.A *= 0.2f;
			FinalColor = FinalColor * 0.5f; 
		}
		else
		{
			const float HighlightIntensity = FMath::Max(Settings->WireHighlightIntensity, 1.0f);
			FinalColor = FinalColor * HighlightIntensity;
			FinalThickness *= HighlightIntensity;
		}
	}

	TArray<float> PointRadii;
	PointRadii.SetNumZeroed(Wire.PathPoints.Num());
	for (int32 i = 1; i < Wire.PathPoints.Num() - 1; ++i)
	{
		float PrevLen = (Wire.PathPoints[i] - Wire.PathPoints[i-1]).Size();
		float NextLen = (Wire.PathPoints[i+1] - Wire.PathPoints[i]).Size();
		PointRadii[i] = FMath::Min3(MaxCornerRadius, PrevLen * 0.5f, NextLen * 0.5f);
	}

	TArray<FBlueLineGraphPoint> FinalPoints;
	for (int32 i = 0; i < Wire.PathPoints.Num() - 1; ++i)
	{
		FBlueLineGraphPoint P1 = Wire.PathPoints[i];
		FBlueLineGraphPoint P2 = Wire.PathPoints[i+1];
		FBlueLineGraphPoint Dir = (P2 - P1).GetSafeNormal();
		float SegLen = (P2 - P1).Size();

		bool bIsHorizontal = FMath::IsNearlyEqual(P1.Y, P2.Y, 1.0f);

		TArray<FBlueLineGraphPoint> Intersections;
		for (const FBlueLineRoutedWire& OtherWire : CollectedWires)
		{
			if (&OtherWire == &Wire) continue;

			for (int32 j = 0; j < OtherWire.PathPoints.Num() - 1; ++j)
			{
				FBlueLineGraphPoint OP1 = OtherWire.PathPoints[j];
				FBlueLineGraphPoint OP2 = OtherWire.PathPoints[j+1];
				bool bOtherIsHorizontal = FMath::IsNearlyEqual(OP1.Y, OP2.Y, 1.0f);

				// In Manhattan style, make ONLY vertical wires jump over horizontal wires to keep it clean like GridLine
				bool bCanHorizontalJump = true;
				if (Settings && Settings->RoutingMethod == EBlueLineRoutingMethod::Manhattan)
				{
					bCanHorizontalJump = false;
				}

				if (bIsHorizontal && !bOtherIsHorizontal)
				{
					if (bCanHorizontalJump)
					{
						if (OP1.X > FMath::Min(P1.X, P2.X) && OP1.X < FMath::Max(P1.X, P2.X) &&
							P1.Y > FMath::Min(OP1.Y, OP2.Y) && P1.Y < FMath::Max(OP1.Y, OP2.Y))
						{
							Intersections.Add(FBlueLineGraphPoint(OP1.X, P1.Y));
						}
					}
				}
				else if (!bIsHorizontal && bOtherIsHorizontal)
				{
					if (OP1.Y > FMath::Min(P1.Y, P2.Y) && OP1.Y < FMath::Max(P1.Y, P2.Y) &&
						P1.X > FMath::Min(OP1.X, OP2.X) && P1.X < FMath::Max(OP1.X, OP2.X))
					{
						Intersections.Add(FBlueLineGraphPoint(P1.X, OP1.Y));
					}
				}
			}
		}

		if (bIsHorizontal)
		{
			Intersections.Sort([P1](const FBlueLineGraphPoint& A, const FBlueLineGraphPoint& B) {
				return FMath::Abs(A.X - P1.X) < FMath::Abs(B.X - P1.X);
			});
		}
		else
		{
			Intersections.Sort([P1](const FBlueLineGraphPoint& A, const FBlueLineGraphPoint& B) {
				return FMath::Abs(A.Y - P1.Y) < FMath::Abs(B.Y - P1.Y);
			});
		}

		float R1 = PointRadii[i];
		float R2 = PointRadii[i+1];

		FBlueLineGraphPoint SegmentStart = P1 + (Dir * R1);
		FBlueLineGraphPoint SegmentEnd = P2 - (Dir * R2);

		if (i > 0)
		{
			FBlueLineGraphPoint PrevDir = (P1 - Wire.PathPoints[i-1]).GetSafeNormal();
			FBlueLineGraphPoint PrevSegmentEnd = P1 - (PrevDir * R1);
			
			int32 ArcSegments = 6;
			for (int32 ArcIdx = 1; ArcIdx <= ArcSegments; ++ArcIdx)
			{
				float T = (float)ArcIdx / (float)ArcSegments;
				FBlueLineGraphPoint ArcP = FMath::Lerp(
					FMath::Lerp(PrevSegmentEnd, P1, T),
					FMath::Lerp(P1, SegmentStart, T),
					T
				);
				FinalPoints.Add(ArcP);
			}
		}
		else
		{
			FinalPoints.Add(SegmentStart);
		}

		for (const FBlueLineGraphPoint& Intersect : Intersections)
		{
			float DistToP1 = (Intersect - P1).Size();
			if (DistToP1 <= R1 + JumpRadius || DistToP1 >= SegLen - R2 - JumpRadius)
				continue; 

			FBlueLineGraphPoint PreJump = Intersect - (Dir * JumpRadius);
			FBlueLineGraphPoint PostJump = Intersect + (Dir * JumpRadius);

			FinalPoints.Add(PreJump);

			FBlueLineGraphPoint Perpendicular(-FMath::Abs(Dir.Y), -FMath::Abs(Dir.X));
			
			int32 ArcSegments = 6;
			for (int32 ArcIdx = 1; ArcIdx < ArcSegments; ++ArcIdx)
			{
				float T = (float)ArcIdx / (float)ArcSegments;
				float Angle = T * PI;
				float OffsetX = FMath::Cos(Angle + PI); 
				float OffsetY = FMath::Sin(Angle); 
				
				FBlueLineGraphPoint ArcPoint = Intersect + (Dir * (OffsetX * JumpRadius)) + (Perpendicular * (OffsetY * JumpRadius));
				FinalPoints.Add(ArcPoint);
			}

			FinalPoints.Add(PostJump);
		}

		if (i == Wire.PathPoints.Num() - 2)
		{
			FinalPoints.Add(SegmentEnd);
		}
	}

	DrawWirePolylineWithGhosting(FinalPoints, FinalColor, FinalThickness);

	// Overdraw the pin-exit stubs (first and last routed segments) at StubThickness so the
	// lead-in/out from each pin reads distinctly from the wire body.
	if (Settings && Wire.PathPoints.Num() >= 2)
	{
		const float StubThickness = Settings->StubThickness * LocalZoomFactor;
		if (StubThickness > 0.0f && !FMath::IsNearlyEqual(StubThickness, FinalThickness))
		{
			const int32 LastIndex = Wire.PathPoints.Num() - 1;
			const TArray<FBlueLineGraphPoint> StartStub = { Wire.PathPoints[0], Wire.PathPoints[1] };
			const TArray<FBlueLineGraphPoint> EndStub = { Wire.PathPoints[LastIndex - 1], Wire.PathPoints[LastIndex] };
			DrawWirePolylineWithGhosting(StartStub, FinalColor, StubThickness);
			DrawWirePolylineWithGhosting(EndStub, FinalColor, StubThickness);
		}
	}

	if (Wire.Params.bDrawBubbles)
	{
		const FBlueLineGraphPoint LastPoint = Wire.PathPoints.Last();
		const FBlueLineGraphPoint PrevPoint = Wire.PathPoints[Wire.PathPoints.Num() - 2];

		FBlueLineGraphPoint Dir = (LastPoint - PrevPoint).GetSafeNormal();
		FBlueLineGraphPoint ArrowPos = LastPoint - (Dir * (10.0f * LocalZoomFactor));

		const FSlateBrush* Brush = ArrowImage;
		if (!Brush) return;

		FBlueLineGraphPoint BrushSize((float)WireSettings.BubbleSize.X, (float)WireSettings.BubbleSize.Y);
		FBlueLineGraphPoint ScaledSize = BrushSize * LocalZoomFactor;

		FPaintGeometry ArrowGeometry(
			ArrowPos,
			ScaledSize,
			1.0f
		);

		FSlateDrawElement::MakeRotatedBox(
			DrawElementsList,
			ArrowLayerID,
			ArrowGeometry,
			Brush,
			ESlateDrawEffect::None,
			(float)FMath::Atan2(Dir.Y, Dir.X),
			TOptional<FVector2f>(),
			FSlateDrawElement::RelativeToElement,
			FinalColor
		);
	}
}






