#pragma once

#include "CoreMinimal.h"
#include "PropertyEditorModule.h"
class IAssetTools;
class IAssetTypeActions;


class FTypeActionContext
{
public:
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);
	void UnregisterAssetTypeActions(IAssetTools& AssetTools);

private:
	TArray<TSharedPtr<IAssetTypeActions>> CreatedAssetTypeActions;
};

class FClassLayoutContext
{
public:
	void RegisterCustomClassLayout(FPropertyEditorModule& PropertyEditorModule , FName ClassName , FOnGetDetailCustomizationInstance DetailLayoutDelegate);
	void UnregisterCustomClassLayouts(FPropertyEditorModule& PropertyEditorModule);

private:
	TArray<FName> CreatedAssetDetailName;
};
