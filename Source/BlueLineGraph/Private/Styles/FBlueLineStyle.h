// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/**
 * FBlueLineStyle
 * 
 * Manages the Slate styling resources (Icons, Brushes, Fonts) for the BlueLine plugin.
 * This acts as the bridge between your 'Resources' folder and the C++ UI code.
 * 
 * Usage:
 * const FSlateBrush* Icon = FBlueLineStyle::Get().GetBrush("BlueLine.AutoFormat");
 */
class FBlueLineStyle
{
public:

	/** Register the style set and load resources */
	static void Initialize();

	/** Unregister the style set and free memory */
	static void Shutdown();

	/** Reloads textures from disk (Useful for iteration) */
	static void ReloadTextures();

	/** Access the singleton instance */
	static const ISlateStyle& Get();

	/** Access the singleton name */
	static FName GetStyleSetName();

private:

	/** Creates the style set definition */
	static TSharedRef<class FSlateStyleSet> Create();

	/** Singleton instance */
	static TSharedPtr<class FSlateStyleSet> StyleInstance;
};