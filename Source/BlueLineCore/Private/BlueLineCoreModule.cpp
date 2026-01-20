// Copyright YourTeamName. All Rights Reserved.

#include "BlueLineCoreModule.h"
#include "BlueLineLog.h"

// FIX: Do NOT define log categories here. They are defined in BlueLineLog.cpp.
// DEFINE_LOG_CATEGORY(LogBlueLineCore); <--- DELETED
// DEFINE_LOG_CATEGORY(LogBlueLineDebug); <--- DELETED

void FBlueLineCoreModule::StartupModule()
{
	UE_LOG(LogBlueLineCore, Log, TEXT("BlueLineCore module has started."));
}

void FBlueLineCoreModule::ShutdownModule()
{
	UE_LOG(LogBlueLineCore, Log, TEXT("BlueLineCore module is shutting down."));
}

IMPLEMENT_MODULE(FBlueLineCoreModule, BlueLineCore)