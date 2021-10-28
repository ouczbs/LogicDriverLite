// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#include "SMEditorModule.h"
#include "AssetRegistryModule.h"
#include "EdGraphUtilities.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "MessageLogInitializationOptions.h"
#include "MessageLogModule.h"
#include "Commands/SMEditorCommands.h"
#include "Compilers/SMKismetCompiler.h"
#include "TypeActions/SMBlueprintAssetTypeActions.h"
#include "KismetCompilerModule.h"
#include "Blueprints/SMBlueprint.h"
#include "Style/SMEditorStyle.h"
#include "Config/SMProjectEditorSettings.h"
#include "ISettingsModule.h"
#include "PropertyEditorModule.h"
#include "Customization/SMEditorCustomization.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/SMGraphFactory.h"
#include "Interfaces/IPluginManager.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMVersionUtils.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SMEditorModule"

void FSMEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	FSMEditorStyle::Initialize();
	FSMEditorCommands::Register();
	RegisterSettings();

	// Register blueprint compiler -- primarily seems to be used when creating a new BP.
	IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);
	KismetCompilerModule.GetCompilers().Add(&SMBlueprintCompiler);

	// This is needed for actually pressing compile on the BP.
	FKismetCompilerContext::RegisterCompilerForBP(USMBlueprint::StaticClass(), &FSMEditorModule::GetCompilerForStateMachineBP);

	// Register graph related factories.
	SMGraphPanelNodeFactory = MakeShareable(new FSMGraphPanelNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(SMGraphPanelNodeFactory);

	SMGraphPinNodeFactory = MakeShareable(new FSMGraphPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(SMGraphPinNodeFactory);

	RefreshAllNodesDelegateHandle = FSMBlueprintEditorUtils::OnRefreshAllNodesEvent.AddStatic(&FSMBlueprintEditorUtils::HandleRefreshAllNodes);
	
	// Register details customization.
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	ClassLayoutContext.RegisterCustomClassLayout(PropertyModule, USMGraphNode_StateNode::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSMNodeCustomization::MakeInstance));
	ClassLayoutContext.RegisterCustomClassLayout(PropertyModule, USMGraphNode_StateMachineStateNode::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSMStateMachineReferenceCustomization::MakeInstance));
	ClassLayoutContext.RegisterCustomClassLayout(PropertyModule, USMGraphNode_TransitionEdge::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSMTransitionEdgeCustomization::MakeInstance));
	ClassLayoutContext.RegisterCustomClassLayout(PropertyModule, USMGraphNode_ConduitNode::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSMNodeCustomization::MakeInstance));
	ClassLayoutContext.RegisterCustomClassLayout(PropertyModule, USMGraphNode_AnyStateNode::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSMNodeCustomization::MakeInstance));

	// Covers all node instances.
	ClassLayoutContext.RegisterCustomClassLayout(PropertyModule, USMNodeInstance::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSMNodeInstanceCustomization::MakeInstance));

	// Register asset categories.
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Register state machines under our own category menu and under the Blueprint menu.
	TypeActionContext.RegisterAssetTypeAction(AssetTools, MakeShareable(new FSMBlueprintAssetTypeActions(EAssetTypeCategories::Blueprint | EAssetTypeCategories::Basic)));
	// Hide base instance from showing up in misc menu.
	TypeActionContext.RegisterAssetTypeAction(AssetTools, MakeShareable(new FSMInstanceAssetTypeActions(EAssetTypeCategories::None)));
	/* // We need to call FMessageLog for registering this log module to have any effect.
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions InitOptions;
	InitOptions.bShowFilters = true;
	InitOptions.bShowPages = true;
	MessageLogModule.RegisterLogListing("LogLogicDriver", LOCTEXT("LogicDriverLog", "Logic Driver Log"), InitOptions);
	*/
	
	BeginPieHandle = FEditorDelegates::BeginPIE.AddRaw(this, &FSMEditorModule::BeginPIE);
	EndPieHandle = FEditorDelegates::EndPIE.AddRaw(this, &FSMEditorModule::EndPie);

	const USMProjectEditorSettings* ProjectEditorSettings = FSMBlueprintEditorUtils::GetProjectEditorSettings();
	if (ProjectEditorSettings->bUpdateAssetsOnStartup)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		FilesLoadedHandle = AssetRegistryModule.Get().OnFilesLoaded().AddStatic(&FSMVersionUtils::UpdateBlueprintsToNewVersion);
	}

	DisplayUpdateNotification();
}

void FSMEditorModule::ShutdownModule()
{
	FKismetEditorUtilities::UnregisterAutoBlueprintNodeCreation(this);

	// Unregister all the asset types that we registered.
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
	TypeActionContext.UnregisterAssetTypeActions(AssetTools);

	FEdGraphUtilities::UnregisterVisualNodeFactory(SMGraphPanelNodeFactory);
	FEdGraphUtilities::UnregisterVisualPinFactory(SMGraphPinNodeFactory);

	FSMBlueprintEditorUtils::OnRefreshAllNodesEvent.Remove(RefreshAllNodesDelegateHandle);
	
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	ClassLayoutContext.UnregisterCustomClassLayouts(PropertyModule);
	
	IKismetCompilerInterface& KismetCompilerModule = FModuleManager::GetModuleChecked<IKismetCompilerInterface>("KismetCompiler");
	KismetCompilerModule.GetCompilers().Remove(&SMBlueprintCompiler);

	/*
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.UnregisterLogListing("LogLogicDriver");
	*/
	
	FSMEditorCommands::Unregister();
	FSMEditorStyle::Shutdown();
	UnregisterSettings();

	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	FEditorDelegates::BeginPIE.Remove(BeginPieHandle);
	FEditorDelegates::EndPIE.Remove(EndPieHandle);

	if (FilesLoadedHandle.IsValid() && FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnFilesLoaded().Remove(FilesLoadedHandle);
	}
}

TSharedPtr<FKismetCompilerContext> FSMEditorModule::GetCompilerForStateMachineBP(UBlueprint* BP,
	FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions)
{
	return TSharedPtr<FKismetCompilerContext>(new FSMKismetCompilerContext(CastChecked<USMBlueprint>(BP), InMessageLog, InCompileOptions));
}

void FSMEditorModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Editor", "ContentEditors", "StateMachineEditor",
			LOCTEXT("SMEditorSettingsName", "Logic Driver Editor"),
			LOCTEXT("SMEditorSettingsDescription", "Configure the state machine editor."),
			GetMutableDefault<USMEditorSettings>());

		SettingsModule->RegisterSettings("Project", "Editor", "StateMachineEditor",
			LOCTEXT("SMProjectEditorSettingsName", "Logic Driver"),
			LOCTEXT("SMProjectEditorSettingsDescription", "Configure the state machine editor."),
			GetMutableDefault<USMProjectEditorSettings>());
	}
}

void FSMEditorModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Editor", "ContentEditors", "StateMachineEditor");
		SettingsModule->UnregisterSettings("Project", "Editor", "StateMachineEditor");
	}
}

void FSMEditorModule::BeginPIE(bool bValue)
{
	bPlayingInEditor = true;
}

void FSMEditorModule::EndPie(bool bValue)
{
	bPlayingInEditor = false;
}

void FSMEditorModule::DisplayUpdateNotification()
{
	const FString PluginName = "SMSystem";
	IPluginManager& PluginManager = IPluginManager::Get();

	TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(PluginName);
	if (Plugin->IsEnabled())
	{
		const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();

		USMProjectEditorSettings* ProjectEditorSettings = FSMBlueprintEditorUtils::GetMutableProjectEditorSettings();
		if (ProjectEditorSettings->InstalledVersion != Descriptor.VersionName)
		{
			const bool bIsUpdate = ProjectEditorSettings->InstalledVersion.Len() > 0;
			
			ProjectEditorSettings->InstalledVersion = Descriptor.VersionName;
			ProjectEditorSettings->SaveConfig();

			if (!bIsUpdate)
			{
				return;
			}
			
			if (ProjectEditorSettings->bDisplayUpdateNotification)
			{
				TArray<FString> PreviousInstalledPlugins;
				GConfig->GetArray(TEXT("PluginBrowser"), TEXT("InstalledPlugins"), PreviousInstalledPlugins, GEditorPerProjectIni);

				if (PreviousInstalledPlugins.Contains(PluginName))
				{
					// We only want to display the popup if the plugin was previously installed. Not always accurate so we check if there was a previous version.
					
					const FString DisplayString = !bIsUpdate ? FString::Printf(TEXT("Logic Driver Lite version %s installed"), *Descriptor.VersionName) :
															FString::Printf(TEXT("Logic Driver Lite updated to version %s"), *Descriptor.VersionName);
					FNotificationInfo Info(FText::FromString(DisplayString));
					Info.bFireAndForget = false;
					Info.bUseLargeFont = true;
					Info.bUseThrobber = false;
					Info.FadeOutDuration = 0.25f;
					Info.ButtonDetails.Add(FNotificationButtonInfo(LOCTEXT("LogicDriverUpdateViewPatchNotes", "View Patch Notes..."), LOCTEXT("LogicDriverUpdateViewPatchTT", "Open the webbrowser to view patch notes"), FSimpleDelegate::CreateRaw(this, &FSMEditorModule::OnViewNewPatchNotesClicked)));
					Info.ButtonDetails.Add(FNotificationButtonInfo(LOCTEXT("LogicDriverUpdatePopupDismiss", "Dismiss"), LOCTEXT("LogicDriverUpdatePopupDismissTT", "Dismiss this notification"), FSimpleDelegate::CreateRaw(this, &FSMEditorModule::OnDismissUpdateNotificationClicked)));

					NewVersionNotification = FSlateNotificationManager::Get().AddNotification(Info);
					NewVersionNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
				}
			}
		}
	}
}

void FSMEditorModule::OnViewNewPatchNotesClicked()
{
	FString Version = FSMBlueprintEditorUtils::GetProjectEditorSettings()->InstalledVersion;

	// Strip '.' out of version.
	TArray<FString> VersionArray;
	Version.ParseIntoArray(VersionArray, TEXT("."));
	Version = FString::Join(VersionArray, TEXT(""));
	
	const FString Url = FString::Printf(TEXT("https://logicdriver.recursoft.net/docs/pages/litechangelog/#version-%s"), *Version);
	FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
	NewVersionNotification.Pin()->ExpireAndFadeout();
}

void FSMEditorModule::OnDismissUpdateNotificationClicked()
{
	NewVersionNotification.Pin()->ExpireAndFadeout();
}

#undef LOCTEXT_NAMESPACE

