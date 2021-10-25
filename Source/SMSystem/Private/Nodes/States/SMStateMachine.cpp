// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#include "SMStateMachine.h"
#include "SMInstance.h"
#include "SMLogging.h"
#include "SMUtils.h"
#include "SMStateMachineInstance.h"

FSMStateMachine::FSMStateMachine() : Super(), bHasAdditionalLogic(false), bReuseCurrentState(false),
                                     bOnlyReuseIfNotEndState(false), bAllowIndependentTick(false),
                                     bCallReferenceTickOnManualUpdate(true), bReuseReference(false),
                                     bWaitForEndState(false),
                                     AllActiveTransactions(nullptr),
                                     ReferencedStateMachineClass(nullptr),
                                     ReferencedStateMachine(nullptr),
                                     IsReferencedByInstance(nullptr), IsReferencedByStateMachine(nullptr),
                                     TimeSpentWaitingForUpdate(0.f), MaxTimeToSpendWaitingForUpdate(2.f),
                                     bWaitingForTransitionUpdate(false), bCanEvaluateTransitions(true),
                                     bCanTakeTransitions(true)
{
}

void FSMStateMachine::SetNetworkedConditions(TArray<FSMNetworkedTransaction>& TransitionArray, bool bEvaluateTransitions, bool bTakeTransitions, float MaxTimeToWait, bool bCanExecuteStateLogic)
{
	AllActiveTransactions = &TransitionArray;
	bCanEvaluateTransitions = bEvaluateTransitions;
	bCanTakeTransitions = bTakeTransitions;
	MaxTimeToSpendWaitingForUpdate = MaxTimeToWait;

	for (FSMState_Base* State : States)
	{
		State->SetCanExecuteLogic(bCanExecuteStateLogic);
	}
}

void FSMStateMachine::ProcessStates(float DeltaSeconds, bool bForceTransitionEvaluationOnly)
{
	if (ReferencedStateMachine)
	{
		ReferencedStateMachine->GetRootStateMachine().ProcessStates(DeltaSeconds, bForceTransitionEvaluationOnly);
		return;
	}

	bool bStateChanged = false;
	TArray<FSMState_Base*> ActiveStatesCopy = GetActiveStates();
	for (FSMState_Base* CurrentState : ActiveStatesCopy)
	{
		bool bStateJustStarted = false;
		
		// Always start the state before attempting a transition.
		if (!CurrentState->IsActive() || CurrentState->HasBeenReenteredFromParallelState())
		{
			// prevents repeated reentry if this state was ending and triggered a transition which led to processing.
			if (CurrentState->IsStateEnding())
			{
				continue;
			}

			if (!CurrentState->IsActive() || !CurrentState->HasBeenReenteredFromParallelState() || CurrentState->bAllowParallelReentry)
			{
				CurrentState->StartState();
				bStateJustStarted = true;
			}
			
			// Prevents repeated reentry with parallel states.
			CurrentState->NotifyOfParallelReentry(false);
			
			// It's possible the current state is null depending on start state's logic (such as if it is shutting down this state machine).
			if (!ActiveStates.Contains(CurrentState) || !CurrentState->bEvalTransitionsOnStart)
			{
				// Don't perform transition evaluation in same tick unless specified.
				continue;
			}
		}

		/*
		 * This can be true when there are multiple active states, and the first one transitions and reentries into the next one.
		 * Without this check that would cause a stack overflow.
		 */
		if (ProcessingStates.Contains(CurrentState))
		{
			continue;
		}

		// Evaluate possible transitions and return the best one. If the state machine is waiting, not allowed to evaluate transitions,
		// or this is a normal update and the state isn't allowed to evaluate, then skip evaluation.
		bool bCanCheckTransitions = !(bWaitingForTransitionUpdate || !bCanEvaluateTransitions ||
			(!bForceTransitionEvaluationOnly && !CurrentState->CanEvaluateTransitionsOnTick()));

		if (bCanCheckTransitions && CurrentState->IsStateMachine())
		{
			if (((FSMStateMachine*)CurrentState)->bWaitForEndState)
			{
				bCanCheckTransitions = (FSMStateMachine*)CurrentState->IsInEndState();
			}
		}
		
		TArray<TArray<FSMTransition*>> ParallelTransitionChains;
		if (bCanCheckTransitions && CurrentState->GetValidTransition(ParallelTransitionChains))
		{
			bool bSuccess = false;
			for (TArray<FSMTransition*>& TransitionChain : ParallelTransitionChains)
			{
				// This specific transition doesn't allow same tick eval with start state.
				if (bStateJustStarted && !FSMTransition::CanEvaluateWithStartState(TransitionChain))
				{
					continue;
				}

				if (FSMState_Base* NextState = FSMTransition::GetFinalStateFromChain(TransitionChain))
				{
					// If the next state is already active the transition may not allow evaluation. Doesn't apply to self transitions.
					if (NextState != CurrentState && NextState->IsActive() && !FSMTransition::CanChainEvalIfNextStateActive(TransitionChain))
					{
						continue;
					}
				}
				
				for (FSMTransition* Transition : TransitionChain)
				{
					if (ProcessTransition(Transition, nullptr, DeltaSeconds))
					{
						ProcessingStates.Add(CurrentState);
						// If this succeeds once we are good. It's possible a multi chain may have a failure when in a networked environment
						// because one was already taken.
						bSuccess = true;
					}
				}
			}

			if (bSuccess)
			{
				bStateChanged = true;
				//  May remain active in which case we should update.
				if (!CurrentState->IsActive())
				{
					continue;
				}
			}
		}
		
		if (!bStateJustStarted)
		{
			if (bForceTransitionEvaluationOnly)
			{
				// This is an optimized transition evaluation branch. Forward request directly to nested FSM if present.
				if (CurrentState->IsStateMachine())
				{
					((FSMStateMachine*)CurrentState)->ProcessStates(DeltaSeconds, bForceTransitionEvaluationOnly);
				}
			}
			else
			{
				// No transition found, perform general update.
				ProcessingStates.Add(CurrentState);
				CurrentState->UpdateState(DeltaSeconds);
			}
		}
	}

	if (bStateChanged)
	{
		ProcessStates(DeltaSeconds, bForceTransitionEvaluationOnly);
	}
	
	ProcessingStates.Reset();
}

bool FSMStateMachine::ProcessTransition(FSMTransition* Transition, const FSMNetworkedTransaction* Transaction, float DeltaSeconds, FDateTime* CurrentTime)
{
	if (ReferencedStateMachine)
	{
		return ReferencedStateMachine->GetRootStateMachine().ProcessTransition(Transition, Transaction, DeltaSeconds, CurrentTime);
	}

	const bool bServerUpdate = Transaction != nullptr;
	const bool bCanTransitionNow = bCanTakeTransitions || bServerUpdate;

	bWaitingForTransitionUpdate = false;

	// If the client is continuing execution while the server is processing this can prevent a double fire.
	if (bServerUpdate)
	{
		if (!ensureAlwaysMsgf(Transaction->IsTransition(), TEXT("Attempted to process a state network transaction when it was expecting a transition network transaction.")) || PreviousTransactions.Contains(Transaction->TransactionGuid))
		{
			return false;
		}
	}

	// If this was called via server the state is likely still active.
	if (bCanTransitionNow)
	{
		FSMState_Base* LastState = Transition->GetFromState();
		FSMState_Base* ToState = Transition->GetToState();
		
		if (LastState->IsActive() && !LastState->bStayActiveOnStateChange)
		{
			LastState->EndState(DeltaSeconds, Transition);
		}

		Transition->TakeTransition();

		if(USMInstance* Instance = GetOwningInstance())
		{
			Instance->NotifyTransitionTaken(*Transition);
		}
		if (IsReferencedByInstance)
		{
			IsReferencedByInstance->NotifyTransitionTaken(*Transition);
		}

		SetCurrentState(ToState, LastState);

		if (!ActiveStates.Contains(ToState))
		{
			const FString InstanceName = GetOwningInstance() ? GetOwningInstance()->GetName() : "Unknown";
			LD_LOG_ERROR(TEXT("Current state not set for state machine node '%s'. The package '%s' may be getting cleaned up. Check your code for proper UE4 memory management."), *GetNodeName(), *InstanceName);
			return false;
		}
		
		ensure(LastState->bStayActiveOnStateChange || !LastState->IsActive());
	}

	// This is a new transition not being supplied by the server.
	if (!bServerUpdate && IsNetworked())
	{
		FSMNetworkedTransaction NewTransition(GetGuid(), Transition->GetGuid());
		NewTransition.Timestamp = CurrentTime ? *CurrentTime : FDateTime::UtcNow();
		
		// Don't follow this transition a second time.
		if (bCanTransitionNow)
		{
			PreviousTransactions.Add(NewTransition.TransactionGuid, NewTransition);
		}
		else
		{
			bWaitingForTransitionUpdate = true;
		}

		// Notifies server we are taking a new transition.
		AllActiveTransactions->Add(NewTransition);
	}
	else if (bServerUpdate && Transaction)
	{
		// Don't record a server transition more than once either.
		PreviousTransactions.Add(Transaction->TransactionGuid, *Transaction);
	}

	return bCanTransitionNow;
}

void FSMStateMachine::CleanupPreviousTransactions(const FDateTime& CurrentTime, float PreviousTransactionTimeout)
{
	// Check for and remove expired transactions.
	if (PreviousTransactions.Num())
	{
		TArray<FGuid> TransactionsToRemove;
		const FTimespan Seconds = FTimespan::FromSeconds((double)PreviousTransactionTimeout);
		for (const auto& PreviousTransaction : PreviousTransactions)
		{
			FDateTime ExpirationDate = PreviousTransaction.Value.Timestamp + Seconds;
			if (ExpirationDate <= CurrentTime)
			{
				TransactionsToRemove.Add(PreviousTransaction.Key);
			}
		}

		for (const FGuid& Id : TransactionsToRemove)
		{
			PreviousTransactions.Remove(Id);
		}
	}
}

void FSMStateMachine::SetReuseCurrentState(bool bValue, bool bOnlyWhenNotInEndState)
{
	if(ReferencedStateMachine)
	{
		ReferencedStateMachine->GetRootStateMachine().SetReuseCurrentState(bValue, bOnlyWhenNotInEndState);
		return;
	}
	
	bReuseCurrentState = bValue;
	bOnlyReuseIfNotEndState = bOnlyWhenNotInEndState;
}

bool FSMStateMachine::CanReuseCurrentState() const
{
	return bReuseCurrentState && (!IsInEndState() || !bOnlyReuseIfNotEndState);
}

void FSMStateMachine::SetClassReference(UClass* ClassReference)
{
	ReferencedStateMachineClass = ClassReference;
}

void FSMStateMachine::SetInstanceReference(USMInstance* InstanceReference)
{
	ReferencedStateMachine = InstanceReference;
	if (ReferencedStateMachine)
	{
		// The reference should inherit the reuse state property.
		ReferencedStateMachine->GetRootStateMachine().SetReuseCurrentState(bReuseCurrentState, bOnlyReuseIfNotEndState);
		
		// Only want the top level instance managing ticks.
		ReferencedStateMachine->SetRegisterTick(bAllowIndependentTick);
		ReferencedStateMachine->SetCanEverTick(bAllowIndependentTick);

		ReferencedStateMachine->SetTickOnManualUpdate(bCallReferenceTickOnManualUpdate);

		ReferencedStateMachine->GetRootStateMachine().SetOwnerNode(this);
	}
}

void FSMStateMachine::SetReferencedTemplateName(const FName& Name)
{
	ReferencedTemplateName = Name;
}

void FSMStateMachine::SetReferencedBy(USMInstance* FromInstance, FSMStateMachine* FromStateMachine)
{
	IsReferencedByInstance = FromInstance;
	IsReferencedByStateMachine = FromStateMachine;
}

void FSMStateMachine::AddActiveState(FSMState_Base* State, bool bReplicate)
{
	SetCurrentState(State, nullptr);

	if (bReplicate && IsNetworked() && State)
	{
		FSMNetworkedTransaction Transaction(GetGuid(), State->GetGuid(), ESMTransactionType::SM_State);
		Transaction.bIsActive = true;
		AllActiveTransactions->Add(Transaction);
	}
}

void FSMStateMachine::RemoveActiveState(FSMState_Base* State, bool bReplicate)
{
	State->EndState(0.f);
	ActiveStates.Remove(State);

	if (USMInstance* Instance = GetOwningInstance())
	{
		Instance->NotifyStateChange(nullptr, State);
	}

	if (IsReferencedByInstance)
	{
		IsReferencedByInstance->NotifyStateChange(nullptr, State);
	}
	
	if (bReplicate && IsNetworked() && State)
	{
		FSMNetworkedTransaction Transaction(GetGuid(), State->GetGuid(), ESMTransactionType::SM_State);
		Transaction.bIsActive = false;
		AllActiveTransactions->Add(Transaction);
	}
}

void FSMStateMachine::SetCurrentState(FSMState_Base* ToState, FSMState_Base* FromState)
{
	const bool bStartedInEndState = IsInEndState();
	
	if (FromState && !FromState->bStayActiveOnStateChange)
	{
		ActiveStates.Remove(FromState);
	}

	if (ToState)
	{
		ToState->SetPreviousEnteredState(FromState);
		if (ActiveStates.Contains(ToState))
		{
			// Reentered.
			ToState->NotifyOfParallelReentry();
		}
		else
		{
			ActiveStates.Add(ToState);
		}
	}

	if(USMInstance* Instance = GetOwningInstance())
	{
		Instance->NotifyStateChange(ToState, FromState);
	}

	if(IsReferencedByInstance)
	{
		IsReferencedByInstance->NotifyStateChange(ToState, FromState);
	}
}

void FSMStateMachine::Initialize(UObject* Instance)
{
	Super::Initialize(Instance);

	UpdateStateGraphEvaluator.Initialize(Instance);
	EndStateGraphEvaluator.Initialize(Instance);
	
	if (ReferencedStateMachine)
	{
		// Let the instance's state machine we are referencing know they are being referenced.
		ReferencedStateMachine->GetRootStateMachine().SetReferencedBy(Cast<USMInstance>(Instance), this);
	}
	
	for (FSMNode_Base* Node : GetAllNodes())
	{
		Node->Initialize(Instance);
	}
}

void FSMStateMachine::Reset()
{
	Super::Reset();
	ClearTemporaryInitialStates();
	UpdateStateGraphEvaluator.Reset();
	EndStateGraphEvaluator.Reset();
}

bool FSMStateMachine::StartState()
{
	if (!Super::StartState())
	{
		return false;
	}

	if(bHasAdditionalLogic)
	{
		if (CanExecuteLogic())
		{
			Execute();
		}
		
		// The additional logic will call start on the instance.
		if (ReferencedStateMachine)
		{
			return true;
		}
	}
	
	if (ReferencedStateMachine)
	{
		ReferencedStateMachine->Start();
		return true;
	}

	if (!bReuseCurrentState || ActiveStates.Num() == 0)
	{
		for (FSMState_Base* InitialState : GetInitialStates())
		{
			SetCurrentState(InitialState, nullptr);
		}
		if(HasTemporaryEntryStates())
		{
			ClearTemporaryInitialStates();
		}
	}
	
	ProcessStates(0.f);

	return true;
}

bool FSMStateMachine::UpdateState(float DeltaSeconds)
{
	if (!Super::UpdateState(DeltaSeconds))
	{
		return false;
	}

	if (bHasAdditionalLogic)
	{
		if (CanExecuteLogic())
		{
			UpdateStateGraphEvaluator.Execute((void*)&DeltaSeconds);
		}
		
		// The additional logic will call update on the instance.
		if (ReferencedStateMachine)
		{
			return true;
		}
	}
	
	if (ReferencedStateMachine)
	{
		ReferencedStateMachine->Update(DeltaSeconds);
		return true;
	}

	if (bWaitingForTransitionUpdate && MaxTimeToSpendWaitingForUpdate > 0.f)
	{
		TimeSpentWaitingForUpdate += DeltaSeconds;

		if (TimeSpentWaitingForUpdate >= MaxTimeToSpendWaitingForUpdate)
		{
			bWaitingForTransitionUpdate = false;
			TimeSpentWaitingForUpdate = 0.f;
		}
	}

	ProcessStates(DeltaSeconds);

	return true;
}

bool FSMStateMachine::EndState(float DeltaSeconds, const FSMTransition* TransitionToTake)
{
	if (!Super::EndState(DeltaSeconds, TransitionToTake))
	{
		return false;
	}

	if (bHasAdditionalLogic)
	{
		if (CanExecuteLogic())
		{
			EndStateGraphEvaluator.Execute();
		}
		
		// The additional logic will call stop on the instance.
		if (ReferencedStateMachine)
		{
			return true;
		}
	}
	
	if (ReferencedStateMachine)
	{
		// Manually set transition since Stop won't provide one.
		ReferencedStateMachine->GetRootStateMachine().SetTransitionToTake(TransitionToTake);
		ReferencedStateMachine->Stop();
		return true;
	}

	TArray<FSMState_Base*> ActiveStatesCopy = GetActiveStates();
	for(FSMState_Base* CurrentState: ActiveStatesCopy)
	{
		CurrentState->EndState(DeltaSeconds);

		if (!CanReuseCurrentState())
		{
			SetCurrentState(nullptr, CurrentState);
		}
	}

	return true;
}

void FSMStateMachine::ExecuteInitializeNodes()
{
	Super::ExecuteInitializeNodes();
}

void FSMStateMachine::ExecuteShutdownNodes()
{
	Super::ExecuteShutdownNodes();
}

void FSMStateMachine::OnStartedByInstance(USMInstance* Instance)
{
	if (bHasAdditionalLogic)
	{
		Super::OnStartedByInstance(Instance);
	}
}

void FSMStateMachine::OnStoppedByInstance(USMInstance* Instance)
{
	if (bHasAdditionalLogic)
	{
		Super::OnStoppedByInstance(Instance);
	}
}

// This is only necessary for legacy operations with reusing references that reference themselves. Also assists with unit test for reusing references.
static TSet<FSMStateMachine*> CalculatingInstances;

void FSMStateMachine::CalculatePathGuid(TMap<FString, int32>& MappedPaths)
{
	if(CalculatingInstances.Contains(this))
	{
		return;
	}
	CalculatingInstances.Add(this);
	
	Super::CalculatePathGuid(MappedPaths);

	if(ReferencedStateMachine)
	{
		ReferencedStateMachine->GetRootStateMachine().CalculatePathGuid(MappedPaths);
	}
	
	for(FSMNode_Base* Node : GetAllNodes())
	{
		Node->CalculatePathGuid(MappedPaths);
	}

	CalculatingInstances.Remove(this);
}


void FSMStateMachine::AddInitialState(FSMState_Base* State)
{
	if (ReferencedStateMachine)
	{
		ReferencedStateMachine->GetRootStateMachine().AddInitialState(State);
		return;
	}
	
	if (State && !States.Contains(State))
	{
		ensureAlwaysMsgf(false, TEXT("Could not set initial state %s. It is not located in state machine %s"), *State->GetNodeName(), *GetNodeName());
		return;
	}

	EntryStates.Add(State);
}

void FSMStateMachine::AddTemporaryInitialState(FSMState_Base* State)
{
	if(ReferencedStateMachine)
	{
		ReferencedStateMachine->GetRootStateMachine().AddTemporaryInitialState(State);
		return;
	}

	if (!State)
	{
		return;
	}
	
	if (!ensureAlwaysMsgf(States.Contains(State), TEXT("Could not set temporary initial state %s. It is not located in state machine %s"), *State->GetNodeName(), *GetNodeName()))
	{
		return;
	}

	TemporaryEntryStates.Add(State);
}

void FSMStateMachine::ClearTemporaryInitialStates()
{
	TemporaryEntryStates.Empty();
}

const TSet<FSMState_Base*>& FSMStateMachine::GetEntryStates() const
{
	if (ReferencedStateMachine)
	{
		return ReferencedStateMachine->GetRootStateMachine().GetEntryStates();
	}

	return EntryStates;
}

TArray<FSMState_Base*> FSMStateMachine::GetInitialStates() const
{
	if(ReferencedStateMachine)
	{
		return ReferencedStateMachine->GetRootStateMachine().GetInitialStates();
	}
	
	return HasTemporaryEntryStates() ? TemporaryEntryStates.Array() : EntryStates.Array();
}

FSMState_Base* FSMStateMachine::GetSingleInitialState() const
{
	TArray<FSMState_Base*> InitialStates = GetInitialStates();
	if (InitialStates.Num() > 0)
	{
		return InitialStates[0];
	}

	return nullptr;
}

FSMState_Base* FSMStateMachine::GetSingleInitialTemporaryState() const
{
	return TemporaryEntryStates.Num() > 0 ? TemporaryEntryStates.Array()[0] : nullptr;
}

FSMState_Base* FSMStateMachine::FindState(const FGuid& StateGuid) const
{
	if (GetGuid() == StateGuid)
	{
		return const_cast<FSMStateMachine*>(this);
	}

	if (ReferencedStateMachine != nullptr)
	{
		return ReferencedStateMachine->FindStateByGuid(StateGuid);
	}

	for (FSMState_Base* State : States)
	{
		if (State->GetGuid() == StateGuid)
		{
			return State;
		}

		if (State->IsStateMachine())
		{
			if (FSMState_Base* FoundState = ((FSMStateMachine*)State)->FindState(StateGuid))
			{
				return FoundState;
			}
		}
	}

	return nullptr;
}

FSMState_Base* FSMStateMachine::GetSingleActiveState() const
{
	if(FSMState_Base* State = ReferencedStateMachine ? ReferencedStateMachine->GetRootStateMachine().GetSingleActiveState() : nullptr)
	{
		return State;
	}

	for (FSMState_Base* CurrentState : ActiveStates)
	{
		return CurrentState;
	}

	// Temporary state needs to be counted as current if it is set.
	return ReferencedStateMachine ? ReferencedStateMachine->GetRootStateMachine().GetSingleInitialTemporaryState() : GetSingleInitialTemporaryState();
}

TArray<FSMState_Base*> FSMStateMachine::GetActiveStates() const
{
	if (ReferencedStateMachine)
	{
		if (ReferencedStateMachine->GetRootStateMachine().HasActiveStates())
		{
			return ReferencedStateMachine->GetRootStateMachine().GetActiveStates();
		}
	}
	
	if (HasActiveStates())
	{
		return ActiveStates.Array();
	}
	
	return ReferencedStateMachine ? ReferencedStateMachine->GetRootStateMachine().TemporaryEntryStates.Array() : TemporaryEntryStates.Array();
}

TArray<FSMState_Base*> FSMStateMachine::GetAllNestedActiveStates() const
{
	if (ReferencedStateMachine)
	{
		return ReferencedStateMachine->GetAllActiveStates();
	}

	TArray<FSMState_Base*> OutStates = GetActiveStates();

	for (FSMState_Base* State : States)
	{
		if (State->IsStateMachine())
		{
			OutStates.Append(((FSMStateMachine*)State)->GetAllNestedActiveStates());
		}
	}

	return OutStates;
}

bool FSMStateMachine::IsInEndState() const
{
	if (ReferencedStateMachine)
	{
		return ReferencedStateMachine->IsInEndState();
	}

	for (FSMState_Base* CurrentState : ActiveStates)
	{
		if (CurrentState->IsEndState())
		{
			if (CurrentState->IsStateMachine())
			{
				// FSM may be waiting to be considered an end state.
				FSMStateMachine* NestedFSM = (FSMStateMachine*)CurrentState;
				if (NestedFSM->bWaitForEndState && !NestedFSM->IsInEndState())
				{
					continue;
				}
			}
			
			return true;
		}
	}

	return ActiveStates.Num() == 0;
}

bool FSMStateMachine::IsNodeInstanceClassCompatible(UClass* NewNodeInstanceClass) const
{
	return NewNodeInstanceClass && NewNodeInstanceClass->IsChildOf<USMStateMachineInstance>();
}

USMNodeInstance* FSMStateMachine::GetNodeInstance() const
{
	if (ReferencedStateMachine)
	{
		return ReferencedStateMachine->GetRootStateMachine().GetNodeInstance();
	}

	return Super::GetNodeInstance();
}

UClass* FSMStateMachine::GetDefaultNodeInstanceClass() const
{
	return USMStateMachineInstance::StaticClass();
}

FSMNode_Base* FSMStateMachine::GetOwnerNode() const
{
	if (IsReferencedByStateMachine)
	{
		return IsReferencedByStateMachine;
	}

	return Super::GetOwnerNode();
}

void FSMStateMachine::AddState(FSMState_Base* State)
{
	State->SetOwnerNode(this);
	States.AddUnique(State);
}

void FSMStateMachine::AddTransition(FSMTransition* Transition)
{
	Transition->SetOwnerNode(this);
	Transitions.AddUnique(Transition);
}

TArray<FSMNode_Base*> FSMStateMachine::GetAllNodes(bool bIncludeNested) const
{
	TArray<FSMNode_Base*> Results;
	
	Results.Append(States);
	Results.Append(Transitions);

	if(bIncludeNested)
	{
		for(FSMState_Base* State : States)
		{
			if(State->IsStateMachine())
			{
				FSMStateMachine* StateMachine = (FSMStateMachine*)State;
				
				if (StateMachine->ReferencedStateMachine)
				{
					Results.Add(&StateMachine->ReferencedStateMachine->GetRootStateMachine());
					Results.Append(StateMachine->ReferencedStateMachine->GetRootStateMachine().GetAllNodes(bIncludeNested));
				}
				else
				{
					Results.Append(((FSMStateMachine*)State)->GetAllNodes(bIncludeNested));
				}
			}
		}
	}

	return Results;
}
