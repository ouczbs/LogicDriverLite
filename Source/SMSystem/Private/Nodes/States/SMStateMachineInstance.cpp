// Copyright Recursoft LLC 2019-2020. All Rights Reserved.

#include "SMStateMachineInstance.h"
#include "SMStateMachine.h"

USMStateMachineInstance::USMStateMachineInstance() : Super(), bReuseCurrentState(false), bReuseIfNotEndState(false)
{
}

void USMStateMachineInstance::GetAllStateInstances(TArray<USMStateInstance_Base*>& StateInstances) const
{
	if (FSMStateMachine* StateMachineOwner = (FSMStateMachine*)GetOwningNode())
	{
		const TArray<FSMState_Base*>& States = StateMachineOwner->GetStates();
		for (FSMState_Base* State : States)
		{
			if (USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(State->GetNodeInstance()))
			{
				StateInstances.Add(StateInstance);
			}
		}
	}
}

void USMStateMachineInstance::GetEntryStates(TArray<USMStateInstance_Base*>& EntryStates) const
{
	if (FSMStateMachine* StateMachineOwner = (FSMStateMachine*)GetOwningNode())
	{
		const TSet<FSMState_Base*>& OriginalEntryStates = StateMachineOwner->GetEntryStates();
		EntryStates.Reset(OriginalEntryStates.Num());
		
		for (FSMState_Base* EntryState : OriginalEntryStates)
		{
			if (USMStateInstance_Base* NodeInstance = Cast<USMStateInstance_Base>(EntryState->GetNodeInstance()))
			{
				EntryStates.Add(NodeInstance);
			}
		}
	}
}

const FSMNode_Base* USMStateMachineInstance::GetOwningNodeContainer() const
{
	if (FSMStateMachine* StateMachineOwner = (FSMStateMachine*)GetOwningNode())
	{
		if (FSMStateMachine* ReferencedBy = StateMachineOwner->GetReferencedByStateMachine())
		{
			// Return the real node that references our owning node.
			return ReferencedBy;
		}
	}

	return Super::GetOwningNodeContainer();
}
