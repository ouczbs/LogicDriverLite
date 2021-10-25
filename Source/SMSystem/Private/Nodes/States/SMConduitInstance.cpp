// Copyright Recursoft LLC 2019-2020. All Rights Reserved.

#include "SMConduitInstance.h"

#include "SMConduit.h"


USMConduitInstance::USMConduitInstance() : Super(), bEvalWithTransitions(false), bCanEvaluate(true)
{
}

void USMConduitInstance::SetCanEvaluate(bool bValue)
{
	if (FSMConduit* Conduit = (FSMConduit*)GetOwningNode())
	{
		Conduit->bCanEvaluate = bValue;
	}
}

bool USMConduitInstance::GetCanEvaluate() const
{
	if (FSMConduit* Conduit = (FSMConduit*)GetOwningNode())
	{
		return Conduit->bCanEvaluate;
	}

	return false;
}
