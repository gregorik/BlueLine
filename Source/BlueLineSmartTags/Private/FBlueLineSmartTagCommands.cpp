// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "FBlueLineSmartTagCommands.h"

#define LOCTEXT_NAMESPACE "BlueLineSmartTags"

void FBlueLineSmartTagCommands::RegisterCommands()
{
    UI_COMMAND(
        AutoTagGraph,
        "Auto-Tag Graph",
        "Intelligently clusters nodes and applies semantic organization (Comment Boxes + Tags).",
        EUserInterfaceActionType::Button,
        FInputChord(EModifierKey::Shift, EKeys::T)
    );
}

#undef LOCTEXT_NAMESPACE

