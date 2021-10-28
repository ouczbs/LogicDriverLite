#include "RegisterContext.h"
#include "IAssetTools.h"

void FTypeActionContext::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	CreatedAssetTypeActions.Add(Action);
}
void FTypeActionContext::UnregisterAssetTypeActions(IAssetTools& AssetTools)
{
	for (int32 Index = 0; Index < CreatedAssetTypeActions.Num(); ++Index)
	{
		AssetTools.UnregisterAssetTypeActions(CreatedAssetTypeActions[Index].ToSharedRef());
	}
}
void FClassLayoutContext::RegisterCustomClassLayout(FPropertyEditorModule& PropertyEditorModule, FName ClassName, FOnGetDetailCustomizationInstance DetailLayoutDelegate) {
	PropertyEditorModule.RegisterCustomClassLayout(ClassName, DetailLayoutDelegate);
	CreatedAssetDetailName.Add(ClassName);
}
void FClassLayoutContext::UnregisterCustomClassLayouts(FPropertyEditorModule& PropertyEditorModule) {
	for (int32 Index = 0; Index < CreatedAssetDetailName.Num(); ++Index)
	{
		PropertyEditorModule.UnregisterCustomClassLayout(CreatedAssetDetailName[Index]);
	}
}