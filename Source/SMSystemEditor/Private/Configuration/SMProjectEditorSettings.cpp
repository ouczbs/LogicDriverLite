// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#include "SMProjectEditorSettings.h"

USMProjectEditorSettings::USMProjectEditorSettings()
{
	bUpdateAssetsOnStartup = true;
	bDisplayAssetUpdateProgress = true;
	bDisplayMemoryLimitsOnCompile = true;
	bAlwaysDisplayStructMemoryUsage = false;
	StructMemoryLimitWarningThreshold = 0.9f;
	bValidateInstanceOnCompile = true;
	bWarnIfChildrenAreOutOfDate = true;
	bConfigureNewConduitsAsTransitions = true;
	bDisplayUpdateNotification = true;
	InstalledVersion = "";
	bEnableBlueprintMenuExtenders = true;
	bEnableBlueprintToolbarExtenders = true;
}

void USMProjectEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SaveConfig();
}
