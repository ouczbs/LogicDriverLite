// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#include "SMNode_Base.h"
#include "SMUtils.h"
#include "SMLogging.h"
#include "SMNodeInstance.h"


FSMNode_Base::FSMNode_Base() : TimeInState(0), bIsInEndState(false), bHasUpdated(false), DuplicateId(0),
OwnerNode(nullptr),
OwningInstance(nullptr), NodeInstance(nullptr), NodeInstanceClass(nullptr),
bInitialized(false), bIsActive(false)
{
	/*
	 * Originally the Guid was initialized here. This caused warnings to show up during packaging because
	 * Unreal does safety checks on struct native constructors by comparing multiple initializations with different
	 * addresses and verifying each property matches. That doesn't work with a Guid because it is guaranteed to
	 * be unique each time.
	 */
}

void FSMNode_Base::Initialize(UObject* Instance)
{
	OwningInstance = Cast<USMInstance>(Instance);
	bInitialized = true;
	GraphEvaluator.Initialize(Instance);

	for (FSMExposedFunctionHandler& FunctionHandler : TransitionInitializedGraphEvaluators)
	{
		FunctionHandler.Initialize(Instance);
	}
	for (FSMExposedFunctionHandler& FunctionHandler : TransitionShutdownGraphEvaluators)
	{
		FunctionHandler.Initialize(Instance);
	}
	
	CreateNodeInstance();
}

void FSMNode_Base::Reset()
{
	GraphEvaluator.Reset();
	
	for (FSMExposedFunctionHandler& FunctionHandler : TransitionInitializedGraphEvaluators)
	{
		FunctionHandler.Reset();
	}
	for (FSMExposedFunctionHandler& FunctionHandler : TransitionShutdownGraphEvaluators)
	{
		FunctionHandler.Reset();
	}
}

const FGuid& FSMNode_Base::GetNodeGuid() const
{
	return Guid;
}

void FSMNode_Base::GenerateNewNodeGuid()
{
	SetNodeGuid(FGuid::NewGuid());
}

const FGuid& FSMNode_Base::GetGuid() const
{
	return PathGuid;
}

void FSMNode_Base::CalculatePathGuid(TMap<FString, int32>& MappedPaths)
{
	USMUtils::PathToGuid(GetGuidPath(MappedPaths), &PathGuid);
}

FString FSMNode_Base::GetGuidPath(TMap<FString, int32>& MappedPaths) const
{
	TArray<const FSMNode_Base*> Owners;
	USMUtils::TryGetAllOwners(this, Owners);
	return USMUtils::BuildGuidPathFromNodes(Owners, &MappedPaths);
}

void FSMNode_Base::GenerateNewNodeGuidIfNotSet()
{
	if (Guid.IsValid())
	{
		return;
	}

	GenerateNewNodeGuid();
}

void FSMNode_Base::SetNodeGuid(const FGuid& NewGuid)
{
	Guid = NewGuid;
}

void FSMNode_Base::SetOwnerNodeGuid(const FGuid& NewGuid)
{
	OwnerGuid = NewGuid;
}

void FSMNode_Base::SetOwnerNode(FSMNode_Base* Owner)
{
	OwnerNode = Owner;
}

void FSMNode_Base::CreateNodeInstance()
{
	if (!NodeInstanceClass)
	{
		SetNodeInstanceClass(GetDefaultNodeInstanceClass());

		check(NodeInstanceClass);
	}

	UObject* TemplateInstance = nullptr;
	if (TemplateName != NAME_None && OwningInstance)
	{
		TemplateInstance = USMUtils::FindTemplateFromInstance(OwningInstance, TemplateName);
		if (TemplateInstance == nullptr)
		{
			LD_LOG_ERROR(TEXT("Could not find node template %s for use on node %s from package %s. Loading defaults."), *TemplateName.ToString(), *GetNodeName(), *OwningInstance->GetName());
		}
	}

	NodeInstance = NewObject<USMNodeInstance>(OwningInstance, NodeInstanceClass, NAME_None, RF_NoFlags, TemplateInstance);
	NodeInstance->SetOwningNode(this);
}

void FSMNode_Base::SetNodeInstanceClass(UClass* NewNodeInstanceClass)
{
	if (NewNodeInstanceClass && !IsNodeInstanceClassCompatible(NewNodeInstanceClass))
	{
		LD_LOG_ERROR(TEXT("Could not set node instance class %s on node %s. The types are not compatible."), *NewNodeInstanceClass->GetName(), *GetNodeName());
		return;
	}

	NodeInstanceClass = NewNodeInstanceClass;
}

bool FSMNode_Base::IsNodeInstanceClassCompatible(UClass* NewNodeInstanceClass) const
{
	ensureMsgf(false, TEXT("FSMNode_Base IsNodeInstanceClassCompatible hit for node %s and instance class %s. This should always be overidden in child classes."),
		*GetNodeName(), NewNodeInstanceClass ? *NewNodeInstanceClass->GetName() : TEXT("None"));
	return false;
}

void FSMNode_Base::SetNodeName(const FString& Name)
{
	NodeName = Name;
}

void FSMNode_Base::SetTemplateName(const FName& Name)
{
	TemplateName = Name;
}

void FSMNode_Base::ExecuteInitializeNodes()
{
	USMUtils::ExecuteGraphFunctions(TransitionInitializedGraphEvaluators);
}

void FSMNode_Base::ExecuteShutdownNodes()
{
	USMUtils::ExecuteGraphFunctions(TransitionShutdownGraphEvaluators);
}

void FSMNode_Base::Execute()
{
	if (!bInitialized)
	{
		return;
	}

	UpdateReadStates();

	GraphEvaluator.Execute();
}

void FSMNode_Base::SetActive(bool bValue)
{
#if WITH_EDITORONLY_DATA
	bWasActive = bIsActive;
#endif
	bIsActive = bValue;
}
