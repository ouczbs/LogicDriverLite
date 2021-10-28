// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#include "TypeActions/SMBlueprintAssetTypeActions.h"
#include "Blueprints/SMBlueprintEditor.h"
#include "Blueprints/SMBlueprint.h"
#include "SMInstance.h"

#define LOCTEXT_NAMESPACE "SMBlueprintAssetTypeActions"

FSMAssetTypeActions_Base::FSMAssetTypeActions_Base(uint32 Categories) : AssetCategory(Categories)
{
}

uint32 FSMAssetTypeActions_Base::GetCategories()
{
	return AssetCategory;
}

FSMBlueprintAssetTypeActions::FSMBlueprintAssetTypeActions(uint32 InAssetCategory)
	: FSMAssetTypeActions_Base(InAssetCategory)
{
}

FText FSMBlueprintAssetTypeActions::GetName() const
{
	return LOCTEXT("FSMBlueprintAssetTypeActions", "State Machine Blueprint");
}

FColor FSMBlueprintAssetTypeActions::GetTypeColor() const
{
	return FColor(10, 175, 241);
}

UClass* FSMBlueprintAssetTypeActions::GetSupportedClass() const
{
	return USMBlueprint::StaticClass();
}

void FSMBlueprintAssetTypeActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (USMBlueprint* StateMachineBP = Cast<USMBlueprint>(*ObjIt))
		{
			TSharedRef<FSMBlueprintEditor> BlueprintEditor(new FSMBlueprintEditor());
			BlueprintEditor->InitSMBlueprintEditor(Mode, EditWithinLevelEditor, StateMachineBP);
		}
	}
}

FSMInstanceAssetTypeActions::FSMInstanceAssetTypeActions(uint32 InAssetCategory) : FSMAssetTypeActions_Base(InAssetCategory)
{
}

FText FSMInstanceAssetTypeActions::GetName() const
{
	return LOCTEXT("FSMGraphAssetTypeActions", "State Machine Instance");
}

FColor FSMInstanceAssetTypeActions::GetTypeColor() const
{
	return FColor(0, 0, 0);
}

UClass* FSMInstanceAssetTypeActions::GetSupportedClass() const
{
	return USMInstance::StaticClass();
}

#undef LOCTEXT_NAMESPACE
