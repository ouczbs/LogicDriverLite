// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SMBlueprintFunctions.h"
#include "SMNode_Base.generated.h"

class USMInstance;
class USMNodeInstance;

UENUM()
enum class ESMTransactionType : uint8
{
	SM_Transition,
	SM_State
};

/** Data to send across the network. Default Implementation uses this to record TRANSITIONS which should be taken. */
USTRUCT()
struct SMSYSTEM_API FSMNetworkedTransaction
{
	GENERATED_USTRUCT_BODY()

public:
	FSMNetworkedTransaction() : FSMNetworkedTransaction(FGuid(), FGuid()) {}
	
	FSMNetworkedTransaction(const FGuid& SMGuid, const FGuid& TGuid, ESMTransactionType Type = ESMTransactionType::SM_Transition) :
	StateMachineGuid(SMGuid), BaseGuid(TGuid), TransactionGuid(FGuid::NewGuid()), Timestamp(0), TransactionType((int32)Type), bIsActive(false) {}

	/** The owning state machine. */
	UPROPERTY()
	FGuid StateMachineGuid;

	/** The node guid. */
	UPROPERTY()
	FGuid BaseGuid;

	/** Unique to this transaction. */
	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FGuid TransactionGuid;

	/** A UTC timestamp. Should be set manually. */
	UPROPERTY()
	FDateTime Timestamp;

	/** ESMTransactionType. */
	UPROPERTY()
	uint32 TransactionType:1;

	/** If the state is active. Valid only for a TransactionType of SM_State. */
	UPROPERTY()
	uint32 bIsActive:1;

	bool IsTransition() const { return (ESMTransactionType)TransactionType == ESMTransactionType::SM_Transition; }
	bool IsState() const { return (ESMTransactionType)TransactionType == ESMTransactionType::SM_State; }
};

/**
 * Base struct for all state machine nodes. The Guid MUST be manually initialized right after construction.
 */
USTRUCT(BlueprintInternalUseOnly)
struct SMSYSTEM_API FSMNode_Base
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY()
	FSMExposedFunctionHandler GraphEvaluator;

//#pragma region State Reads
	/** Special Values which have their own Nodes & Pins for reading state values
	 * from within a state or the previous state values from within a transition. */

	/** The current time spent in the state. */
	UPROPERTY(BlueprintReadWrite, Category = "State Machines")
	float TimeInState;

	/** State Machine is in end state or the state is an end state. */
	UPROPERTY(BlueprintReadWrite, Category = "State Machines")
	bool bIsInEndState;

	/** State has updated at least once. */
	UPROPERTY(BlueprintReadWrite, Category = "State Machines")
	bool bHasUpdated;

	/** Entry point to when a transition is first initialized. */
	UPROPERTY()
	TArray<FSMExposedFunctionHandler> TransitionInitializedGraphEvaluators;

	/** Entry point to when a transition is shutdown. */
	UPROPERTY()
	TArray<FSMExposedFunctionHandler> TransitionShutdownGraphEvaluators;
	
	/** Special indicator in case this node is a duplicate within the same blueprint. If this isn't 0 then the NodeGuid will have been adjusted. */
	UPROPERTY()
	int32 DuplicateId;

public:
	virtual void UpdateReadStates() {}
//#pragma endregion

public:
	FSMNode_Base();
	virtual ~FSMNode_Base() = default;
	FSMNode_Base(const FSMNode_Base& Node) = default;

	/** Initialize the graph evaluator with the instance. */
	virtual void Initialize(UObject* Instance);
	bool IsInitialized() const { return bInitialized; }

	/** Resets persistent data. */
	virtual void Reset();

	/** Unique identifier used in constructing nodes from a graph. May not be unique if this is from a parent graph or a reference. */
	const FGuid& GetNodeGuid() const;
	void GenerateNewNodeGuid();
	
	/** Unique identifier taking into account qualified path. Unique across blueprints if called after Instance initialization. */
	const FGuid& GetGuid() const;
	/** Calculate the value returned from GetGuid(). Gets all owner nodes and builds a path to this node. Hashes the path and sets PathGuid. */
	virtual void CalculatePathGuid(TMap<FString, int32>& MappedPaths);
	/** Unhashed string format of the guid path. MappedPaths are used to adjust for collisions. */
	FString GetGuidPath(TMap<FString, int32>& MappedPaths) const;
	
	/** Only generate a new guid if the current guid is invalid. This needs to be called
	 * on new nodes. */
	void GenerateNewNodeGuidIfNotSet();
	void SetNodeGuid(const FGuid& NewGuid);

	/** The state machine's NodeGuid owning this node. */
	void SetOwnerNodeGuid(const FGuid& NewGuid);
	/** Unique identifier to help determine which state machine this node belongs to. */
	const FGuid& GetOwnerNodeGuid() const { return OwnerGuid; }

	/** The node directly owning this node. Should be a StateMachine. */
	void SetOwnerNode(FSMNode_Base* Owner);
	/** The node directly owning this node. Should be a StateMachine. */
	virtual FSMNode_Base* GetOwnerNode() const { return OwnerNode; }
	
	/** The state machine instance owning this node. */
	USMInstance* GetOwningInstance() const { return OwningInstance; }

	/** Create the node instance if a node instance class is set. */
	void CreateNodeInstance();

	/** Calls CheckNodeInstanceCompatible. */
	void SetNodeInstanceClass(UClass* NewNodeInstanceClass);
	
	/** Derived nodes should overload and check for the correct type. */
	virtual bool IsNodeInstanceClassCompatible(UClass* NewNodeInstanceClass) const;
	
	/** Return the current node instance. Only valid after initialization and may be nullptr. */
	virtual USMNodeInstance* GetNodeInstance() const { return NodeInstance; }

	/** The default node instance class. Each derived node class needs to implement. */
	virtual UClass* GetDefaultNodeInstanceClass() const { return nullptr; }

	void SetNodeName(const FString& Name);
	const FString& GetNodeName() const { return NodeName; }
	
	void SetTemplateName(const FName& Name);
	const FName& GetTemplateName() const { return TemplateName; }

	/** If this node is active. */
	virtual bool IsActive() const { return bIsActive; }

	virtual void ExecuteInitializeNodes();
	virtual void ExecuteShutdownNodes();

#if WITH_EDITORONLY_DATA
	virtual bool IsDebugActive() const { return bIsActive; }
	virtual bool WasDebugActive() const { return bWasActive; }
	
	/** Debug helper in case a state switches to inactive in one frame. */
	bool bWasActive = false;
#endif
protected:
	/** Execute the graph. */
	virtual void Execute();
	virtual void SetActive(bool bValue);
protected:
	/*
	 * NodeGuid used in constructing nodes from a graph. Set initially from the editor graph.
	 * This is not guaranteed to be unique across Blueprints.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "State Machines")
	FGuid Guid;

	/** The state machine's NodeGuid owning this node. */
	UPROPERTY()
	FGuid OwnerGuid;

	/*
	 * Unique identifier calculated from this node's place in an instance.
	 * Calculated by taking the MD5 hash of the full path of all owner NodeGuids and
	 * this NodeGuid.
	 * This is what is returned from GetGuid().
	 * 
	 * ReadWrite so it can be easily read from custom graph nodes.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "State Machines")
	FGuid PathGuid;
	
	/** The node directly owning this node. Should be a StateMachine. */
	FSMNode_Base* OwnerNode;

	UPROPERTY()
	FString NodeName;

	/** The name of a template archetype to use when constructing an instance. This allows default values be passed into the instance. */
	UPROPERTY()
	FName TemplateName;

	/** The state machine instance owning this node. */
	UPROPERTY()
	USMInstance* OwningInstance;

	/** The node instance for this node if it exists. */
	UPROPERTY(BlueprintReadWrite, Transient, Category = "Node Class")
	USMNodeInstance* NodeInstance;

	/** The class to use to construct the node instance if one exists. */
	UPROPERTY(BlueprintReadWrite, Category = "Node Class")
	UClass* NodeInstanceClass;

	bool bInitialized;

	bool bIsActive;
};