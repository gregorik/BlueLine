// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FBlueLineSmartTagsModule
 * 
 * Editor-only module responsible for injecting custom UI into the Details Panel.
 * It overrides how FGameplayTag and FGameplayTagContainer are rendered,
 * replacing the standard text with the "Colored Chip" style defined in your Theme Data.
 */
class FBlueLineSmartTagsModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** 
	 * Helper to register custom property type layouts.
	 * We separate this to keep StartupModule clean.
	 */
	void RegisterPropertyTypeCustomizations();

	/** 
	 * Helper to unregister layouts on shutdown. 
	 * Essential for supporting hot-reloading without crashing.
	 */
	void UnregisterPropertyTypeCustomizations();

	/** Name of the property name to customize (FGameplayTag) */
	FName GameplayTagName;
	
	/** Name of the container struct to customize (FGameplayTagContainer) */
	FName GameplayTagContainerName;
};