// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SMStateInstance.h"
#include "SMStateMachineInstance.generated.h"


/**
 * The base class for state machine nodes. These are different from regular state machines (SMInstance) in that they can be assigned to a state machine directly
 * either in the class defaults or in the details panel of a nested state machine node. Think of this as giving a state machine a 'type' which allows you to
 * identify it in rule behavior. This is still considered a state as well which allows access to hooking into Start, Update, and End events even when placed as
 * a nested state machine.
 */
UCLASS(NotBlueprintable, BlueprintType, classGroup = "State Machine", hideCategories = (SMStateMachineInstance), meta = (DisplayName = "State Machine Class"))
class SMSYSTEM_API USMStateMachineInstance : public USMStateInstance_Base
{
	GENERATED_BODY()

public:
	USMStateMachineInstance();

	/** Retrieve all contained state instances defined within the state machine graph this instance represents. These can be States, State Machines, and Conduits. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void GetAllStateInstances(TArray<USMStateInstance_Base*>& StateInstances) const;

	/** Return the entry states of the state machine. Generally one unless parallel states are used. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void GetEntryStates(TArray<USMStateInstance_Base*>& EntryStates) const;
	
	// USMNodeInstance
	/** Special handling to retrieve the real FSM node in the event this is a state machine reference. */
	virtual const FSMNode_Base* GetOwningNodeContainer() const override;
	// ~USMNodeInstance
	
public:
	/**
	 * Wait for an end state to be hit before evaluating transitions or being considered an end state itself.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "State Machine")
	bool bWaitForEndState;
	
	/**
	 * When true the current state is reused on end/start.
	 * When false the current state is cleared on end and the initial state used on start.
	 * References will inherit this behavior.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "State Machine")
	bool bReuseCurrentState;

	/**
	 * Do not reuse if in an end state.
	 * References will inherit this behavior.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "State Machine", meta = (EditCondition = "bReuseCurrentState"))
	bool bReuseIfNotEndState;
};

