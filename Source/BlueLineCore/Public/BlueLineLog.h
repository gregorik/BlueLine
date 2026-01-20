// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * LogBlueLineCore
 *
 * General purpose logging for the core module (Data loading, Runtime Debugging).
 */
BLUELINECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogBlueLineCore, Log, All);

/**
 * LogBlueLineDebug
 *
 * Dedicated category for verbose runtime visualization.
 */
BLUELINECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogBlueLineDebug, Log, All);