// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "Templates/SharedPointer.h"
#include "EdGraph/EdGraphPin.h"

class FBlueLineWireSnapper : public IInputProcessor, public TSharedFromThis<FBlueLineWireSnapper>
{
public:
    static void Enable();
    static void Disable();

    virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
    
    // IInputProcessor interface
    virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override { return false; }
    virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override { return false; }
    virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override { return false; }
    virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return false; }
    virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return false; }
    virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return false; }
    virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return false; }
    virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override { return false; }

    static void SetIsDraggingWire(bool bIsDragging) { bIsDraggingWire = bIsDragging; }

private:
    static TSharedPtr<FBlueLineWireSnapper> Instance;
    static bool bIsDraggingWire;
    
    FVector2D DragOriginScreen;
    bool bHasDragOrigin = false;
    FEdGraphPinReference DragSourcePin;
};
