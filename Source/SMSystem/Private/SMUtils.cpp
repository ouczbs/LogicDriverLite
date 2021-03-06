// Copyright Recursoft LLC 2019-2020. All Rights Reserved.

#include "SMUtils.h"
//#include "Blueprints/SMBlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "SMLogging.h"


USMInstance* USMBlueprintUtils::CreateStateMachineInstance(TSubclassOf<class USMInstance> StateMachineClass, UObject* Context)
{
	return CreateStateMachineInstanceInternal(StateMachineClass, Context, nullptr);
}

USMInstance* USMBlueprintUtils::CreateStateMachineInstanceFromTemplate(TSubclassOf<USMInstance> StateMachineClass,
	UObject* Context, USMInstance* Template)
{
	return CreateStateMachineInstanceInternal(StateMachineClass, Context, Template);
}

USMInstance* USMBlueprintUtils::CreateStateMachineInstanceInternal(TSubclassOf<USMInstance> StateMachineClass,
	UObject* Context, USMInstance* Template)
{
	if (StateMachineClass.Get() == nullptr)
	{
		LD_LOG_ERROR(TEXT("No state machine class provided to CreateStateMachineInstance for context: %s"), Context ? *Context->GetName() : TEXT("No Context"));
		return nullptr;
	}

	if (Template && Template->GetClass() != StateMachineClass)
	{
		LD_LOG_ERROR(TEXT("Attempted to instantiate state machine with template of class %s but was expecting: %s. Try restarting the play session."), *Template->GetClass()->GetName(), *StateMachineClass->GetName());
		return nullptr;
	}

	USMInstance* Instance = NewObject<USMInstance>(Context, StateMachineClass, NAME_None, RF_NoFlags, Template);
	Instance->Initialize(Context);

	return Instance;
}

bool USMUtils::GenerateStateMachine(UObject* Instance, FSMStateMachine& StateMachineOut,
	const TSet<FStructProperty*>& RunTimeProperties, bool bDryRun)
{
	// State machines that contain references to each other can risk stack overflow. Let's track the ones being generated for a specific thread.
	static TMap<uint32, GeneratingStateMachines> StateMachinesGeneratingForThread;
	const uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();

	// Check if we're at the top of the stack for this generation.
	const bool bIsTopLevel = !StateMachinesGeneratingForThread.Contains(ThreadId);
	/* TODO: If we ever support multi-threaded initialization we need to re-evaluate how we are retrieving the CurrentGeneration
	 * as the reference could become invalid after a map resize from a removal. */
	GeneratingStateMachines& CurrentGeneration = StateMachinesGeneratingForThread.FindOrAdd(ThreadId);

	// If the state machine is a reference instantiate its blueprint and pass our context in.
	if (TSubclassOf<USMInstance> StateMachineClassReference = StateMachineOut.GetClassReference())
	{
		if (USMInstance* SMInstance = Cast<USMInstance>(Instance))
		{
			USMInstance* TemplateInstance = nullptr;
			if (!bDryRun)
			{
				// Check if we are using a template.
				const FName& TemplateName = StateMachineOut.GetReferencedTemplateName();
				if (TemplateName != NAME_None)
				{
					TemplateInstance = Cast<USMInstance>(FindTemplateFromInstance(SMInstance, TemplateName));
					if (TemplateInstance == nullptr)
					{
						LD_LOG_ERROR(TEXT("Could not find reference template %s for use within state machine %s from package %s. Loading defaults."), *TemplateName.ToString(), *StateMachineOut.GetNodeName(), *Instance->GetName());
					}
					else if (TemplateInstance->GetClass() != StateMachineClassReference)
					{
						/*
						 * This error can occur when setting an sm actor comp state machine class, then switching it to another that uses a reference with a template.
						 * The ReferencedStateMachineClass in the FSM struct will be set to the value of the class that was just placed in the actor component,
						 * but nothing else appears to be out of place. This problem occurs until the sm with the reference is recompiled or the editor restarted.
						 * 
						 * Fix for now: The template instance appears to be correct, so use that and log a warning.
						 *
						 * It is unknown what causes it specifically since this happens in the runtime module when setting the actor component class. Somehow
						 * this effects the ReferencedStateMachineClass in the struct owning the sm reference template. The most likely cause would be in the component under
						 * InitInstanceTemplate when CopyPropertiesForUnrelatedObjects is called. But setting BlueprintCompiledGeneratedDefaults on ReferencedStateMachineClass
						 * had no effect.
						 */
						
						LD_LOG_WARNING(TEXT("State machine node %s in package %s uses a reference template %s with class %s, but was expecting class %s. The package may just need to be recompiled."),
							*StateMachineOut.GetNodeName(), *Instance->GetName(), *TemplateName.ToString(), *TemplateInstance->GetClass()->GetName(), *StateMachineClassReference->GetName());

						StateMachineClassReference = TemplateInstance->GetClass();
					}
				}
			}
			// Check for circular referencing. Behavior varies based on normal instantiation approach or legacy instance reuse.
			{
				// Reuse behavior: Reuse the same instance.
				if (StateMachineOut.bReuseReference)
				{
					if (CurrentGeneration.CreatedReferences.Contains(StateMachineClassReference))
					{
						if (USMInstance* AlreadyInstantiated = CurrentGeneration.CreatedReferences.FindRef(StateMachineClassReference))
						{
							StateMachineOut.SetInstanceReference(AlreadyInstantiated);
						}
						else
						{
							// Currently in process of being instantiated, we can set it later.
							TSet<FSMStateMachine*>& StateMachinesWithoutReferences = CurrentGeneration.StateMachinesThatNeedReferences.FindOrAdd(StateMachineClassReference);
							StateMachinesWithoutReferences.Add(&StateMachineOut);
						}
						FinishStateMachineGeneration(bIsTopLevel, StateMachinesGeneratingForThread, ThreadId);
						return true;
					}
					// Record instance created.
					CurrentGeneration.CreatedReferences.Add(StateMachineClassReference, nullptr);
				}
				// Normal use.
				else
				{
					// Prevent infinite loop if this machine references itself.
					if (int32* CurrentInstancesOfClass = CurrentGeneration.InstancesGenerating.Find(StateMachineClassReference))
					{
						// This should never be greater than 1 otherwise it means this state machine class has a reference to itself.
						// If we don't stop here we will be in an infinite loop.
						if (*CurrentInstancesOfClass > 1)
						{
							LD_LOG_ERROR(TEXT("Attempted to generate state machine with circular referencing. This behavior is no longer allowed but can still be achieved by setting bReuseReference to true on the state machine reference node. Offending state machine: %s"), *SMInstance->GetName())
							FinishStateMachineGeneration(bIsTopLevel, StateMachinesGeneratingForThread, ThreadId);
							return false;
						}
					}
				}
			}

			if (!bDryRun)
			{
				int32& CurrentInstances = CurrentGeneration.InstancesGenerating.FindOrAdd(StateMachineClassReference);
				CurrentInstances++;

				// Instantiate template.
				USMInstance* ReferencedInstance = USMBlueprintUtils::CreateStateMachineInstanceFromTemplate(StateMachineClassReference, SMInstance->GetContext(), TemplateInstance);
				if (ReferencedInstance == nullptr)
				{
					LD_LOG_ERROR(TEXT("Could not create reference %s for use within state machine %s from package %s."), *StateMachineClassReference->GetName(), *StateMachineOut.GetNodeName(), *Instance->GetName());
					return false;
				}
				
				ReferencedInstance->SetReferenceOwner(SMInstance);

				// Reuse behavior: The instantiation process may have nested state machine references which loop back to this reference.
				{
					if (StateMachineOut.bReuseReference)
					{
						CurrentGeneration.CreatedReferences[StateMachineClassReference] = ReferencedInstance;
					}

					if (TSet<FSMStateMachine*>* StateMachinesWithoutReferences = CurrentGeneration.StateMachinesThatNeedReferences.Find(StateMachineClassReference))
					{
						for (FSMStateMachine* StateMachine : *StateMachinesWithoutReferences)
						{
							if (StateMachine->bReuseReference)
							{
								StateMachine->SetInstanceReference(ReferencedInstance);
							}
						}
					}
				}
				
				int32* CurrentInstancesAfter = CurrentGeneration.InstancesGenerating.Find(StateMachineClassReference);
				if (ensureAlwaysMsgf(CurrentInstancesAfter, TEXT("The reference class instance %s should be found."), *StateMachineClassReference->GetName()))
				{
					// Should go back to zero but could be more in the event of an attempted self reference.
					(*CurrentInstancesAfter)--;
				}

				// Notify the state machine of the correct instance.
				StateMachineOut.SetInstanceReference(ReferencedInstance);
			}
			
			FinishStateMachineGeneration(bIsTopLevel, StateMachinesGeneratingForThread, ThreadId);
			return true;
		}
	}

	// Only match properties belonging to this state machine.
	const FGuid& StateMachineNodeGuid = StateMachineOut.GetNodeGuid();

	// Used for quick lookup when linking to states.
	TMap<FGuid, FSMState_Base*> MappedStates;
	TMap<FGuid, FSMTransition*> MappedTransitions;
	// Retrieve pointers to the runtime states and store in state machine for quick access.
	for (auto& Property : RunTimeProperties)
	{
		if (Property->Struct->IsChildOf(FSMState_Base::StaticStruct()))
		{
			FSMState_Base* State = Property->ContainerPtrToValuePtr<FSMState_Base>(Instance);

			if (State->GetOwnerNodeGuid() != StateMachineNodeGuid)
			{
				continue;
			}

			StateMachineOut.AddState(State);

			/*
			 * Unique GUID check 1:
			 * The NodeGuid at this stage should always be unique and the ensure should never be tripped.
			 * Multiple inheritance parent calls is the only scenario where NodeGuid duplicates could exist but the sm compiler will adjust them.
			 *
			 * If this is triggered please check to make sure the state machine blueprint in question doesn't do anything abnormal such as use circular referencing.
			 */
			ensureAlwaysMsgf(!MappedStates.Contains(State->GetNodeGuid()), TEXT("State machine generation error for state machine %s: found node %s but its guid %s has already been added."),
				*Instance->GetName(), *State->GetNodeName(), *State->GetNodeGuid().ToString());
			
			MappedStates.Add(State->GetNodeGuid(), State);

			if (Property->Struct->IsChildOf(FSMStateMachine::StaticStruct()))
			{
				FSMStateMachine& NestedStateMachine = *(FSMStateMachine*)State;
				GenerateStateMachine(Instance, NestedStateMachine, RunTimeProperties, bDryRun);
			}

			if (State->IsRootNode())
			{
				StateMachineOut.AddInitialState(State);
			}
		}
	}

	// Second pass build transitions.
	for (auto& Property : RunTimeProperties)
	{
		if (Property->Struct->IsChildOf(FSMTransition::StaticStruct()))
		{
			FSMTransition* Transition = Property->ContainerPtrToValuePtr<FSMTransition>(Instance);

			if (Transition->GetOwnerNodeGuid() != StateMachineNodeGuid)
			{
				continue;
			}

			// Convert linked guids to the actual states.
			FSMState_Base* FromState = MappedStates.FindRef(Transition->FromGuid);
			if (!FromState)
			{
				LD_LOG_ERROR(TEXT("Critical error creating state machine %s for package %s. The transition %s could not locate the FromState using Guid %s."), *StateMachineOut.GetNodeName(), *Instance->GetName(),
					*Transition->GetNodeName(), *Transition->FromGuid.ToString());
				return false;
			}
			FSMState_Base* ToState = MappedStates.FindRef(Transition->ToGuid);
			if (!ToState)
			{
				LD_LOG_ERROR(TEXT("Critical error creating state machine %s for package %s. The transition %s could not locate the ToState using Guid %s."), *StateMachineOut.GetNodeName(), *Instance->GetName(),
					*Transition->GetNodeName(), *Transition->ToGuid.ToString());
				return false;
			}

			// The transition will handle updating the state.
			Transition->SetFromState(FromState);
			Transition->SetToState(ToState);

			StateMachineOut.AddTransition(Transition);
			
			/*
			 * Unique GUID check 1:
			 * The NodeGuid at this stage should always be unique and the ensure should never be tripped.
			 * Multiple inheritance parent calls is the only scenario where NodeGuid duplicates could exist but the sm compiler will adjust them.
			 *
			 * If this is triggered please check to make sure the state machine blueprint in question doesn't do anything abnormal such as use circular referencing.
			 */
			ensureAlwaysMsgf(!MappedStates.Contains(Transition->GetNodeGuid()), TEXT("State machine generation error for state machine %s: found node %s but its guid %s has already been added."),
				*Instance->GetName(), *Transition->GetNodeName(), *Transition->GetNodeGuid().ToString());
			
			MappedTransitions.Add(Transition->GetNodeGuid(), Transition);
		}
	}

	FinishStateMachineGeneration(bIsTopLevel, StateMachinesGeneratingForThread, ThreadId);
	return true;
}

bool USMUtils::TryGetStateMachinePropertiesForClass(UClass* Class, TSet<FStructProperty*>& PropertiesOut, FGuid& RootGuid, EFieldIteratorFlags::SuperClassFlags SuperFlags)
{
	// Look for properties in this class.
	for (TFieldIterator<FStructProperty> It(Class, SuperFlags); It; ++It)
	{
		if (It->Struct->IsChildOf(FSMNode_Base::StaticStruct()))
		{
			PropertiesOut.Add(*It);
		}
	}

	// Check parent classes.
	if (PropertiesOut.Num() == 0)
	{
		/*
		// Blueprint parent.
		if (USMBlueprintGeneratedClass* NextClass = Cast<USMBlueprintGeneratedClass>(Class->GetSuperClass()))
		{
			// Need to set the guid -- The child class instance won't know this.
			RootGuid = NextClass->GetRootGuid();
			return TryGetStateMachinePropertiesForClass(NextClass, PropertiesOut, RootGuid, SuperFlags);
		}
		*/
		// Nativized parent.
		if (UDynamicClass* NextClass = Cast<UDynamicClass>(Class->GetSuperClass()))
		{
			// Need to set the guid -- The child class instance won't know this.
			RootGuid = CastChecked<USMInstance>(NextClass->GetDefaultObject())->RootStateMachineGuid;
			return TryGetStateMachinePropertiesForClass(NextClass, PropertiesOut, RootGuid, SuperFlags);
		}
	}

	return PropertiesOut.Num() > 0;
}

void USMUtils::TryGetAllOwners(const FSMNode_Base* Node, TArray<const FSMNode_Base*>& OwnersOrdered, USMInstance* LimitToInstance)
{
	for(const FSMNode_Base* CurrentNode = Node; CurrentNode; CurrentNode = CurrentNode->GetOwnerNode())
	{
		USMInstance* Instance = CurrentNode->GetOwningInstance();
		if (LimitToInstance && Instance != LimitToInstance)
		{
			continue;
		}
		
		OwnersOrdered.Add(CurrentNode);
	}

	Algo::Reverse(OwnersOrdered);
}

FString USMUtils::BuildGuidPathFromNodes(const TArray<const FSMNode_Base*>& Nodes, TMap<FString, int32>* MappedPaths)
{
	FString Path;
	for (int32 i = 0; i < Nodes.Num() - 1; ++i)
	{
		const FSMNode_Base* Node = Nodes[i];
		Path += Node->GetNodeGuid().ToString() + "/";
	}

	if (Nodes.Num())
	{
		Path += Nodes[Nodes.Num() - 1]->GetNodeGuid().ToString();
	}

	// Check for duplicates and adjust.
	if (MappedPaths)
	{
		int32& ExistingPaths = MappedPaths->FindOrAdd(Path);
		if (++ExistingPaths > 1)
		{
			Path += "_" + FString::FromInt(ExistingPaths - 1);
		}
	}
	
	return Path;
}

FGuid USMUtils::PathToGuid(const FString& UnhashedPath, FGuid* OutGuid)
{
	FGuid Guid;
	if(!OutGuid)
	{
		OutGuid = &Guid;
	}
	
	FGuid::Parse(FMD5::HashAnsiString(*UnhashedPath), *OutGuid);
	return *OutGuid;
}

void USMUtils::ExecuteGraphFunctions(TArray<FSMExposedFunctionHandler>& GraphFunctions)
{
	for (const FSMExposedFunctionHandler& FunctionHandler : GraphFunctions)
	{
		FunctionHandler.Execute();
	}
}

UObject* USMUtils::FindTemplateFromInstance(USMInstance* Instance, const FName& TemplateName)
{
	UClass* CurrentClass = Instance->GetClass();
	while (CurrentClass)
	{
		if (UObject* FoundTemplate = Cast<UObject>(CurrentClass->GetDefaultSubobjectByName(TemplateName)))
		{
			return FoundTemplate;
		}
		CurrentClass = CurrentClass->GetSuperClass();
	}

	return nullptr;
}

bool USMUtils::TryGetAllReferenceTemplatesFromInstance(USMInstance* Instance, TSet<USMInstance*>& TemplatesOut, bool bIncludeNested)
{
	for (UObject* Template : Instance->ReferenceTemplates)
	{
		USMInstance* ReferenceTemplate = Cast<USMInstance>(Template);
		
		if(!ReferenceTemplate)
		{
			continue;
		}
		
		TemplatesOut.Add(ReferenceTemplate);

		if (bIncludeNested)
		{
			TryGetAllReferenceTemplatesFromInstance(ReferenceTemplate, TemplatesOut, bIncludeNested);
		}
	}

	return TemplatesOut.Num() > 0;
}

bool USMUtils::FinishStateMachineGeneration(bool bIsTopLevel, TMap<uint32, GeneratingStateMachines>& ThreadMap, uint32 ThreadId)
{
	if (bIsTopLevel)
	{
		if (GeneratingStateMachines* Generation = ThreadMap.Find(ThreadId))
		{
#if !UE_BUILD_SHIPPING
			for (const auto& InstanceCount : Generation->InstancesGenerating)
			{
				ensureAlwaysMsgf(InstanceCount.Value == 0, TEXT("Ref count is %s when it should be 0. Offending class instance %s."), *FString::FromInt(InstanceCount.Value), *InstanceCount.Key->GetName());
			}
#endif		
			Generation->InstancesGenerating.Empty();
			Generation->CreatedReferences.Empty();
			Generation->StateMachinesThatNeedReferences.Empty();
		}

		ThreadMap.Remove(ThreadId);
	}

	return bIsTopLevel;
}
