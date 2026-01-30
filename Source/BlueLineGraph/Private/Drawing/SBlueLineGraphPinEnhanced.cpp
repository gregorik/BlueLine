// Copyright YourTeamName. All Rights Reserved.

#include "Drawing/SBlueLineGraphPinEnhanced.h"
#include "Settings/UBlueLineEditorSettings.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h" 
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/Geometry.h" // <--- FIX: Provides FSlateLayoutTransform

void SBlueLineGraphPinEnhanced::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InPin);
	bStubCacheDirty = true;
}

int32 SBlueLineGraphPinEnhanced::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	// Draw base pin
	LayerId = SGraphPin::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);

	const UBlueLineEditorSettings* Settings = GetSettings();
	if (!Settings || !Settings->bEnableManhattanRouting || !GraphPinObj || GraphPinObj->LinkedTo.Num() == 0)
	{
		return LayerId;
	}

	if (bStubCacheDirty || GraphPinObj->LinkedTo.Num() != LastConnectionCount)
	{
		UpdateStubCache(AllottedGeometry, AllottedGeometry.Scale);
		LastConnectionCount = GraphPinObj->LinkedTo.Num();
		bStubCacheDirty = false;
	}

	DrawOrthogonalStub(OutDrawElements, LayerId, AllottedGeometry);

	if (Settings->bShowConnectionCount && GraphPinObj->LinkedTo.Num() > 1)
	{
		DrawConnectionIndicator(OutDrawElements, LayerId, AllottedGeometry);
	}

	return LayerId + 3;
}

void SBlueLineGraphPinEnhanced::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SGraphPin::OnMouseEnter(MyGeometry, MouseEvent);
	bIsHovered = true;
}

void SBlueLineGraphPinEnhanced::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SGraphPin::OnMouseLeave(MouseEvent);
	bIsHovered = false;
}

const UBlueLineEditorSettings* SBlueLineGraphPinEnhanced::GetSettings() const
{
	return GetDefault<UBlueLineEditorSettings>();
}

void SBlueLineGraphPinEnhanced::UpdateStubCache(const FGeometry& PinGeometry, float ZoomFactor) const
{
	CachedStubPoints.Reset(2);

	if (!GraphPinObj) return;

	const FVector2f PinCenter = GetPinConnectionPoint(PinGeometry);
	const bool bIsOutputPin = (GraphPinObj->Direction == EGPD_Output);
	const float StubDirection = bIsOutputPin ? 1.0f : -1.0f;
	const float ScaledStubLength = GetStubLength() * ZoomFactor;

	CachedStubPoints.Add(PinCenter);
	CachedStubPoints.Add(PinCenter + FVector2f(StubDirection * ScaledStubLength, 0.0f));
}

void SBlueLineGraphPinEnhanced::DrawOrthogonalStub(
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FGeometry& AllottedGeometry) const
{
	if (CachedStubPoints.Num() < 2) return;

	const int32 StubLayerId = LayerId + 2;
	const float StubThickness = GetStubThickness() * AllottedGeometry.Scale;

	FLinearColor StubColor = GetStubColor(true);
	if (bIsHovered)
	{
		StubColor = FLinearColor::LerpUsingHSV(StubColor, FLinearColor::White, 0.3f);
	}

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		StubLayerId,
		AllottedGeometry.ToPaintGeometry(),
		CachedStubPoints,
		ESlateDrawEffect::None,
		StubColor,
		true,
		StubThickness
	);
}

void SBlueLineGraphPinEnhanced::DrawConnectionIndicator(
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FGeometry& AllottedGeometry) const
{
	if (!GraphPinObj || GraphPinObj->LinkedTo.Num() <= 1) return;

	const FVector2f StubEnd = CachedStubPoints.Last();
	const float ZoomFactor = AllottedGeometry.Scale;
	const float BadgeRadius = 8.0f * ZoomFactor;
	const FVector2f BadgeSize(BadgeRadius * 2.0f, BadgeRadius * 2.0f);

	// FIX: Use explicit LayoutTransform overload with Correct Order: (Size, Transform)
	FPaintGeometry BadgeGeometry = AllottedGeometry.ToPaintGeometry(
		BadgeSize,
		FSlateLayoutTransform(StubEnd - FVector2f(BadgeRadius, BadgeRadius))
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 3,
		BadgeGeometry,
		FAppStyle::GetBrush("Graph.Node.Body"),
		ESlateDrawEffect::None,
		GetStubColor(false)
	);

	const FString CountText = FString::Printf(TEXT("%d"), GraphPinObj->LinkedTo.Num());
	const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Bold", (uint16)(7.0f * ZoomFactor));

	// FIX: Use explicit LayoutTransform overload with Correct Order: (Size, Transform)
	FPaintGeometry TextGeometry = AllottedGeometry.ToPaintGeometry(
		BadgeSize,
		FSlateLayoutTransform(StubEnd - FVector2f(BadgeRadius * 0.5f, BadgeRadius * 0.7f))
	);

	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId + 4,
		TextGeometry,
		CountText,
		FontInfo,
		ESlateDrawEffect::None,
		FLinearColor::Black
	);
}

FVector2f SBlueLineGraphPinEnhanced::GetPinConnectionPoint(const FGeometry& PinGeometry) const
{
	const FVector2D LocalSize = PinGeometry.GetLocalSize();
	const bool bIsOutputPin = GraphPinObj && (GraphPinObj->Direction == EGPD_Output);

	const float XPos = bIsOutputPin ? LocalSize.X : 0.0f;
	const float YPos = LocalSize.Y * 0.5f;

	return FVector2f((float)XPos, (float)YPos);
}

FLinearColor SBlueLineGraphPinEnhanced::GetStubColor(bool bEnhanced) const
{
	if (!GraphPinObj) return FLinearColor::White;

	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
	if (Schema)
	{
		FLinearColor BaseColor = Schema->GetPinTypeColor(GraphPinObj->PinType);
		if (bEnhanced)
		{
			BaseColor = FLinearColor::LerpUsingHSV(BaseColor, FLinearColor::White, 0.15f);
		}
		return BaseColor;
	}
	return FLinearColor::White;
}

float SBlueLineGraphPinEnhanced::GetStubLength() const
{
	const UBlueLineEditorSettings* Settings = GetSettings();
	return Settings ? Settings->StubLength : 25.0f;
}

float SBlueLineGraphPinEnhanced::GetStubThickness() const
{
	const UBlueLineEditorSettings* Settings = GetSettings();
	return Settings ? Settings->StubThickness : 2.5f;
}