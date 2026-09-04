// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "Settings/UBlueLineEditorSettings.h"
#include "BlueLineLog.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Define the static delegate
UBlueLineEditorSettings::FOnBlueLineSettingsChanged UBlueLineEditorSettings::OnSettingsChanged;

UBlueLineEditorSettings::UBlueLineEditorSettings()
{
    //==========================================================================
    // General
    //==========================================================================
    bEnableBlueLine = true;
    bShowWelcomeNotification = true;
    bCheckForUpdates = true;
    
    //==========================================================================
    // Routing
    //==========================================================================
    bEnableWireSnapping = true;
    WireSnapRadius = 40.0f;
    RoutingMethod = EBlueLineRoutingMethod::Manhattan;
    bAutoRouteNewConnections = false;
    AutoRouteMaxNodes = 200;
    bEnableRigidifyCommand = true;
    HorizontalStubLength = 50.0f;
    VerticalOffset = 80.0f;
    MinRigidifySpacing = 100.0f;
    bSnapReroutesToGrid = true;
    GridSnapSize = 16;
    
    //==========================================================================
    // Visuals
    //==========================================================================
    WireStyle = EBlueLineWireStyle::Medium;
    WireThicknessMultiplier = 1.0f;
    StubThickness = 2.0f;
    StubLength = 20.0f;
    bShowConnectionCount = true;
    bHighlightWiresOnHover = true;
    WireHighlightIntensity = 1.3f;
    
    //==========================================================================
    // Formatting
    //==========================================================================
    FormatStrategy = EBlueLineFormatStrategy::Flow;
    bEnableAutoFormat = true;
    HorizontalSpacing = 300.0f;
    VerticalSpacing = 120.0f;
    CommentBoxPadding = 40.0f;
    MagnetEvaluationDistance = 100.0f;
    bPreventNodeOverlap = true;
    CollisionPadding = 20;
    
    //==========================================================================
    // Smart Tags
    //==========================================================================
    bEnableSmartTags = true;
    bEnableAutoTagCommand = true;
    MinClusterSizeAuto = 3;
    MinClusterSizeSelection = 2;
    SemanticConfidenceThreshold = 3.0f;
    ClusterWeightMultiplier = 1.0f;
    
    //==========================================================================
    // Level Editor
    //==========================================================================
    bEnableLevelPieMenu = true;
    bEnableScopeSelection = true;
    DefaultSelectionRadius = 500.0f;
    SelectionRadiusIncrement = 50.0f;
    MaxSelectionRadius = 5000.0f;
    MinSelectionRadius = 100.0f;
    PieMenuRadius = 120.0f;
    PieMenuDeadZone = 30.0f;
    PieMenuAnimationSpeed = 15.0f;
    bInvertPieMenuYAxis = false;
    
    //==========================================================================
    // Debug
    //==========================================================================
    bEnableDebugVisualization = true;
    bShowGraphAnalysisOverlay = false;
    bShowNodeComplexity = false;
    bDebugDrawFormattingBounds = false;
    bVerboseLogging = false;
    bExposeDemoNodes = false;
    DebugDrawDuration = 0.05f;
    DebugSphereSegments = 32;
    DebugLineThickness = 1.5f;
    
    //==========================================================================
    // Export
    //==========================================================================
    DefaultExportPath.Path = TEXT("");
    DefaultSubsystemPath.Path = TEXT("");
    bExportIncludeComments = true;
    bExportIncludeVariableTypes = true;
    bExportIncludeDefaultValues = false;
}

#if WITH_EDITOR
void UBlueLineEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    
    // Validate GridSnapSize is a power of 2
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UBlueLineEditorSettings, GridSnapSize))
    {
        // Round to nearest power of 2
        int32 NewValue = GridSnapSize;
        if (NewValue < 1) NewValue = 1;
        else if (NewValue > 64) NewValue = 64;
        else
        {
            // Round to nearest power of 2
            int32 Lower = 1;
            int32 Upper = 1;
            while (Upper < NewValue && Upper < 64) 
            {
                Lower = Upper;
                Upper <<= 1;
            }
            GridSnapSize = (NewValue - Lower < Upper - NewValue) ? Lower : Upper;
        }
    }
    
    // Clamp Min/Max selection radius
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UBlueLineEditorSettings, MinSelectionRadius) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(UBlueLineEditorSettings, MaxSelectionRadius))
    {
        if (MinSelectionRadius >= MaxSelectionRadius)
        {
            MinSelectionRadius = MaxSelectionRadius - 100.0f;
        }
    }

    // When any setting changes, broadcast this event
    // Modules listen to this to update their behavior immediately
    OnSettingsChanged.Broadcast();
    
    UE_LOG(LogBlueLineCore, Verbose, TEXT("BlueLine Settings changed: %s"), *PropertyName.ToString());
}

void UBlueLineEditorSettings::ResetToDefaults()
{
    // Create a temporary CDO-like instance to get default values, then copy all
    // UPROPERTY fields from it onto this instance.
    UBlueLineEditorSettings* TempDefaults = NewObject<UBlueLineEditorSettings>();

    // Copy all UProperty values from the temp defaults to this instance
    for (TFieldIterator<FProperty> It(GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        if (Property->HasAnyPropertyFlags(CPF_Config | CPF_Edit))
        {
            const void* SrcAddr = Property->ContainerPtrToValuePtr<void>(TempDefaults);
            void* DstAddr = Property->ContainerPtrToValuePtr<void>(this);
            Property->CopyCompleteValue(DstAddr, SrcAddr);
        }
    }

    SaveConfig();
    OnSettingsChanged.Broadcast();
}

void UBlueLineEditorSettings::ExportSettings()
{
    // Create JSON object with all settings
    TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
    RootObject->SetStringField(TEXT("Version"), TEXT("1.0"));
    RootObject->SetStringField(TEXT("Plugin"), TEXT("BlueLine"));
    
    // General
    TSharedPtr<FJsonObject> GeneralObj = MakeShared<FJsonObject>();
    GeneralObj->SetBoolField(TEXT("bEnableBlueLine"), bEnableBlueLine);
    GeneralObj->SetBoolField(TEXT("bShowWelcomeNotification"), bShowWelcomeNotification);
    GeneralObj->SetBoolField(TEXT("bCheckForUpdates"), bCheckForUpdates);
    RootObject->SetObjectField(TEXT("General"), GeneralObj);
    
    // Routing
    TSharedPtr<FJsonObject> RoutingObj = MakeShared<FJsonObject>();
    RoutingObj->SetNumberField(TEXT("RoutingMethod"), static_cast<int32>(RoutingMethod));
    RoutingObj->SetBoolField(TEXT("bAutoRouteNewConnections"), bAutoRouteNewConnections);
    RoutingObj->SetBoolField(TEXT("bEnableRigidifyCommand"), bEnableRigidifyCommand);
    RoutingObj->SetNumberField(TEXT("HorizontalStubLength"), HorizontalStubLength);
    RoutingObj->SetNumberField(TEXT("VerticalOffset"), VerticalOffset);
    RoutingObj->SetNumberField(TEXT("MinRigidifySpacing"), MinRigidifySpacing);
    RoutingObj->SetBoolField(TEXT("bSnapReroutesToGrid"), bSnapReroutesToGrid);
    RoutingObj->SetNumberField(TEXT("GridSnapSize"), GridSnapSize);
    RootObject->SetObjectField(TEXT("Routing"), RoutingObj);
    
    // Visuals
    TSharedPtr<FJsonObject> VisualsObj = MakeShared<FJsonObject>();
    VisualsObj->SetNumberField(TEXT("WireStyle"), static_cast<int32>(WireStyle));
    VisualsObj->SetNumberField(TEXT("WireThicknessMultiplier"), WireThicknessMultiplier);
    VisualsObj->SetNumberField(TEXT("StubThickness"), StubThickness);
    VisualsObj->SetNumberField(TEXT("StubLength"), StubLength);
    VisualsObj->SetBoolField(TEXT("bShowConnectionCount"), bShowConnectionCount);
    VisualsObj->SetBoolField(TEXT("bHighlightWiresOnHover"), bHighlightWiresOnHover);
    VisualsObj->SetNumberField(TEXT("WireHighlightIntensity"), WireHighlightIntensity);
    RootObject->SetObjectField(TEXT("Visuals"), VisualsObj);
    
    // Formatting
    TSharedPtr<FJsonObject> FormattingObj = MakeShared<FJsonObject>();
    FormattingObj->SetNumberField(TEXT("FormatStrategy"), static_cast<int32>(FormatStrategy));
    FormattingObj->SetBoolField(TEXT("bEnableAutoFormat"), bEnableAutoFormat);
    FormattingObj->SetNumberField(TEXT("HorizontalSpacing"), HorizontalSpacing);
    FormattingObj->SetNumberField(TEXT("VerticalSpacing"), VerticalSpacing);
    FormattingObj->SetNumberField(TEXT("CommentBoxPadding"), CommentBoxPadding);
    FormattingObj->SetNumberField(TEXT("MagnetEvaluationDistance"), MagnetEvaluationDistance);
    FormattingObj->SetBoolField(TEXT("bPreventNodeOverlap"), bPreventNodeOverlap);
    FormattingObj->SetNumberField(TEXT("CollisionPadding"), CollisionPadding);
    RootObject->SetObjectField(TEXT("Formatting"), FormattingObj);
    
    // Smart Tags
    TSharedPtr<FJsonObject> SmartTagsObj = MakeShared<FJsonObject>();
    SmartTagsObj->SetBoolField(TEXT("bEnableSmartTags"), bEnableSmartTags);
    SmartTagsObj->SetBoolField(TEXT("bEnableAutoTagCommand"), bEnableAutoTagCommand);
    SmartTagsObj->SetNumberField(TEXT("MinClusterSizeAuto"), MinClusterSizeAuto);
    SmartTagsObj->SetNumberField(TEXT("MinClusterSizeSelection"), MinClusterSizeSelection);
    SmartTagsObj->SetNumberField(TEXT("SemanticConfidenceThreshold"), SemanticConfidenceThreshold);
    SmartTagsObj->SetNumberField(TEXT("ClusterWeightMultiplier"), ClusterWeightMultiplier);
    RootObject->SetObjectField(TEXT("SmartTags"), SmartTagsObj);
    
    // Level Editor
    TSharedPtr<FJsonObject> LevelObj = MakeShared<FJsonObject>();
    LevelObj->SetBoolField(TEXT("bEnableLevelPieMenu"), bEnableLevelPieMenu);
    LevelObj->SetBoolField(TEXT("bEnableScopeSelection"), bEnableScopeSelection);
    LevelObj->SetNumberField(TEXT("DefaultSelectionRadius"), DefaultSelectionRadius);
    LevelObj->SetNumberField(TEXT("SelectionRadiusIncrement"), SelectionRadiusIncrement);
    LevelObj->SetNumberField(TEXT("MaxSelectionRadius"), MaxSelectionRadius);
    LevelObj->SetNumberField(TEXT("MinSelectionRadius"), MinSelectionRadius);
    LevelObj->SetNumberField(TEXT("PieMenuRadius"), PieMenuRadius);
    LevelObj->SetNumberField(TEXT("PieMenuDeadZone"), PieMenuDeadZone);
    LevelObj->SetNumberField(TEXT("PieMenuAnimationSpeed"), PieMenuAnimationSpeed);
    LevelObj->SetBoolField(TEXT("bInvertPieMenuYAxis"), bInvertPieMenuYAxis);
    RootObject->SetObjectField(TEXT("LevelEditor"), LevelObj);
    
    // Debug
    TSharedPtr<FJsonObject> DebugObj = MakeShared<FJsonObject>();
    DebugObj->SetBoolField(TEXT("bEnableDebugVisualization"), bEnableDebugVisualization);
    DebugObj->SetBoolField(TEXT("bShowGraphAnalysisOverlay"), bShowGraphAnalysisOverlay);
    DebugObj->SetBoolField(TEXT("bShowNodeComplexity"), bShowNodeComplexity);
    DebugObj->SetBoolField(TEXT("bDebugDrawFormattingBounds"), bDebugDrawFormattingBounds);
    DebugObj->SetBoolField(TEXT("bVerboseLogging"), bVerboseLogging);
    DebugObj->SetBoolField(TEXT("bExposeDemoNodes"), bExposeDemoNodes);
    DebugObj->SetNumberField(TEXT("DebugDrawDuration"), DebugDrawDuration);
    DebugObj->SetNumberField(TEXT("DebugSphereSegments"), DebugSphereSegments);
    DebugObj->SetNumberField(TEXT("DebugLineThickness"), DebugLineThickness);
    RootObject->SetObjectField(TEXT("Debug"), DebugObj);
    
    // Export
    TSharedPtr<FJsonObject> ExportObj = MakeShared<FJsonObject>();
    ExportObj->SetStringField(TEXT("DefaultExportPath"), DefaultExportPath.Path);
    ExportObj->SetStringField(TEXT("DefaultSubsystemPath"), DefaultSubsystemPath.Path);
    ExportObj->SetBoolField(TEXT("bExportIncludeComments"), bExportIncludeComments);
    ExportObj->SetBoolField(TEXT("bExportIncludeVariableTypes"), bExportIncludeVariableTypes);
    ExportObj->SetBoolField(TEXT("bExportIncludeDefaultValues"), bExportIncludeDefaultValues);
    RootObject->SetObjectField(TEXT("Export"), ExportObj);
    
    // Serialize to string
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
    
    // Save to file
    FString SavePath = FPaths::ProjectSavedDir() / TEXT("BlueLineSettings.json");
    FFileHelper::SaveStringToFile(OutputString, *SavePath);
    
    UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine settings exported to: %s"), *SavePath);
}

void UBlueLineEditorSettings::ImportSettings()
{
    FString LoadPath = FPaths::ProjectSavedDir() / TEXT("BlueLineSettings.json");
    FString FileContents;
    
    if (!FFileHelper::LoadFileToString(FileContents, *LoadPath))
    {
        UE_LOG(LogBlueLineCore, Warning, TEXT("Failed to load BlueLine settings from: %s"), *LoadPath);
        return;
    }
    
    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContents);
    
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        UE_LOG(LogBlueLineCore, Warning, TEXT("Failed to parse BlueLine settings JSON"));
        return;
    }
    
    // Version check
    FString Version;
    if (RootObject->TryGetStringField(TEXT("Version"), Version))
    {
        UE_LOG(LogBlueLineCore, Log, TEXT("Importing BlueLine settings version: %s"), *Version);
    }
    
    // Helper lambda to get nested object
    auto GetCategory = [&](const FString& Name) -> TSharedPtr<FJsonObject>
    {
        const TSharedPtr<FJsonObject>* Obj;
        if (RootObject->TryGetObjectField(Name, Obj))
        {
            return *Obj;
        }
        return nullptr;
    };
    
    // General
    if (TSharedPtr<FJsonObject> Obj = GetCategory(TEXT("General")))
    {
        Obj->TryGetBoolField(TEXT("bEnableBlueLine"), bEnableBlueLine);
        Obj->TryGetBoolField(TEXT("bShowWelcomeNotification"), bShowWelcomeNotification);
        Obj->TryGetBoolField(TEXT("bCheckForUpdates"), bCheckForUpdates);
    }
    
    // Routing
    if (TSharedPtr<FJsonObject> Obj = GetCategory(TEXT("Routing")))
    {
        int32 EnumVal;
        if (Obj->TryGetNumberField(TEXT("RoutingMethod"), EnumVal))
            RoutingMethod = static_cast<EBlueLineRoutingMethod>(EnumVal);
        Obj->TryGetBoolField(TEXT("bAutoRouteNewConnections"), bAutoRouteNewConnections);
        Obj->TryGetBoolField(TEXT("bEnableRigidifyCommand"), bEnableRigidifyCommand);
        Obj->TryGetNumberField(TEXT("HorizontalStubLength"), HorizontalStubLength);
        Obj->TryGetNumberField(TEXT("VerticalOffset"), VerticalOffset);
        Obj->TryGetNumberField(TEXT("MinRigidifySpacing"), MinRigidifySpacing);
        Obj->TryGetBoolField(TEXT("bSnapReroutesToGrid"), bSnapReroutesToGrid);
        Obj->TryGetNumberField(TEXT("GridSnapSize"), GridSnapSize);
    }
    
    // Visuals
    if (TSharedPtr<FJsonObject> Obj = GetCategory(TEXT("Visuals")))
    {
        int32 EnumVal;
        if (Obj->TryGetNumberField(TEXT("WireStyle"), EnumVal))
            WireStyle = static_cast<EBlueLineWireStyle>(EnumVal);
        Obj->TryGetNumberField(TEXT("WireThicknessMultiplier"), WireThicknessMultiplier);
        Obj->TryGetNumberField(TEXT("StubThickness"), StubThickness);
        Obj->TryGetNumberField(TEXT("StubLength"), StubLength);
        Obj->TryGetBoolField(TEXT("bShowConnectionCount"), bShowConnectionCount);
        Obj->TryGetBoolField(TEXT("bHighlightWiresOnHover"), bHighlightWiresOnHover);
        Obj->TryGetNumberField(TEXT("WireHighlightIntensity"), WireHighlightIntensity);
    }
    
    // Formatting
    if (TSharedPtr<FJsonObject> Obj = GetCategory(TEXT("Formatting")))
    {
        int32 EnumVal;
        if (Obj->TryGetNumberField(TEXT("FormatStrategy"), EnumVal))
            FormatStrategy = static_cast<EBlueLineFormatStrategy>(EnumVal);
        Obj->TryGetBoolField(TEXT("bEnableAutoFormat"), bEnableAutoFormat);
        Obj->TryGetNumberField(TEXT("HorizontalSpacing"), HorizontalSpacing);
        Obj->TryGetNumberField(TEXT("VerticalSpacing"), VerticalSpacing);
        Obj->TryGetNumberField(TEXT("CommentBoxPadding"), CommentBoxPadding);
        Obj->TryGetNumberField(TEXT("MagnetEvaluationDistance"), MagnetEvaluationDistance);
        Obj->TryGetBoolField(TEXT("bPreventNodeOverlap"), bPreventNodeOverlap);
        Obj->TryGetNumberField(TEXT("CollisionPadding"), CollisionPadding);
    }
    
    // Smart Tags
    if (TSharedPtr<FJsonObject> Obj = GetCategory(TEXT("SmartTags")))
    {
        Obj->TryGetBoolField(TEXT("bEnableSmartTags"), bEnableSmartTags);
        Obj->TryGetBoolField(TEXT("bEnableAutoTagCommand"), bEnableAutoTagCommand);
        Obj->TryGetNumberField(TEXT("MinClusterSizeAuto"), MinClusterSizeAuto);
        Obj->TryGetNumberField(TEXT("MinClusterSizeSelection"), MinClusterSizeSelection);
        Obj->TryGetNumberField(TEXT("SemanticConfidenceThreshold"), SemanticConfidenceThreshold);
        Obj->TryGetNumberField(TEXT("ClusterWeightMultiplier"), ClusterWeightMultiplier);
    }
    
    // Level Editor
    if (TSharedPtr<FJsonObject> Obj = GetCategory(TEXT("LevelEditor")))
    {
        Obj->TryGetBoolField(TEXT("bEnableLevelPieMenu"), bEnableLevelPieMenu);
        Obj->TryGetBoolField(TEXT("bEnableScopeSelection"), bEnableScopeSelection);
        Obj->TryGetNumberField(TEXT("DefaultSelectionRadius"), DefaultSelectionRadius);
        Obj->TryGetNumberField(TEXT("SelectionRadiusIncrement"), SelectionRadiusIncrement);
        Obj->TryGetNumberField(TEXT("MaxSelectionRadius"), MaxSelectionRadius);
        Obj->TryGetNumberField(TEXT("MinSelectionRadius"), MinSelectionRadius);
        Obj->TryGetNumberField(TEXT("PieMenuRadius"), PieMenuRadius);
        Obj->TryGetNumberField(TEXT("PieMenuDeadZone"), PieMenuDeadZone);
        Obj->TryGetNumberField(TEXT("PieMenuAnimationSpeed"), PieMenuAnimationSpeed);
        Obj->TryGetBoolField(TEXT("bInvertPieMenuYAxis"), bInvertPieMenuYAxis);
    }
    
    // Debug
    if (TSharedPtr<FJsonObject> Obj = GetCategory(TEXT("Debug")))
    {
        Obj->TryGetBoolField(TEXT("bEnableDebugVisualization"), bEnableDebugVisualization);
        Obj->TryGetBoolField(TEXT("bShowGraphAnalysisOverlay"), bShowGraphAnalysisOverlay);
        Obj->TryGetBoolField(TEXT("bShowNodeComplexity"), bShowNodeComplexity);
        Obj->TryGetBoolField(TEXT("bDebugDrawFormattingBounds"), bDebugDrawFormattingBounds);
        Obj->TryGetBoolField(TEXT("bVerboseLogging"), bVerboseLogging);
        Obj->TryGetBoolField(TEXT("bExposeDemoNodes"), bExposeDemoNodes);
        Obj->TryGetNumberField(TEXT("DebugDrawDuration"), DebugDrawDuration);
        Obj->TryGetNumberField(TEXT("DebugSphereSegments"), DebugSphereSegments);
        Obj->TryGetNumberField(TEXT("DebugLineThickness"), DebugLineThickness);
    }
    
    // Export
    if (TSharedPtr<FJsonObject> Obj = GetCategory(TEXT("Export")))
    {
        Obj->TryGetStringField(TEXT("DefaultExportPath"), DefaultExportPath.Path);
        Obj->TryGetStringField(TEXT("DefaultSubsystemPath"), DefaultSubsystemPath.Path);
        Obj->TryGetBoolField(TEXT("bExportIncludeComments"), bExportIncludeComments);
        Obj->TryGetBoolField(TEXT("bExportIncludeVariableTypes"), bExportIncludeVariableTypes);
        Obj->TryGetBoolField(TEXT("bExportIncludeDefaultValues"), bExportIncludeDefaultValues);
    }
    
    // Save and notify
    SaveConfig();
    OnSettingsChanged.Broadcast();
    
    UE_LOG(LogBlueLineCore, Log, TEXT("BlueLine settings imported from: %s"), *LoadPath);
}
#endif
