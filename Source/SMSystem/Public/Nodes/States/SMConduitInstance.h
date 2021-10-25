// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SMStateInstance.h"
#include "SMConduitInstance.generated.h"


/**
 * The base class for conduit nodes.
 */
UCLASS(NotBlueprintable, BlueprintType, classGroup = "State Machine", hideCategories = (SMConduitInstance), meta = (DisplayName = "Conduit Class"))
class SMSYSTEM_API USMConduitInstance : public USMStateInstance_Base
{
	GENERATED_BODY()

public:
	USMConduitInstance();

	/** Sets whether this node is allowed to evaluate or not. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void SetCanEvaluate(bool bValue);

	/** Check whether this node is allowed to evaluate. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool GetCanEvaluate() const;
	
public:
	/**
	 * This conduit will be evaluated with inbound and outbound transitions.
	 * If any transition fails the entire transition fails. In that case the
	 * state leading to this conduit will not take this transition.
	 *
	 * This makes the behavior similar to AnimGraph conduits.
	 */
	UPROPERTY(EditDefaultsOnly, Category = Conduit)
	bool bEvalWithTransitions;

	/**
	 * If this conduit is allowed to evaluate.
	 */
	UPROPERTY(EditDefaultsOnly, Category = Conduit, meta=(DisplayName = "Can Evaluate"))
	bool bCanEvaluate;
};

