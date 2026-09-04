// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "UBlueLineEditorSettings.generated.h"

/**
 * Blueprint Graph Routing Method
 */
UENUM(BlueprintType)
enum class EBlueLineRoutingMethod : uint8
{
    /** Standard UE curved wires */
    Curved        UMETA(DisplayName = "Curved (Default)"),
    /** Orthogonal Manhattan-style routing */
    Manhattan     UMETA(DisplayName = "Manhattan (Orthogonal)"),
    /** Circuit-board style with rounded corners */
    Circuit       UMETA(DisplayName = "Circuit Board"),
    /** Hybrid - Manhattan for long wires, curved for short */
    Hybrid        UMETA(DisplayName = "Hybrid (Auto)"),
};

/**
 * Wire Style Configuration
 */
UENUM(BlueprintType)
enum class EBlueLineWireStyle : uint8
{
    /** Thin consistent lines */
    Thin          UMETA(DisplayName = "Thin (1.5px)"),
    /** Medium thickness */
    Medium        UMETA(DisplayName = "Medium (2.5px)"),
    /** Thick bold lines */
    Thick         UMETA(DisplayName = "Thick (4px)"),
    /** Variable based on connection type */
    Dynamic       UMETA(DisplayName = "Dynamic (Type-based)")
};

/**
 * Auto-Formatting Strategy
 */
UENUM(BlueprintType)
enum class EBlueLineFormatStrategy : uint8
{
    /** Align to a grid */
    Grid          UMETA(DisplayName = "Grid Snap"),
    /** Flow-based layout */
    Flow          UMETA(DisplayName = "Flow Layout"),
    /** Columnar organization */
    Columns       UMETA(DisplayName = "Column Layout"),
    /** Minimum node movement */
    Minimal       UMETA(DisplayName = "Minimal Changes"),
};

/**
 * BlueLine Editor Settings
 * 
 * Comprehensive configuration for all BlueLine modules.
 * Located in BlueLineCore so all modules can access settings without circular dependencies.
 * 
 * Access via: Edit > Editor Preferences > Plugins > BlueLine
 */
UCLASS(config = EditorPerProjectUserSettings, defaultconfig, meta = (DisplayName = "BlueLine"))
class BLUELINECORE_API UBlueLineEditorSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UBlueLineEditorSettings();

    //==========================================================================
    // GENERAL - Enable/Disable Features
    //==========================================================================
    
    /** Master switch to enable/disable all BlueLine features */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|General", meta = (DisplayName = "Enable BlueLine"))
    bool bEnableBlueLine = true;
    
    /** Show welcome notification on editor startup */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|General", meta = (EditCondition = "bEnableBlueLine"))
    bool bShowWelcomeNotification = true;
    
    /** Automatically check for updates */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|General", meta = (EditCondition = "bEnableBlueLine"))
    bool bCheckForUpdates = true;

    //==========================================================================
    // ROUTING - Wire Connection Routing
    //==========================================================================
    
    /** Enable Node Graph Assistant-style wire snapping */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Routing", meta = (EditCondition = "bEnableBlueLine"))
    bool bEnableWireSnapping = true;
    
    /** Radius (in graph units) where the wire will magnetically snap to a pin */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Routing", meta = (EditCondition = "bEnableBlueLine && bEnableWireSnapping", UIMin = "10", UIMax = "100"))
    float WireSnapRadius = 40.0f;
    
    /** Primary routing method for wire connections */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Routing", meta = (EditCondition = "bEnableBlueLine"))
    EBlueLineRoutingMethod RoutingMethod = EBlueLineRoutingMethod::Manhattan;
    
    /** If true, Manhattan routing will be automatically applied to new connections */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Routing", 
        meta = (EditCondition = "bEnableBlueLine && RoutingMethod == EBlueLineRoutingMethod::Manhattan", DisplayName = "Auto-Route New Connections"))
    bool bAutoRouteNewConnections = false;
    
    /** Maximum number of nodes in a graph for auto-routing to activate (prevents lag on large graphs) */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Routing|Advanced",
        meta = (EditCondition = "bEnableBlueLine && bAutoRouteNewConnections", UIMin = "50", UIMax = "500", ClampMin = "10", ClampMax = "1000"))
    int32 AutoRouteMaxNodes = 200;
    
    /** Enable rigidify command (Shift+R) for straightening connections */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Routing", meta = (EditCondition = "bEnableBlueLine"))
    bool bEnableRigidifyCommand = true;
    
    /** Horizontal distance from pin before making first turn (Manhattan routing) */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Routing|Advanced", 
        meta = (EditCondition = "bEnableBlueLine", UIMin = "20", UIMax = "200", ClampMin = "10", ClampMax = "500"))
    float HorizontalStubLength = 50.0f;
    
    /** Vertical offset for Manhattan routing paths */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Routing|Advanced",
        meta = (EditCondition = "bEnableBlueLine", UIMin = "40", UIMax = "200", ClampMin = "20", ClampMax = "500"))
    float VerticalOffset = 80.0f;
    
    /** Minimum horizontal spacing required between nodes for rigidify to activate */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Routing|Advanced",
        meta = (EditCondition = "bEnableBlueLine && bEnableRigidifyCommand", UIMin = "50", UIMax = "300", ClampMin = "0"))
    float MinRigidifySpacing = 100.0f;
    
    /** Snap reroute nodes to grid after creation */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Routing|Advanced", meta = (EditCondition = "bEnableBlueLine"))
    bool bSnapReroutesToGrid = true;
    
    /** Grid snap size for reroute nodes (must be power of 2) */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Routing|Advanced",
        meta = (EditCondition = "bEnableBlueLine && bSnapReroutesToGrid", UIMin = "8", UIMax = "64", ClampMin = "1"))
    int32 GridSnapSize = 16;

    //==========================================================================
    // VISUALS - Wire & Pin Appearance
    //==========================================================================
    
    /** Wire thickness style */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Visuals", meta = (EditCondition = "bEnableBlueLine"))
    EBlueLineWireStyle WireStyle = EBlueLineWireStyle::Medium;
    
    /** Base wire thickness multiplier */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Visuals|Advanced",
        meta = (EditCondition = "bEnableBlueLine", UIMin = "0.5", UIMax = "3.0"))
    float WireThicknessMultiplier = 1.0f;
    
    /** Thickness of the stub line extending from pins */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Visuals|Advanced",
        meta = (EditCondition = "bEnableBlueLine", UIMin = "1", UIMax = "8", ClampMin = "0.5"))
    float StubThickness = 2.0f;
    
    /** Length of the straight line extending from the pin */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Visuals|Advanced",
        meta = (EditCondition = "bEnableBlueLine", UIMin = "10", UIMax = "100", ClampMin = "0"))
    float StubLength = 20.0f;
    
    /** If true, shows a small badge count when a pin has >1 connection */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Visuals", 
        meta = (EditCondition = "bEnableBlueLine", DisplayName = "Show Connection Count Badges"))
    bool bShowConnectionCount = true;
    
    /** Highlight wires on mouse hover */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Visuals", meta = (EditCondition = "bEnableBlueLine"))
    bool bHighlightWiresOnHover = true;

    /**
     * Allows BlueLine to write visual color changes into Blueprint graph assets
     * such as Smart Tag comment box colors.
     */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Visuals",
        meta = (EditCondition = "bEnableBlueLine", DisplayName = "Allow Blueprint Color Edits"))
    bool bAllowBlueprintColorEdits = true;
    
    /** Wire highlight brightness multiplier */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Visuals|Advanced",
        meta = (EditCondition = "bEnableBlueLine && bHighlightWiresOnHover", UIMin = "1.0", UIMax = "2.0"))
    float WireHighlightIntensity = 1.3f;

    //==========================================================================
    // FORMATTING - Node Auto-Layout
    //==========================================================================
    
    /** Primary formatting strategy */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Formatting", meta = (EditCondition = "bEnableBlueLine"))
    EBlueLineFormatStrategy FormatStrategy = EBlueLineFormatStrategy::Flow;
    
    /** Enable auto-format command (Shift+F) */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Formatting", meta = (EditCondition = "bEnableBlueLine"))
    bool bEnableAutoFormat = true;
    
    /** Horizontal spacing between nodes when formatting (pixels) */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Formatting|Spacing",
        meta = (EditCondition = "bEnableBlueLine && bEnableAutoFormat", UIMin = "150", UIMax = "600", ClampMin = "50"))
    float HorizontalSpacing = 300.0f;
    
    /** Vertical spacing between nodes when formatting (pixels) */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Formatting|Spacing",
        meta = (EditCondition = "bEnableBlueLine && bEnableAutoFormat", UIMin = "60", UIMax = "250", ClampMin = "30"))
    float VerticalSpacing = 120.0f;
    
    /** Padding around nodes when creating comment boxes */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Formatting|Spacing",
        meta = (EditCondition = "bEnableBlueLine && bEnableAutoFormat", UIMin = "10", UIMax = "100"))
    float CommentBoxPadding = 40.0f;
    
    /** Distance threshold for magnetic alignment snapping */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Formatting|Advanced",
        meta = (EditCondition = "bEnableBlueLine && bEnableAutoFormat", UIMin = "20", UIMax = "200"))
    float MagnetEvaluationDistance = 100.0f;
    
    /** Enable collision detection to prevent node overlap */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Formatting|Advanced", meta = (EditCondition = "bEnableBlueLine && bEnableAutoFormat"))
    bool bPreventNodeOverlap = true;
    
    /** Extra padding for collision detection */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Formatting|Advanced",
        meta = (EditCondition = "bEnableBlueLine && bEnableAutoFormat && bPreventNodeOverlap", UIMin = "0", UIMax = "50"))
    int32 CollisionPadding = 20;

    //==========================================================================
    // SMART TAGS - Auto-Tagging & Analysis
    //==========================================================================
    
    /** Enable smart auto-tagging features */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Smart Tags", meta = (EditCondition = "bEnableBlueLine"))
    bool bEnableSmartTags = true;
    
    /** Enable auto-tag command (Shift+T) */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Smart Tags", 
        meta = (EditCondition = "bEnableBlueLine && bEnableSmartTags", DisplayName = "Enable Auto-Tag Command"))
    bool bEnableAutoTagCommand = true;
    
    /** Minimum nodes in a cluster for auto-tagging (auto mode) */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Smart Tags|Thresholds",
        meta = (EditCondition = "bEnableBlueLine && bEnableSmartTags", UIMin = "2", UIMax = "10", ClampMin = "1"))
    int32 MinClusterSizeAuto = 3;
    
    /** Minimum nodes in a cluster for user selection mode */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Smart Tags|Thresholds",
        meta = (EditCondition = "bEnableBlueLine && bEnableSmartTags", UIMin = "1", UIMax = "5", ClampMin = "1"))
    int32 MinClusterSizeSelection = 2;
    
    /** Confidence threshold for semantic classification (higher = more conservative) */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Smart Tags|Thresholds",
        meta = (EditCondition = "bEnableBlueLine && bEnableSmartTags", UIMin = "1.0", UIMax = "10.0"))
    float SemanticConfidenceThreshold = 3.0f;
    
    /** Weight multiplier for cluster importance scoring */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Smart Tags|Advanced",
        meta = (EditCondition = "bEnableBlueLine && bEnableSmartTags", UIMin = "0.5", UIMax = "3.0"))
    float ClusterWeightMultiplier = 1.0f;
    
    /** Default gameplay tag container for untagged clusters */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Smart Tags|Advanced", 
        meta = (EditCondition = "bEnableBlueLine && bEnableSmartTags"))
    FGameplayTag DefaultClusterTag;

    //==========================================================================
    // LEVEL EDITOR - Pie Menu & Selection
    //==========================================================================
    
    /** Enable BlueLine pie menu in level editor (middle mouse) */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Level Editor", meta = (EditCondition = "bEnableBlueLine"))
    bool bEnableLevelPieMenu = true;
    
    /** Enable scope-based selection (Shift+Scroll in viewport) */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Level Editor", meta = (EditCondition = "bEnableBlueLine"))
    bool bEnableScopeSelection = true;
    
    /** Default selection radius for scope selection */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Level Editor|Radius",
        meta = (EditCondition = "bEnableBlueLine && bEnableScopeSelection", UIMin = "100", UIMax = "2000"))
    float DefaultSelectionRadius = 500.0f;
    
    /** Selection radius increment when scrolling */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Level Editor|Radius",
        meta = (EditCondition = "bEnableBlueLine && bEnableScopeSelection", UIMin = "10", UIMax = "200"))
    float SelectionRadiusIncrement = 50.0f;
    
    /** Maximum selection radius */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Level Editor|Radius",
        meta = (EditCondition = "bEnableBlueLine && bEnableScopeSelection", UIMin = "500", UIMax = "10000"))
    float MaxSelectionRadius = 5000.0f;
    
    /** Minimum selection radius */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Level Editor|Radius",
        meta = (EditCondition = "bEnableBlueLine && bEnableScopeSelection", UIMin = "50", UIMax = "500"))
    float MinSelectionRadius = 100.0f;
    
    /** Pie menu radius in pixels */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Level Editor|Pie Menu",
        meta = (EditCondition = "bEnableBlueLine && bEnableLevelPieMenu", UIMin = "80", UIMax = "200"))
    float PieMenuRadius = 120.0f;
    
    /** Dead zone in center of pie menu (pixels) */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Level Editor|Pie Menu",
        meta = (EditCondition = "bEnableBlueLine && bEnableLevelPieMenu", UIMin = "10", UIMax = "60"))
    float PieMenuDeadZone = 30.0f;
    
    /** Animation speed for pie menu open/close */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Level Editor|Pie Menu",
        meta = (EditCondition = "bEnableBlueLine && bEnableLevelPieMenu", UIMin = "5", UIMax = "30"))
    float PieMenuAnimationSpeed = 15.0f;
    
    /** Invert pie menu mouse Y-axis (top-to-bottom vs bottom-to-top) */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Level Editor|Pie Menu", meta = (EditCondition = "bEnableBlueLine && bEnableLevelPieMenu"))
    bool bInvertPieMenuYAxis = false;

    //==========================================================================
    // DEBUG - Visualization & Diagnostics
    //==========================================================================
    
    /** Enable debug visualization in level editor (selection spheres, etc.) */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Debug", meta = (EditCondition = "bEnableBlueLine"))
    bool bEnableDebugVisualization = true;
    
    /** Show graph analysis overlay in blueprint editor */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Debug|Blueprint", meta = (EditCondition = "bEnableBlueLine"))
    bool bShowGraphAnalysisOverlay = false;
    
    /** Display node complexity score on graph nodes */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Debug|Blueprint", 
        meta = (EditCondition = "bEnableBlueLine && bShowGraphAnalysisOverlay"))
    bool bShowNodeComplexity = false;
    
    /** Draw debug bounds during formatting operations */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Debug|Blueprint", meta = (EditCondition = "bEnableBlueLine"))
    bool bDebugDrawFormattingBounds = false;
    
    /** Log verbose debug information to output log */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Debug", meta = (EditCondition = "bEnableBlueLine"))
    bool bVerboseLogging = false;
    
    /** Default duration for debug draws (0 = one frame, -1 = persistent) */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Debug|Advanced",
        meta = (EditCondition = "bEnableBlueLine", UIMin = "-1", UIMax = "5.0"))
    float DebugDrawDuration = 0.05f;
    
    /** Debug sphere segments for selection visualization */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Debug|Advanced",
        meta = (EditCondition = "bEnableBlueLine && bEnableDebugVisualization", UIMin = "8", UIMax = "64"))
    int32 DebugSphereSegments = 32;
    
    /** Debug line thickness */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Debug|Advanced",
        meta = (EditCondition = "bEnableBlueLine", UIMin = "0.5", UIMax = "5.0"))
    float DebugLineThickness = 1.5f;

    /** Expose internal demo K2 nodes in the Blueprint action menu */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Debug|Advanced", meta = (EditCondition = "bEnableBlueLine"))
    bool bExposeDemoNodes = false;

    //==========================================================================
    // EXPORT - File Operations
    //==========================================================================
    
    /** Default export directory for graph text exports */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Export", meta = (EditCondition = "bEnableBlueLine", ContentDir))
    FDirectoryPath DefaultExportPath;
    
    /** Default directory for subsystem extraction */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Export", meta = (EditCondition = "bEnableBlueLine", ContentDir))
    FDirectoryPath DefaultSubsystemPath;
    
    /** Include node comments in text exports */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Export|Options", meta = (EditCondition = "bEnableBlueLine"))
    bool bExportIncludeComments = true;
    
    /** Include variable types in text exports */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Export|Options", meta = (EditCondition = "bEnableBlueLine"))
    bool bExportIncludeVariableTypes = true;
    
    /** Export default values */
    UPROPERTY(EditAnywhere, Config, Category = "BlueLine|Export|Options", meta = (EditCondition = "bEnableBlueLine"))
    bool bExportIncludeDefaultValues = false;

public:
    virtual FName GetContainerName() const override { return FName("Editor"); }
    virtual FName GetCategoryName() const override { return FName("Plugins"); }
    virtual FName GetSectionName() const override { return FName("BlueLine"); }
    
    /** Get settings description for tooltip */
    virtual FText GetSectionDescription() const override 
    { 
        return NSLOCTEXT("BlueLine", "SettingsDescription", "Configure BlueLine Blueprint Editor Toolkit features");
    }

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    
    /** Reset all settings to defaults */
    UFUNCTION(CallInEditor, Category = "BlueLine")
    void ResetToDefaults();
    
    /** Export settings to a JSON file */
    UFUNCTION(CallInEditor, Category = "BlueLine")
    void ExportSettings();
    
    /** Import settings from a JSON file */
    UFUNCTION(CallInEditor, Category = "BlueLine")
    void ImportSettings();
#endif

public:
    DECLARE_MULTICAST_DELEGATE(FOnBlueLineSettingsChanged);
    static FOnBlueLineSettingsChanged OnSettingsChanged;
    
    /** Static accessor for runtime settings */
    static const UBlueLineEditorSettings* Get() { return GetDefault<UBlueLineEditorSettings>(); }
    
    /** Check if BlueLine is effectively enabled */
    bool IsBlueLineEnabled() const { return bEnableBlueLine; }
};
