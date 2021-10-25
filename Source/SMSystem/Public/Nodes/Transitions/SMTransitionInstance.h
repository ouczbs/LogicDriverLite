// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SMNodeInstance.h"
#include "SMNode_Info.h"
#include "SMTransitionInstance.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTransitionEnteredSignature, class USMTransitionInstance*, TransitionInstance);

/**
 * The base class for transition connections.
 */
UCLASS(NotBlueprintable, BlueprintType, classGroup = "State Machine", hideCategories = (SMTransitionInstance), meta = (DisplayName = "Transition Class"))
class SMSYSTEM_API USMTransitionInstance : public USMNodeInstance
{
	GENERATED_BODY()

public:
	USMTransitionInstance();

	/** Sets whether this node is allowed to evaluate or not. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void SetCanEvaluate(bool bValue);

	/** Check whether this transition is allowed to evaluate. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool GetCanEvaluate() const;
	
	/** The state this transition leaves from. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	class USMStateInstance_Base* GetPreviousStateInstance() const;

	/** The state this transition leads to. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	class USMStateInstance_Base* GetNextStateInstance() const;

	/** Return read only information about the owning transition. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void GetTransitionInfo(FSMTransitionInfo& Transition) const;

public:
	/**
	 * Lower number transitions will be evaluated first.
	 */
	UPROPERTY(EditDefaultsOnly, Category = Transition)
	int32 PriorityOrder;

	/**
	 * If this transition is allowed to evaluate conditionally.
	 */
	UPROPERTY(EditDefaultsOnly, Category = Transition, meta=(DisplayName = "Can Evaluate Conditionally"))
	bool bCanEvaluate;

	/** If this transition can evaluate from autobinded events. */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = Transition)
	bool bCanEvaluateFromEvent;
	
	/**
	 * Setting to false forces this transition to never evaluate on the same tick as Start State.
	 * Only checked if this transition's from state has bEvalTransitionsOnStart set to true.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = Transition, meta = (NoResetToDefault))
	bool bCanEvalWithStartState;

	/** Called when this transition has been entered from the previous state. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|Node Instance")
	FOnTransitionEnteredSignature OnTransitionEnteredEvent;
};