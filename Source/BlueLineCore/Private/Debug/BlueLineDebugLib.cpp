// Copyright YourTeamName. All Rights Reserved.

#include "Debug/BlueLineDebugLib.h"
#include "Data/UBlueLineThemeData.h"
#include "BlueLineLog.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"
#include "AssetRegistry/AssetRegistryModule.h"

void UBlueLineDebugLib::DrawBlueLineDebugTag(const UObject* WorldContextObject, FGameplayTag Tag, FVector Location, float TextScale, float Duration)
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (!ensure(WorldContextObject))
	{
		return;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return;
	}

	// Resolve the color
	FLinearColor TagColor = GetColorForTag(Tag);

	// Convert FLinearColor to FColor for DrawDebugHelpers
	FColor DebugColor = TagColor.ToFColor(true);

	// Format text
	FString DebugText = Tag.ToString();

	// Draw the string in World space
	DrawDebugString(World, Location, DebugText, nullptr, DebugColor, Duration, true, TextScale);
#endif
}

FLinearColor UBlueLineDebugLib::GetColorForTag(FGameplayTag Tag)
{
	const UBlueLineThemeData* ThemeData = GetActiveThemeData();

	if (ThemeData)
	{
		return ThemeData->GetColorForTag(Tag);
	}

	// This is the Magenta color indicating missing DataAsset
	return FLinearColor(1.0f, 0.0f, 1.0f, 1.0f);
}

UBlueLineThemeData* UBlueLineDebugLib::GetActiveThemeData()
{
	// Cache the result so we don't search the hard drive every frame
	static TWeakObjectPtr<UBlueLineThemeData> CachedThemeData = nullptr;

	if (CachedThemeData.IsValid())
	{
		return CachedThemeData.Get();
	}

	// 1. Try the hardcoded convention first (Fastest)
	const FString DefaultAssetPath = TEXT("/Game/BlueLine/DA_BlueLineDefault.DA_BlueLineDefault");
	UBlueLineThemeData* LoadedData = Cast<UBlueLineThemeData>(StaticLoadObject(UBlueLineThemeData::StaticClass(), nullptr, *DefaultAssetPath));

	if (LoadedData)
	{
		CachedThemeData = LoadedData;
		return LoadedData;
	}

	// 2. If missing, Search the Asset Registry for ANY BlueLineThemeData asset
	// This ensures we find it even if you renamed it or moved it.
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetData;

	// UE 5.1+ Syntax
	AssetRegistryModule.Get().GetAssetsByClass(UBlueLineThemeData::StaticClass()->GetClassPathName(), AssetData);

	if (AssetData.Num() > 0)
	{
		// Load the first one found
		LoadedData = Cast<UBlueLineThemeData>(AssetData[0].GetAsset());
		if (LoadedData)
		{
			UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine: Auto-detected Theme Data at '%s'"), *AssetData[0].ObjectPath.ToString());
			CachedThemeData = LoadedData;
			return LoadedData;
		}
	}

	return nullptr;
}