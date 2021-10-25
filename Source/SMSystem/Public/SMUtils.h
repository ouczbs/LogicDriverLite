// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SMInstance.h"
#include "SMUtils.generated.h"


/**
 * General Blueprint helpers for creating state machines.
 */
UCLASS()
class SMSYSTEM_API USMBlueprintUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Create a new state machine instance initialized with the given context. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Logic Driver|State Machine Utilities")
	static USMInstance* CreateStateMachineInstance(TSubclassOf<class USMInstance> StateMachineClass, UObject* Context);

	/** Create a new state machine instance from a template and initialize with the given context. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Logic Driver|State Machine Utilities")
	static USMInstance* CreateStateMachineInstanceFromTemplate(TSubclassOf<class USMInstance> StateMachineClass, UObject* Context, USMInstance* Template);

private:
	static USMInstance* CreateStateMachineInstanceInternal(TSubclassOf<class USMInstance> StateMachineClass, UObject* Context, USMInstance* Template);
};

/**
 * Internal helpers around utilizing state machine instances.
 */
class SMSYSTEM_API USMUtils
{
public:
	/**
	 * Compiles a state machine from an object. (Should be SMInstance). Guid needs to be set correctly prior to calling.
	 *
	 * @param Instance The object containing instance data. Should be an USMInstance.
	 * @param StateMachineOut The state machine struct which will be assembled.
	 * @param RunTimeProperties Class properties which will be used to create the state machine.
	 * @param bDryRun Debugging flag to prevent templates and references from being assigned.
	 */
	static bool GenerateStateMachine(UObject* Instance, FSMStateMachine& StateMachineOut, const TSet<FStructProperty*>& RunTimeProperties, bool bDryRun = false);

	/** Locate the properties required for a state machine looking backwards up the parent classes. */
	static bool TryGetStateMachinePropertiesForClass(UClass* Class, TSet<FStructProperty*>& PropertiesOut, FGuid& RootGuid, EFieldIteratorFlags::SuperClassFlags SuperFlags = EFieldIteratorFlags::ExcludeSuper);

	/** Look up all node owners. Results will be ordered oldest to newest with the given Node as the last entry. */
	static void TryGetAllOwners(const FSMNode_Base* Node, TArray<const FSMNode_Base*>& OwnersOrdered, USMInstance* LimitToInstance = nullptr);

	/** Construct a path of guids from the nodes. */
	static FString BuildGuidPathFromNodes(const TArray<const FSMNode_Base*>& Nodes, TMap<FString, int32>* MappedPaths = nullptr);
	
	/** Convert an unhashed path to a hashed guid path. */
	static FGuid PathToGuid(const FString& UnhashedPath, FGuid* OutGuid = nullptr);

	/** Iterates through all functions executing them. */
	static void ExecuteGraphFunctions(TArray<FSMExposedFunctionHandler>& GraphFunctions);
	
	/** Search up parents for a default sub objects for a template. */
	static UObject* FindTemplateFromInstance(USMInstance* Instance, const FName& TemplateName);

	/** Find all reference templates from an instance. Nested children shouldn't be found after a compile or during run-time! */
	static bool TryGetAllReferenceTemplatesFromInstance(USMInstance* Instance, TSet<USMInstance*>& TemplatesOut, bool bIncludeNested = false);

	/** Iterate properties of an instance finding all structs derived from the given type (such as FSMNode_Base). */
	template<typename T>
	static bool TryGetAllRuntimeNodesFromInstance(USMInstance* Instance, TSet<T*>& NodesOut)
	{
		TSet<FStructProperty*> Properties;
		FGuid RootGuid;
		TryGetStateMachinePropertiesForClass(Instance->GetClass(), Properties, RootGuid, EFieldIteratorFlags::IncludeSuper);

		for (FStructProperty* Property : Properties)
		{
			if (Property->Struct->IsChildOf(T::StaticStruct()))
			{
				if (T* StateMachinePtr = Property->ContainerPtrToValuePtr<T>(Instance))
				{
					NodesOut.Add(StateMachinePtr);
				}
			}
		}

		return NodesOut.Num() > 0;
	}

	template<typename T>
	static void BlueprintPropertyToNativeProperty(FProperty* Property, UObject* Scope, TArray<T>& OutNativeProperties)
	{
		TArray<T*> PtrArray;

		BlueprintPropertyToNativeProperty(Property, Scope, PtrArray);
		for(T* Ptr : PtrArray)
		{
			OutNativeProperties.Add(*Ptr);
		}
	}
	
	template<typename T>
	static void BlueprintPropertyToNativeProperty(FProperty* Property, UObject* Scope, TArray<T*>& OutNativeProperties)
	{
		// Check if this is an array or if this is an element in an array.
		FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property);
		if(!ArrayProp)
		{
			if (FField* OwnerField = Property->GetOwnerProperty())
			{
				ArrayProp = CastField<FArrayProperty>(OwnerField);
			}
		}
		
		if (ArrayProp)
		{
			// Blueprint array.
			FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<uint8>(Scope));

			for(int32 i = 0; i < Helper.Num(); ++i)
			{
				if (uint8** ObjectPtr = (uint8**)Helper.GetRawPtr(i))
				{
					OutNativeProperties.Add((T*)*&ObjectPtr);
				}
			}
		}
		else if (Property->ArrayDim > 1)
		{
			// Native array.
			for (int32 i = 0; i < Property->ArrayDim; ++i)
			{
				if (uint8** ResolvedObject = Property->ContainerPtrToValuePtr<uint8*>(Scope, i))
				{
					OutNativeProperties.Add((T*)*&ResolvedObject);
				}
			}
		}
		else if(FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			// Regular property.
			if (StructProperty->Struct->IsChildOf(T::StaticStruct()))
			{
				if (T* Object = StructProperty->ContainerPtrToValuePtr<T>(Scope))
				{
					OutNativeProperties.Add(Object);
				}
			}
		}
	}
private:
	struct GeneratingStateMachines
	{
		TMap<const TSubclassOf<USMInstance>, int32> InstancesGenerating;
		TMap<const TSubclassOf<USMInstance>, USMInstance*> CreatedReferences;
		TMap<const TSubclassOf<USMInstance>, TSet<FSMStateMachine*>> StateMachinesThatNeedReferences;
	};

	/** Returns true if the state machine has completely finished generation. Can only be true when called from the top of the stack. */
	static bool FinishStateMachineGeneration(bool bIsTopLevel, TMap<uint32, GeneratingStateMachines>& ThreadMap, uint32 ThreadId);
};
