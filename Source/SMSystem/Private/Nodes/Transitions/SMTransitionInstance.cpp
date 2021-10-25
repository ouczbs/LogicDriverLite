// Copyright Recursoft LLC 2019-2020. All Rights Reserved.

#include "SMTransitionInstance.h"
#include "SMStateInstance.h"
#include "SMTransition.h"

USMTransitionInstance::USMTransitionInstance() : Super(), PriorityOrder(0),
bCanEvaluate(true), bCanEvaluateFromEvent(true),
bCanEvalWithStartState(true)
{
}

void USMTransitionInstance::SetCanEvaluate(bool bValue)
{
	if (FSMTransition* Transition = (FSMTransition*)GetOwningNode())
	{
		Transition->bCanEvaluate = bValue;
	}
}

bool USMTransitionInstance::GetCanEvaluate() const
{
	if (FSMTransition* Transition = (FSMTransition*)GetOwningNode())
	{
		return Transition->bCanEvaluate;
	}

	return false;
}

USMStateInstance_Base* USMTransitionInstance::GetPreviousStateInstance() const
{
	if (FSMTransition* Transition = (FSMTransition*)GetOwningNode())
	{
		if (FSMState_Base* PrevState = Transition->GetFromState())
		{
			return Cast<USMStateInstance_Base>(PrevState->GetNodeInstance());
		}
	}

	return nullptr;
}

USMStateInstance_Base* USMTransitionInstance::GetNextStateInstance() const
{
	if (FSMTransition* Transition = (FSMTransition*)GetOwningNode())
	{
		if (FSMState_Base* NextState = Transition->GetToState())
		{
			return Cast<USMStateInstance_Base>(NextState->GetNodeInstance());
		}
	}

	return nullptr;
}

void USMTransitionInstance::GetTransitionInfo(FSMTransitionInfo& Transition) const
{
	if (FSMTransition* TransitionNode = (FSMTransition*)GetOwningNode())
	{
		Transition = FSMTransitionInfo(*TransitionNode);
	}
	else
	{
		Transition = FSMTransitionInfo();
	}
}
