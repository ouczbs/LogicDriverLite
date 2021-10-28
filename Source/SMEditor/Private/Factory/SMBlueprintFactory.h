// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "SMBlueprintFactory.generated.h"

UCLASS(HideCategories = Object, MinimalAPI)
class USMBlueprintFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// UFactory
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context,
	                                  FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context,
	                                  FFeedbackContext* Warn) override;
	virtual FString GetDefaultNewAssetName() const override;
	// ~UFactory

private:
	// The type of blueprint that will be created
	UPROPERTY(EditAnywhere, Category=StateMachineBlueprintFactory)
	TEnumAsByte<EBlueprintType> BlueprintType;

	// The parent class of the created blueprint
	UPROPERTY(EditAnywhere, Category= StateMachineBlueprintFactory, meta=(AllowAbstract = ""))
	TSubclassOf<class USMInstance> ParentClass;

};
