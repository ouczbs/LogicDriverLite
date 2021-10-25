// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SMProjectEditorSettings.generated.h"


UCLASS(config = Editor)
class USMProjectEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	USMProjectEditorSettings();

	UPROPERTY(config, VisibleAnywhere, Category = "Version Updates (Lite)")
	FString InstalledVersion;
	
	/**
	 * Automatically update assets saved by older versions to the most current version. It is strongly recommended to leave this on.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Version Updates (Lite)")
	bool bUpdateAssetsOnStartup;

	/**
	 * Display a progress bar when updating assets to a new version.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Version Updates (Lite)", meta = (EditCondition = "bUpdateAssetsOnStartup"))
	bool bDisplayAssetUpdateProgress;

	/**
	 * Display a popup with a link to the patch notes when a new version is detected.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Version Updates (Lite)")
	bool bDisplayUpdateNotification;

	/**
	 * Warn if approaching Blueprint memory limits on a compile.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Validation|Memory")
	bool bDisplayMemoryLimitsOnCompile;

	/**
	 * Display the used struct memory as an info message on compile. 
	 */
	UPROPERTY(config, EditAnywhere, Category = "Validation|Memory", meta = (EditCondition = "bDisplayMemoryLimitsOnCompile"))
	bool bAlwaysDisplayStructMemoryUsage;
	
	/**
	 * The percent of used struct memory that must be reached before a warning is triggered.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Validation|Memory", meta = (EditCondition = "bDisplayMemoryLimitsOnCompile", UIMin = 0.0, UIMax = 1.0))
	float StructMemoryLimitWarningThreshold;
	
	/**
	 * Perform additional validation when compiling the state machine default object by attempting a low level instantiation.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Validation")
	bool bValidateInstanceOnCompile;
	
	/**
	 * Children which reference a parent state machine graph risk being out of date if a package the parent references is modified.
	 * Compiling the package will signal that affected state machine children need to be compiled, however if you start a PIE session
	 * instead of pressing the compile button, the children may not be updated. In this case the state machine compiler will attempt to warn you.
	 */
	bool bWarnIfChildrenAreOutOfDate;
	
	/**
	 * Newly placed conduits will automatically be configured as transitions.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Conduits")
	bool bConfigureNewConduitsAsTransitions;

	/**
	 * Add all Kismet toolbar extenders to the state machine editor. This allows items from third party plugins to be visible in Logic Driver.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Plugin Compatibility")
	bool bEnableBlueprintToolbarExtenders;

	/**
	 * Add all Kismet menu extenders to the state machine editor. This allows items from third party plugins to be visible in Logic Driver.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Plugin Compatibility")
	bool bEnableBlueprintMenuExtenders;

	// UObject
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// ~UObject
};
