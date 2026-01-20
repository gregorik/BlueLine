// Copyright YourTeamName. All Rights Reserved.

#include "BlueLineSmartTagsModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Data/UBlueLineThemeData.h"
#include "Customization/FBlueLineTagCustomization.h"

#define LOCTEXT_NAMESPACE "BlueLineSmartTags"

void FBlueLineSmartTagsModule::StartupModule()
{
	// FIX: Force load the GameplayTagsEditor module.
	// This ensures that the global FGameplayTag customization is registered 
	// so that when we ask for 'CreatePropertyValueWidget', we get the nice dropdown, not a grey text box.
	FModuleManager::Get().LoadModuleChecked<IModuleInterface>("GameplayTagsEditor");

	RegisterPropertyTypeCustomizations();
}

void FBlueLineSmartTagsModule::ShutdownModule()
{
	UnregisterPropertyTypeCustomizations();
}

void FBlueLineSmartTagsModule::RegisterPropertyTypeCustomizations()
{
	if (!FModuleManager::Get().IsModuleLoaded("PropertyEditor")) return;

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Customize our Theme Style struct only.
	// We leave FGameplayTag alone so the engine defaults handle the dropdown logic.
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FBlueLineTagStyle::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FBlueLineTagCustomization::MakeInstance)
	);
}

void FBlueLineSmartTagsModule::UnregisterPropertyTypeCustomizations()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(FBlueLineTagStyle::StaticStruct()->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FBlueLineSmartTagsModule, BlueLineSmartTags)