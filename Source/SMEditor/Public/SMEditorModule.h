// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#pragma once

#include "ISMEditorModule.h"
#include "TypeActions/SMBlueprintAssetTypeActions.h"
#include "EdGraphUtilities.h"
#include "Compilers/SMKismetCompiler.h"
#include "Context/RegisterContext.h"
#include "EdGraphUtilities.h"

#define LOCTEXT_NAMESPACE "SMEditorModule"

class FExtensibilityManager;

class FSMEditorModule : public ISMEditorModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Gets the extensibility managers for outside entities to extend this editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() const override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() const override { return ToolBarExtensibilityManager; }

	/** If the user has pressed play in editor. */
	virtual bool IsPlayingInEditor() const override { return bPlayingInEditor; }
private:
	static TSharedPtr<FKismetCompilerContext> GetCompilerForStateMachineBP(UBlueprint* BP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions);

	void RegisterSettings();
	void UnregisterSettings();

	void BeginPIE(bool bValue);
	void EndPie(bool bValue);

	void DisplayUpdateNotification();
	void OnViewNewPatchNotesClicked();
	void OnDismissUpdateNotificationClicked();
	
private:

	TSharedPtr<class FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<class FExtensibilityManager> ToolBarExtensibilityManager;

	TSharedPtr<FGraphPanelNodeFactory> SMGraphPanelNodeFactory;
	TSharedPtr<FGraphPanelPinFactory> SMGraphPinNodeFactory;

	FSMKismetCompiler SMBlueprintCompiler;

	FDelegateHandle RefreshAllNodesDelegateHandle;

	FDelegateHandle BeginPieHandle;
	FDelegateHandle EndPieHandle;
	FDelegateHandle FilesLoadedHandle;

	/** Notification popup that the plugin has updated. */
	TWeakPtr<SNotificationItem> NewVersionNotification;
	
	/** If the user has pressed play in editor. */
	bool bPlayingInEditor = false;

private:
	FTypeActionContext TypeActionContext;
	FClassLayoutContext ClassLayoutContext;
};

IMPLEMENT_MODULE(FSMEditorModule, SMEditor)


#undef LOCTEXT_NAMESPACE

