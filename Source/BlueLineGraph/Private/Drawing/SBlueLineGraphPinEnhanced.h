// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphPin.h"

class UBlueLineEditorSettings;

/**
 * Enhanced Pin Widget that draws "Manhattan Stubs" and Connection Badges.
 */
class SBlueLineGraphPinEnhanced : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SBlueLineGraphPinEnhanced) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

	// SWidget Interface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

protected:
	const UBlueLineEditorSettings* GetSettings() const;

	// Visual Helpers
	void UpdateStubCache(const FGeometry& PinGeometry, float ZoomFactor) const;
	void DrawOrthogonalStub(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& AllottedGeometry) const;
	void DrawConnectionIndicator(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& AllottedGeometry) const;
	
	FVector2f GetPinConnectionPoint(const FGeometry& PinGeometry) const;
	FLinearColor GetStubColor(bool bEnhanced) const;
	
	float GetStubLength() const;
	float GetStubThickness() const;

protected:
	// Cache state
	mutable TArray<FVector2f> CachedStubPoints;
	mutable int32 LastConnectionCount = -1;
	mutable bool bStubCacheDirty = true;
	mutable bool bIsHovered = false;
};