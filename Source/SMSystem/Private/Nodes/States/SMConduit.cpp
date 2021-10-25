// Copyright Recursoft LLC 2019-2020. All Rights Reserved.

#include "SMConduit.h"
#include "SMConduitInstance.h"
#include "SMUtils.h"

FSMConduit::FSMConduit() : Super(), bCanEnterTransition(false), bCanEvaluate(true), bEvalWithTransitions(false),
                           bIsEvaluating(false), bCheckedForTransitions(false)
{
}

void FSMConduit::Initialize(UObject* Instance)
{
	Super::Initialize(Instance);
	ConduitEnteredGraphEvaluator.Initialize(Instance);
}

void FSMConduit::Reset()
{
	Super::Reset();
	ConduitEnteredGraphEvaluator.Reset();
}

bool FSMConduit::IsNodeInstanceClassCompatible(UClass* NewNodeInstanceClass) const
{
	return NewNodeInstanceClass && NewNodeInstanceClass->IsChildOf<USMConduitInstance>();
}

UClass* FSMConduit::GetDefaultNodeInstanceClass() const
{
	return USMConduitInstance::StaticClass();
}

bool FSMConduit::StartState()
{
	const bool bResult = Super::StartState();

	ConduitEnteredGraphEvaluator.Execute();

	return bResult;
}

bool FSMConduit::UpdateState(float DeltaSeconds)
{
	const bool bResult = Super::UpdateState(DeltaSeconds);

	return bResult;
}

bool FSMConduit::EndState(float DeltaSeconds, const FSMTransition* TransitionToTake)
{
	const bool bResult = Super::EndState(DeltaSeconds, TransitionToTake);

	return bResult;
}

bool FSMConduit::GetValidTransition(TArray<TArray<FSMTransition*>>& Transitions)
{
	if (bCheckedForTransitions || !bCanEvaluate)
	{
		return false;
	}

	bIsEvaluating = true;
#if WITH_EDITORONLY_DATA
	bWasEvaluating = true; // Will be set to false from the editor.
#endif
	
	// First check that the conduit passes.
	Execute();

	bIsEvaluating = false;
	
	if(!bCanEnterTransition)
	{
		return false;
	}

	bCheckedForTransitions = true;
	
	// Passes, find the best transition.
	const bool bResult = Super::GetValidTransition(Transitions);
	
	bCheckedForTransitions = false;
	return bResult;
}

void FSMConduit::EnterConduitWithTransition()
{
	if (IsConfiguredAsTransition())
	{
		SetActive(true);
		ConduitEnteredGraphEvaluator.Execute();
		SetActive(false);
	}
}
