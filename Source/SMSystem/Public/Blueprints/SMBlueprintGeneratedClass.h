// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#pragma once

#include "Engine/BlueprintGeneratedClass.h"
#include "SMBlueprintGeneratedClass.generated.h"



UCLASS()
class SMSYSTEM_API USMBlueprintGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_UCLASS_BODY()

public:
	// UClass
	virtual void PurgeClass(bool bRecompilingOnLoad) override;
	// ~UClass

	/** The root state machine Guid- set by the compiler. */
	void SetRootGuid(const FGuid& Guid);

	/** The root state machine Guid. */
	const FGuid& GetRootGuid() const { return RootGuid; }

protected:
	UPROPERTY(meta=(BlueprintCompilerGeneratedDefaults))
	FGuid RootGuid;
};

UCLASS()
class SMSYSTEM_API USMNodeBlueprintGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_UCLASS_BODY()

};