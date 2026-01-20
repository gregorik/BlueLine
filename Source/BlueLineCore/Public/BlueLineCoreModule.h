// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// Note: Logging definitions are now strictly in BlueLineLog.h to avoid redefinition errors.

class FBlueLineCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FBlueLineCoreModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FBlueLineCoreModule>("BlueLineCore");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("BlueLineCore");
	}
};