// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#include "SMNodeInstance.h"
#include "CoreMinimal.h"
#include "SMInstance.h"


DEFINE_STAT(STAT_NodeInstances);

USMNodeInstance::USMNodeInstance() : Super(), OwningNode(nullptr)
{
	INC_DWORD_STAT(STAT_NodeInstances)
}

UWorld* USMNodeInstance::GetWorld() const
{
	if(UObject* Context = GetContext())
	{
		return Context->GetWorld();
	}

	return nullptr;
}

void USMNodeInstance::BeginDestroy()
{
	Super::BeginDestroy();
	DEC_DWORD_STAT(STAT_NodeInstances)
}

UObject* USMNodeInstance::GetContext() const
{
	if(USMInstance* SMInstance = GetStateMachineInstance())
	{
		return SMInstance->GetContext();
	}

	return nullptr;
}

USMInstance* USMNodeInstance::GetStateMachineInstance(bool bTopMostInstance) const
{
	if(USMInstance* Instance = Cast<USMInstance>(GetOuter()))
	{
		if(bTopMostInstance)
		{
			return Instance->GetMasterReferenceOwner();
		}

		return Instance;
	}

	return nullptr;
}

void USMNodeInstance::SetOwningNode(FSMNode_Base* Node)
{
	OwningNode = Node;
}

USMStateMachineInstance* USMNodeInstance::GetOwningStateMachineNodeInstance() const
{
	if (const FSMNode_Base* Node = GetOwningNode())
	{
		if (FSMNode_Base* NodeOwner = Node->GetOwnerNode())
		{
			return Cast<USMStateMachineInstance>(NodeOwner->GetNodeInstance());
		}
	}

	return nullptr;
}

float USMNodeInstance::GetTimeInState() const
{
	return OwningNode ? OwningNode->TimeInState : 0.f;
}

bool USMNodeInstance::IsInEndState() const
{
	return OwningNode ? OwningNode->bIsInEndState : false;
}

bool USMNodeInstance::HasUpdated() const
{
	return OwningNode ? OwningNode->bHasUpdated : false;
}

bool USMNodeInstance::IsActive() const
{
	return OwningNode ? OwningNode->IsActive() : false;
}

const FString& USMNodeInstance::GetNodeName() const
{
	static FString EmptyString;
	return OwningNode ? OwningNode->GetNodeName() : EmptyString;
}

const FGuid& USMNodeInstance::GetGuid() const
{
	static FGuid BlankGuid;
	return OwningNode ? OwningNode->GetGuid() : BlankGuid;
}
