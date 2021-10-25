// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SMNodeInstance.h"
#include "SMNode_Info.h"
#include "SMStateInstance.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStateBeginSignature, class USMStateInstance_Base*, StateInstance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStateUpdateSignature, class USMStateInstance_Base*, StateInstance, float, DeltaSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStateEndSignature, class USMStateInstance_Base*, StateInstance);

/**
 * The abstract base class for all state type nodes including state machine nodes and conduits.
 */
UCLASS(Abstract, NotBlueprintable, BlueprintType, classGroup = "State Machine", hideCategories = (SMStateInstance_Base), meta = (DisplayName = "State Class Base"))
class SMSYSTEM_API USMStateInstance_Base : public USMNodeInstance
{
	GENERATED_BODY()

public:
	USMStateInstance_Base();
	
	// USMNodeInstance
	/** If this state is an end state. */
	virtual bool IsInEndState() const override;
	// ~USMNodeInstance
	
	/** Return read only information about the owning state. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void GetStateInfo(FSMStateInfo& State) const;
	
	/** Checks if this state is a state machine. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool IsStateMachine() const;

	/** Force set the active flag of this state. This call is replicated and can be called from the server or from a client that is not a simulated proxy.
	 * @param bValue True activates the state, false deactivates the state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void SetActive(bool bValue);
	
	/**
	 * Signals to the owning state machine to process transition evaluation.
	 * This is similar to calling Update on the owner root state machine, however state update logic (Tick) won't execute.
	 * All transitions are evaluated as normal starting from the root state machine down.
	 * Depending on super state transitions it's possible the state machine this state is part of may exit.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	void EvaluateTransitions();
	
	/**
	 * Return all outgoing transition instances.
	 * @param Transitions The outgoing transitions.
	 * @param bExcludeAlwaysFalse Skip over transitions that can't ever be true.
	 * @return True if any transitions exist.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool GetOutgoingTransitions(TArray<class USMTransitionInstance*>& Transitions, bool bExcludeAlwaysFalse = true) const;

	/**
	 * Return all incoming transition instances.
	 * @param Transitions The incoming transitions.
	 * @param bExcludeAlwaysFalse Skip over transitions that can't ever be true.
	 * @return True if any transitions exist.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool GetIncomingTransitions(TArray<class USMTransitionInstance*>& Transitions, bool bExcludeAlwaysFalse = true) const;
	
	/** The transition this state will be taking. Generally only valid on EndState and if this state isn't an end state. May be null. */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	class USMTransitionInstance* GetTransitionToTake() const;

	/**
	 * Forcibly move to the next state providing this state is active and a transition is directly connecting the states.
	 * @param NextStateInstance The state node instance to switch to.
	 * @param bRequireTransitionToPass Will evaluate the transition and only switch states if it passes.
	 *
	 * @return True if the active state was switched.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	bool SwitchToLinkedState(USMStateInstance_Base* NextStateInstance, bool bRequireTransitionToPass = true);

	/**
	 * Return a transition given the transition index.
	 * @param Index The array index of the transition. If transitions have manual priorities they should correlate with the index value.
	 *
	 * @return The transition or nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMTransitionInstance* GetTransitionByIndex(int32 Index) const;
	
	/**
	 * Return the next connected state given a transition index.
	 * @param Index The array index of the transition. If transitions have manual priorities they should correlate with the index value.
	 *
	 * @return The connected state or nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "Logic Driver|Node Instance")
	USMStateInstance_Base* GetNextStateByTransitionIndex(int32 Index) const;
	
	/**
	 * Recursively search connected nodes for nodes matching the given type.
	 * @param OutNodes All found nodes.
	 * @param NodeClass The class type to search for.
	 * @param bIncludeChildren Include children of type NodeClass or require an exact match.
	 * @param StopIfTypeIsNot The search is broken when a node's type is not found in this list. Leave empty to never break the search.
	 */
	void GetAllNodesOfType(TArray<USMNodeInstance*>& OutNodes, TSubclassOf<USMNodeInstance> NodeClass, bool bIncludeChildren = true, const TArray<UClass*>& StopIfTypeIsNot = TArray<UClass*>()) const;

protected:
#if WITH_EDITORONLY_DATA
public:
	const FLinearColor& GetEndStateColor() const { return NodeEndStateColor; }
	
protected:
	/** The color this node should be when it is an end state. */
	UPROPERTY()
	FLinearColor NodeEndStateColor;

#endif

public:
	/**
	 * Always update the state at least once before ending.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "State", meta = (NodeBaseOnly))
	bool bAlwaysUpdate;

	/**
	 * Prevents conditional transitions for this state from being evaluated on Tick.
	 * This is good to use if the transitions leading out of the state are event based
	 * or if you are manually calling EvaluateTransitions from a state instance.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "State", meta = (NodeBaseOnly))
	bool bDisableTickTransitionEvaluation;

	/**
	 * Allows transitions to be evaluated in the same tick as Start State.
	 * Normally transitions are evaluated on the second tick.
	 * This can be chained with other nodes that have this checked making it
	   possible to evaluate multiple nodes and transitions in a single tick.
	 
	 * When using this consider performance implications and any potential
	   infinite loops such as if you are using a self transition on this state.

	 * Individual transitions can modify this behavior with bCanEvalWithStartState.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "State", meta = (NodeBaseOnly))
	bool bEvalTransitionsOnStart;

	/**
	 * Prevents the `Any State` node from adding transitions to this node.
	 * This can be useful for maintaining end states.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "State", meta = (NodeBaseOnly))
	bool bExcludeFromAnyState;

	/** Called right before the state has started. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|Node Instance")
	FOnStateBeginSignature OnStateBeginEvent;

	/** Called before the state has updated. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|Node Instance")
	FOnStateUpdateSignature OnStateUpdateEvent;

	/** Called before the state has ended. */
	UPROPERTY(BlueprintAssignable, Category = "Logic Driver|Node Instance")
	FOnStateEndSignature OnStateEndEvent;
};

/**
 * The base class for state nodes. This is where most execution logic should be defined.
 */
UCLASS(NotBlueprintable, BlueprintType, classGroup = "State Machine", hideCategories = (SMStateInstance), meta = (DisplayName = "State Class"))
class SMSYSTEM_API USMStateInstance : public USMStateInstance_Base
{
	GENERATED_BODY()

public:
	USMStateInstance();

};
