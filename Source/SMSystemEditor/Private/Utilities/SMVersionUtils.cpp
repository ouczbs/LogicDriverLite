// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#include "SMVersionUtils.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Misc/ScopedSlowTask.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SMVersionUtils"

static TWeakPtr<SNotificationItem> WrongVersionNotification;

int32 FSMVersionUtils::GetCurrentBlueprintVersion()
{
	return 2;
}

void FSMVersionUtils::UpdateBlueprintsToNewVersion()
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry")).Get();
	
	TArray<FAssetData> OutAssets;
	AssetRegistry.GetAssetsByClass(USMBlueprint::StaticClass()->GetFName(), OutAssets, true);

	bool bIsAssetVersionNotSupported = false;
	
	TArray<FAssetData> AssetsToUpdate;
	for (const FAssetData& Asset : OutAssets)
	{
		int32 Version;
		
		const bool bVersionFound = Asset.GetTagValue(GET_MEMBER_NAME_CHECKED(USMBlueprint, AssetVersion), Version);
		if (!bVersionFound || !IsStateMachineUpToDate(Version))
		{
			AssetsToUpdate.Add(Asset);
			continue;
		}

		if (IsStateMachineFromNewerPluginVersion(Version))
		{
			bIsAssetVersionNotSupported = true;
		}
	}

	if (bIsAssetVersionNotSupported)
	{
		// There are assets from a newer version of the plugin.
		FNotificationInfo Info(LOCTEXT("LogicDriverAssetsFromNewerVersion", "Logic Driver assets are from a newer version of the plugin!\nPlease update Logic Driver and verify your team is using the same version."));
		Info.bFireAndForget = false;
		Info.bUseLargeFont = false;
		Info.bUseThrobber = false;
		Info.FadeOutDuration = 0.25f;
		Info.ButtonDetails.Add(FNotificationButtonInfo(LOCTEXT("LogicDriverWrongVersionDismiss", "Dismiss"), LOCTEXT("LogicDriverWrongVersionDismissTT", "Dismiss this notification"), FSimpleDelegate::CreateStatic(&FSMVersionUtils::DismissWrongVersionNotification)));
		
		WrongVersionNotification = FSlateNotificationManager::Get().AddNotification(Info);
		WrongVersionNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
	}

	if (AssetsToUpdate.Num() > 0)
	{
		FScopedSlowTask Feedback(AssetsToUpdate.Num(), NSLOCTEXT("LogicDriver", "LogicDriverAssetUpdate", "Updating Logic Driver assets to the current version..."));

		if (FSMBlueprintEditorUtils::GetProjectEditorSettings()->bDisplayAssetUpdateProgress)
		{
			Feedback.MakeDialog(true);
		}
		
		for (FAssetData& Asset : AssetsToUpdate)
		{
			USMBlueprint* SMBlueprint = CastChecked<USMBlueprint>(Asset.GetAsset());
			
			TArray<USMGraphNode_Base*> GraphNodes;
			FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_Base>(SMBlueprint, GraphNodes);

			for (USMGraphNode_Base* Node : GraphNodes)
			{
				Node->ConvertToCurrentVersion(false);
			}

			SetToLatestVersion(SMBlueprint);
			SMBlueprint->MarkPackageDirty();
			
			Feedback.CompletedWork += 1;
		}
	}
}

bool FSMVersionUtils::IsStateMachineUpToDate(int32 CompareVersion)
{
	return CompareVersion >= GetCurrentBlueprintVersion();
}

bool FSMVersionUtils::IsStateMachineUpToDate(USMBlueprint* Blueprint)
{
	const int32 Version = Blueprint->AssetVersion;
	return IsStateMachineUpToDate(Version);
}

bool FSMVersionUtils::IsStateMachineFromNewerPluginVersion(int32 CompareVersion)
{
	return CompareVersion > GetCurrentBlueprintVersion();
}

void FSMVersionUtils::SetToLatestVersion(UBlueprint* Blueprint)
{
	if (USMBlueprint* SMBlueprint = Cast<USMBlueprint>(Blueprint))
	{
		SMBlueprint->AssetVersion = GetCurrentBlueprintVersion();
	}
}

void FSMVersionUtils::DismissWrongVersionNotification()
{
	WrongVersionNotification.Pin()->ExpireAndFadeout();
}

#undef LOCTEXT_NAMESPACE
