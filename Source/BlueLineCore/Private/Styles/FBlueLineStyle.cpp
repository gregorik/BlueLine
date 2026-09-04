// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "Styles/FBlueLineStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )

TSharedPtr<FSlateStyleSet> FBlueLineStyle::StyleInstance = nullptr;

void FBlueLineStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FBlueLineStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		ensure(StyleInstance.IsUnique());
		StyleInstance.Reset();
	}
}

void FBlueLineStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FBlueLineStyle::Get()
{
	checkf(StyleInstance.IsValid(), TEXT("FBlueLineStyle not initialized - call Initialize() first"));
	return *StyleInstance;
}

FName FBlueLineStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("BlueLineStyle"));
	return StyleSetName;
}

TSharedRef<FSlateStyleSet> FBlueLineStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet(GetStyleSetName()));

	// Locate the Resources folder relative to the Plugin root
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("BlueLineCore");
	if (!Plugin.IsValid())
	{
		Plugin = IPluginManager::Get().FindPlugin("BlueLine");
	}

	if (Plugin.IsValid())
	{
		Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
	}
	else
	{
		// Fallback for development scenarios
		Style->SetContentRoot(FPaths::ProjectPluginsDir() / TEXT("BlueLineCore/Resources"));
	}

	// Standard Icon Sizes
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon32x32(32.0f, 32.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);

	// --- Graph Editor Icons ---
	// Expects: /Resources/Button_AutoFormat.png
	Style->Set("BlueLine.AutoFormat", new IMAGE_BRUSH("Button_AutoFormat", Icon20x20));

	// Main plugin logo
	// Expects: /Resources/Icon128.png
	Style->Set("BlueLine.PluginIcon", new IMAGE_BRUSH("Icon128", Icon40x40));

	return Style;
	}
#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
