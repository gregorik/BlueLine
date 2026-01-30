// Copyright YourTeamName. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UBlueLineEditorSettings.generated.h"

UCLASS(config = EditorPerProjectUserSettings, defaultconfig, meta = (DisplayName = "BlueLine Graph"))
class BLUELINEGRAPH_API UBlueLineEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBlueLineEditorSettings();

	/** Enable the Manhattan-style logic (Stubs and formatting). */
	UPROPERTY(EditAnywhere, config, Category = "Visuals")
	bool bEnableManhattanRouting;

	/** Length of the straight line extending from the pin. */
	UPROPERTY(EditAnywhere, config, Category = "Visuals")
	float StubLength;

	/** Thickness of the stub line. */
	UPROPERTY(EditAnywhere, config, Category = "Visuals")
	float StubThickness;

	/** If true, shows a small badge count when a pin has >1 connection. */
	UPROPERTY(EditAnywhere, config, Category = "Visuals")
	bool bShowConnectionCount;

	// Formatting settings (kept from previous steps)
	UPROPERTY(EditAnywhere, config, Category = "Formatting")
	float FormatterPadding;

	UPROPERTY(EditAnywhere, config, Category = "Formatting")
	float MagnetEvaluationDistance;

public:
	virtual FName GetContainerName() const override { return FName("Editor"); }
	virtual FName GetCategoryName() const override { return FName("Plugins"); }
	virtual FName GetSectionName() const override { return FName("BlueLine Graph"); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	DECLARE_MULTICAST_DELEGATE(FOnBlueLineSettingsChanged);
	static FOnBlueLineSettingsChanged OnSettingsChanged;
};