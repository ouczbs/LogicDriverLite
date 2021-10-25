// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#include "SMTestHelpers.h"
#include "Modules/ModuleManager.h"
#include "Misc/AutomationTest.h"
#include "PackageTools.h"
#include "ObjectTools.h"
#include "HAL/FileManager.h"
#include "AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UnrealEd.h"
#include "BlueprintEditorSettings.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "SMBlueprintFactory.h"
#include "Blueprints/SMBlueprint.h"
#include "SMTestContext.h"
#include "SMUtils.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateEntryNode.h"
#include "Graph/SMStateGraph.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionResultNode.h"
#include "Graph/SMTransitionGraph.h"
#include "Graph/SMGraph.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateUpdateNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_StateEndNode.h"
#include "Graph/Nodes/Helpers/SMGraphK2Node_StateReadNodes.h"
#include "K2Node_FunctionEntry.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMNodeInstanceUtils.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_IntermediateNodes.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionEnteredNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionInitializedNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionShutdownNode.h"


bool FAssetHandler::CreateAsset()
{
	const FString PackageName = GamePath + Name;

	UPackage* AssetPackage = CreatePackage(nullptr, *PackageName);
	Package = AssetPackage;

	const EObjectFlags Flags = RF_Public | RF_Standalone;

	Object = Factory->FactoryCreateNew(Class, Package, FName(*Name), Flags, nullptr, GWarn);
	if (Object)
	{
		FAssetRegistryModule::AssetCreated(Object);

		Package->MarkPackageDirty();

		return true;
	}

	return false;
}

bool FAssetHandler::SaveAsset()
{
	if (Package && Object)
	{
		Package->SetDirtyFlag(true);
		const FString PackagePath = FString::Printf(TEXT("%s%s"), *GamePath, *Name);

		return UPackage::SavePackage(Package, nullptr, RF_Standalone, *FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension()),
			GError, nullptr, false, true, SAVE_NoError);
	}

	return false;
}

bool FAssetHandler::LoadAsset()
{
	Package = nullptr;
	Object = nullptr;

	const FString PackagePath = FString::Printf(TEXT("%s%s"), *GamePath, *Name);

	Package = LoadPackage(nullptr, *FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension()), LOAD_None);
	if(!Package)
	{
		return false;
	}

	Object = LoadObject<USMBlueprint>(Package, *Name);

	return Object != nullptr;
}

bool FAssetHandler::DeleteAsset()
{
	bool bSuccessful = false;

	if(Object)
	{
		bSuccessful = ObjectTools::DeleteSingleObject(Object, false);

		// Try force delete.
		if (!bSuccessful)
		{
			TArray<UObject*> Objects = TArray<UObject*>();
			Objects.Add(Object);
			bSuccessful = ObjectTools::ForceDeleteObjects(Objects, false) > 0;
		}

		// Delete the package.
		if (bSuccessful)
		{
			FString PackageFilename;
			if (FPackageName::DoesPackageExist(Package->GetName(), nullptr, &PackageFilename))
			{
				TArray<UPackage*> PackagesToDelete;
				PackagesToDelete.Add(Package);

				// Let the package auto-saver know that it needs to ignore the deleted packages.
				GUnrealEd->GetPackageAutoSaver().OnPackagesDeleted(PackagesToDelete);

				Package->SetDirtyFlag(false);

				// Unload the packages and collect garbage.
				UPackageTools::UnloadPackages(PackagesToDelete);

				IFileManager::Get().Delete(*PackageFilename);
			}
		}
	}
	return bSuccessful;
}

bool FAssetHandler::UnloadAsset()
{
	bool bResult = false;
	if (Object)
	{
		Object->ConditionalBeginDestroy();
	}
	
	if (Package)
	{
		TArray<UPackage*> Packages{ Package };
		bResult = UPackageTools::UnloadPackages(Packages);
	}
	
	Package = nullptr;
	Object = nullptr;

	GEngine->ForceGarbageCollection(true);

	return bResult;
}

bool FAssetHandler::CreateAsset(FAutomationTestBase* Test)
{
	const bool bResult = CreateAsset();
	Test->TestTrue("Asset should have created", bResult);
	return bResult;
}

bool FAssetHandler::SaveAsset(FAutomationTestBase* Test)
{
	const bool bResult = SaveAsset();
	Test->TestTrue("Asset should have saved", bResult);
	return bResult;
}

bool FAssetHandler::LoadAsset(FAutomationTestBase* Test)
{
	const bool bResult = LoadAsset();
	Test->TestTrue("Asset should have loaded", bResult);
	return bResult;
}

bool FAssetHandler::DeleteAsset(FAutomationTestBase* Test)
{
	const bool bResult = DeleteAsset();
	Test->TestTrue("Asset should have been deleted", bResult);
	return bResult;
}

bool FAssetHandler::UnloadAsset(FAutomationTestBase* Test)
{
	const bool bResult = UnloadAsset();
	Test->TestTrue("Asset should have been unloaded", bResult);
	return bResult;
}

void FAssetHandler::ValidateDirectory()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	//Create the folder if it doesn't already exist
	if (!IFileManager::Get().DirectoryExists(*DefaultFullPath()))
	{
		//Make the new folder
		IFileManager::Get().MakeDirectory(*DefaultFullPath(), true);

		// Add the path to the asset registry
		AssetRegistryModule.Get().AddPath(GamePath);
	}
}

USMInstance* TestHelpers::CreateNewStateMachineInstanceFromBP(FAutomationTestBase* Test, USMBlueprint* Blueprint, USMTestContext* Context, bool bTestNodeMap)
{
	USMInstance* StateMachineInstance = USMBlueprintUtils::CreateStateMachineInstance(Blueprint->GetGeneratedClass(), Context);
	Test->TestNotNull("State Machine should not be null", StateMachineInstance);
	Test->TestTrue("State Machine should be initialized", StateMachineInstance->IsInitialized());

	if (bTestNodeMap)
	{
		// Nodes mapped via generated guid.
		const TMap<FGuid, FSMNode_Base*>& NodeMap = StateMachineInstance->GetNodeMap();

		// Search for actual nodes.
		const TArray<FSMNode_Base*> Nodes = StateMachineInstance->GetRootStateMachine().GetAllNodes(true);
		
		TMap<USMInstance*, int32> ReferenceInstanceCount;
		TArray<FSMStateMachine*> ReferencedStateMachines = StateMachineInstance->GetStateMachinesWithReferences(true);
		TArray<USMInstance*> TotalReferences;

		// Only valid if reference is actually reused.
		bool bHasReusedInstances = false;
		// Total amount reused not including base instance.
		int32 TotalReused = 0;
		for(FSMStateMachine* SM : ReferencedStateMachines)
		{
			TotalReferences.Add(SM->GetInstanceReference());

			int32& Count = ReferenceInstanceCount.FindOrAdd(SM->GetInstanceReference());
			if(++Count > 1)
			{
				Test->TestTrue("Reused reference is correct", SM->bReuseReference);
				bHasReusedInstances = true;
				TotalReused++;
			}
		}
		const TSet<USMInstance*> UniqueReferences(TotalReferences);

		if(bHasReusedInstances)
		{
			Test->TestEqual("Reused unique reference count less than total count", UniqueReferences.Num(), TotalReferences.Num() - TotalReused);

			int32 TotalDupedNodes = 0;
			for(auto& KeyVal : ReferenceInstanceCount)
			{
				if(KeyVal.Value == 1)
				{
					continue;
				}

				const TArray<FSMNode_Base*> DupeNodes = KeyVal.Key->GetRootStateMachine().GetAllNodes(true);
				TotalDupedNodes += DupeNodes.Num();
			}

			Test->TestEqual("Calculated node hashes matches node count", NodeMap.Num(), Nodes.Num() - TotalDupedNodes);
		}
		else
		{
			Test->TestEqual("References are not reused and unique", UniqueReferences.Num(), TotalReferences.Num());

			TSet<FSMNode_Base*> UniqueNodes(Nodes);
			Test->TestEqual("All nodes unique", UniqueNodes.Num(), Nodes.Num());
			
			// They should be the same less 1 because the root state machine used to initiate the search isn't counted.
			Test->TestEqual("Calculated node hashes matches node count", NodeMap.Num(), Nodes.Num() + 1);
		}
	}
	
	return StateMachineInstance;
}

FAssetHandler TestHelpers::ConstructNewStateMachineAsset()
{
	FString AssetName(FGuid::NewGuid().ToString());

	return FAssetHandler(AssetName, USMBlueprint::StaticClass(), NewObject<USMBlueprintFactory>());
}

bool TestHelpers::TryCreateNewStateMachineAsset(FAutomationTestBase* Test, FAssetHandler& NewAsset, bool Save)
{
	NewAsset = ConstructNewStateMachineAsset();

	if (!NewAsset.CreateAsset(Test))
	{
		return false;
	}

	if (!Save)
	{
		return true;
	}

	if (!NewAsset.SaveAsset(Test))
	{
		return false;
	}

	return true;
}

UK2Node_CallFunction* TestHelpers::CreateContextGetter(FAutomationTestBase* Test, UEdGraph* Graph, UEdGraphPin** ContextOutPin)
{
	UK2Node_CallFunction* GetContextNode = NewObject<UK2Node_CallFunction>(Graph);
	UFunction* MakeNodeFunction = USMInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMInstance, GetContext));
	GetContextNode->CreateNewGuid();
	GetContextNode->PostPlacedNewNode();
	GetContextNode->SetFromFunction(MakeNodeFunction);
	GetContextNode->SetFlags(RF_Transactional);
	GetContextNode->AllocateDefaultPins();
	UEdGraphSchema_K2::SetNodeMetaData(GetContextNode, FNodeMetadata::DefaultGraphNode);
	Graph->AddNode(GetContextNode);

	UEdGraphPin** FoundPin = GetContextNode->Pins.FindByPredicate([&](UEdGraphPin* Pin)
	{
		return Pin->Direction == EGPD_Output;
	});

	Test->TestNotNull("Expected to find ContextOutPin", FoundPin);

	if(FoundPin)
	{
		*ContextOutPin = *FoundPin;
	}

	return GetContextNode;
}

UK2Node_CallFunction* TestHelpers::CreateFunctionCall(UEdGraph* Graph, UFunction* Function)
{
	return FSMBlueprintEditorUtils::CreateFunctionCall(Graph, Function);
}

UK2Node_DynamicCast* TestHelpers::CreateAndLinkPureCastNode(FAutomationTestBase* Test, UEdGraph* Graph, UEdGraphPin* ObjectOutPin, UEdGraphPin* ObjectInPin)
{
	// These pins won't connect, attempt to auto create a cast node.
	UBlueprintEditorSettings* BlueprintSettings = GetMutableDefault<UBlueprintEditorSettings>();
	const bool bOriginalAutoCast = BlueprintSettings->bAutoCastObjectConnections;
	BlueprintSettings->bAutoCastObjectConnections = true;
	Test->TestTrue("Editor Settings - Auto Cast Settings needs to be true or the test can't place Cast Nodes", BlueprintSettings->bAutoCastObjectConnections);

	const UEdGraphSchema_K2* GraphSchema = CastChecked<UEdGraphSchema_K2>(Graph->GetSchema());

	class UK2Node* CreatedCastNode;
	Test->TestTrue("Tried to create conversion node", GraphSchema->FindSpecializedConversionNode(ObjectOutPin, ObjectInPin, true, CreatedCastNode));

	check(CreatedCastNode != nullptr); // If this fails you need to enable bAutoCastObjectConnections under Editor Settings.

	BlueprintSettings->bAutoCastObjectConnections = bOriginalAutoCast;
	
	CreatedCastNode->Rename(nullptr, Graph, RF_Transactional);
	CreatedCastNode->CreateNewGuid();
	//CreatedCastNode->PostPlacedNewNode(); // This will reset bIsPureCast unless project is set to use bFavorPureCast.
	CreatedCastNode->AllocateDefaultPins();
	Graph->AddNode(CreatedCastNode);

	UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(CreatedCastNode);
	Test->TestNotNull("Conversion should be DynamicCastNode", CastNode);
	CastNode->SetPurity(true);
	
	// The original object input pin to the cast.
	UEdGraphPin* CastedObjectSourcePin = GetCastSourcePin(CastNode);
	Test->TestNotNull("CastSourcePin should exist", CastedObjectSourcePin);

	// Connect the object to be casted.
	Test->TestTrue("Tried to make connection from original object to casted object source", GraphSchema->TryCreateConnection(ObjectOutPin, CastedObjectSourcePin));

	// The casted object type pin.
	UEdGraphPin* CastedObjectOutPin = GetCastResultPin(CastNode);
	Test->TestNotNull("CastResultPin should exist", CastedObjectOutPin);

	// Wire the casted result pin to the object in pin.
	Test->TestTrue("Tried to make connection from casted object result to logic node", GraphSchema->TryCreateConnection(CastedObjectOutPin, ObjectInPin));

	return CastNode;
}

void TestHelpers::TestNodeHasGuid(FAutomationTestBase* Test, FSMNode_Base* RuntimeNode)
{
	check(RuntimeNode);
	Test->TestTrue("GUID generated for runtime node", RuntimeNode->GetNodeGuid().IsValid());
}

void TestHelpers::BuildLinearStateMachine(FAutomationTestBase* Test, USMGraph* StateMachineGraph, int32 NumStates, UEdGraphPin** FromPinInOut, UClass* StateClass, UClass* TransitionClass)
{
	TestNodeHasGuid(Test, StateMachineGraph->GetRuntimeNode());
	
	// Choose start pin.
	UEdGraphPin* FromPin = FromPinInOut && *FromPinInOut ? *FromPinInOut : StateMachineGraph->GetEntryNode()->GetOutputPin();

	for(int32 i = 0; i < NumStates; ++i)
	{
		// Add a new node.
		USMGraphNode_StateNode* StateNode = TestHelpers::CreateNewNode<USMGraphNode_StateNode>(Test, StateMachineGraph, FromPin);
		TestNodeHasGuid(Test, StateNode->FindRuntimeNode());

		SetNodeClass(Test, StateNode, StateClass);
		
		// Create logic we can execute on a context.
		AddStateEntryLogic(Test, StateNode);
		AddStateUpdateLogic(Test, StateNode);
		AddStateEndLogic(Test, StateNode);

		// Make sure a transition can exist.
		if(FromPin->GetOwningNode()->IsA<USMGraphNode_StateNodeBase>())
		{
			// Make the transition able to be taken.
			USMGraphNode_TransitionEdge* Transition = CastChecked<USMGraphNode_TransitionEdge>(StateNode->GetInputPin()->LinkedTo[0]->GetOwningNode());
			TestNodeHasGuid(Test, Transition->FindRuntimeNode());

			SetNodeClass(Test, Transition, TransitionClass);
			
			AddTransitionResultLogic(Test, Transition);
		}

		// For wiring the next state.
		FromPin = StateNode->GetOutputPin();
	}

	// Most recent outgoing pin.
	if(FromPinInOut)
	{
		*FromPinInOut = FromPin;
	}
}

void TestHelpers::BuildBranchingStateMachine(FAutomationTestBase* Test, USMGraph* StateMachineGraph, int32 Rows,
	int32 Branches, bool bRunParallel, TArray<UEdGraphPin*>* FromPinsInOut, bool bLeaveActive, bool bReEnterStates, bool bEvalIfNextStateActive, UClass* StateClass, UClass* TransitionClass)
{
	TestNodeHasGuid(Test, StateMachineGraph->GetRuntimeNode());

	// Choose start pin.
	TArray<UEdGraphPin*> FromPins = FromPinsInOut && FromPinsInOut->Num() ? *FromPinsInOut : TArray<UEdGraphPin*>{ StateMachineGraph->GetEntryNode()->GetOutputPin() };

	for (int32 r = 0; r < Rows; ++r)
	{
		TArray<UEdGraphPin*> NextFromPins;
		for (UEdGraphPin* FromPin : FromPins) 
		{
			// Each pin needs to branch.
			for (int32 b = 0; b < Branches; ++b)
			{
				// Add a new node.
				USMGraphNode_StateNode* StateNode = TestHelpers::CreateNewNode<USMGraphNode_StateNode>(Test, StateMachineGraph, FromPin);
				TestNodeHasGuid(Test, StateNode->FindRuntimeNode());

				SetNodeClass(Test, StateNode, StateClass);

				// Create logic we can execute on a context.
				AddStateEntryLogic(Test, StateNode);
				AddStateUpdateLogic(Test, StateNode);
				AddStateEndLogic(Test, StateNode);

				// Make sure a transition can exist.
				if (FromPin->GetOwningNode()->IsA<USMGraphNode_StateNodeBase>())
				{
					// Make the transition able to be taken.
					USMGraphNode_TransitionEdge* Transition = CastChecked<USMGraphNode_TransitionEdge>(StateNode->GetInputPin()->LinkedTo[0]->GetOwningNode());
					TestNodeHasGuid(Test, Transition->FindRuntimeNode());

					SetNodeClass(Test, Transition, TransitionClass);

					AddTransitionResultLogic(Test, Transition);
				}

				// For wiring the next state.
				NextFromPins.Add(StateNode->GetOutputPin());
			}
		}

		FromPins = NextFromPins;
	}

	// Most recent outgoing pin.
	if (FromPinsInOut)
	{
		*FromPinsInOut = FromPins;
	}
}

USMGraphNode_StateMachineStateNode* TestHelpers::BuildNestedStateMachine(FAutomationTestBase* Test,
                                                                         USMGraph* StateMachineGraph, int32 NumStates, UEdGraphPin** FromPinInOut, UEdGraphPin** NestedPinOut)
{
	UEdGraphPin* FromPin = FromPinInOut && *FromPinInOut ? *FromPinInOut : StateMachineGraph->GetEntryNode()->GetOutputPin();
	
	USMGraphNode_StateMachineStateNode* NestedStateMachineNode = CreateNewNode<USMGraphNode_StateMachineStateNode>(Test, StateMachineGraph, FromPin);

	UEdGraphPin* LastNestedPin = nullptr;
	BuildLinearStateMachine(Test, Cast<USMGraph>(NestedStateMachineNode->GetBoundGraph()), 1, &LastNestedPin);

	// Add logic to the state machine transition.
	if (USMGraphNode_TransitionEdge* TransitionToNestedStateMachine = Cast<USMGraphNode_TransitionEdge>(NestedStateMachineNode->GetInputPin()->LinkedTo[0]->GetOwningNode()))
	{
		AddTransitionResultLogic(Test, TransitionToNestedStateMachine);
	}
	
	if (NumStates > 1)
	{
		// Add to the nested state machine
		BuildLinearStateMachine(Test, Cast<USMGraph>(NestedStateMachineNode->GetBoundGraph()), NumStates - 1, &LastNestedPin);
	}

	// Most recent outgoing pin.
	if (FromPinInOut)
	{
		*FromPinInOut = FromPin;
	}

	if(NestedPinOut)
	{
		*NestedPinOut = LastNestedPin;
	}
	
	return NestedStateMachineNode;
}

USMInstance* TestHelpers::TestLinearStateMachine(FAutomationTestBase* Test, USMBlueprint* Blueprint, int32 NumStates, bool bShutdownStateMachine)
{
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	// Create a context we will run the state machine for.
	USMTestContext* Context = NewObject<USMTestContext>();
	USMInstance* StateMachineInstance = CreateNewStateMachineInstanceFromBP(Test, Blueprint, Context);

	// Verify machine matches expected states.
	Test->TestEqual("Testing states of instance match expected", StateMachineInstance->GetRootStateMachine().GetStates().Num(), NumStates);
	Test->TestEqual("Testing transitions of instance match expected", StateMachineInstance->GetRootStateMachine().GetTransitions().Num(), NumStates - 1);

	// Start the machine.
	int32 CurrentInt = 0;
	FSMState_Base* CurrentState = StateMachineInstance->GetRootStateMachine().GetSingleActiveState();
	Test->TestNull("Current state shouldn't be set", CurrentState);

	FGuid CurrentGuid;
	Test->TestEqual("Context int should be unchanged", Context->GetEntryInt(), CurrentInt);
	StateMachineInstance->Start();
	
	for(int32 i = 0; i < NumStates; ++i)
	{
		const float DeltaTime = 1.f;
		const bool bIsEndState = i == NumStates - 1;
		
		// State should have changed.
		Test->TestNotEqual("Test current state", StateMachineInstance->GetRootStateMachine().GetSingleActiveState()->GetGuid(), CurrentGuid);
		Test->TestEqual("Context int should have incremented", Context->GetEntryInt(), ++CurrentInt);
		CurrentState = StateMachineInstance->GetRootStateMachine().GetSingleActiveState();
		CurrentGuid = CurrentState->GetGuid();

		USMNodeInstance* NodeInstance = CurrentState->GetNodeInstance();
		Test->TestNotNull("Node instance created", NodeInstance);

		// Test that the state won't change if it can't transition.
		Context->bCanTransition = false;
		Context->TestUpdateInt = 0;
		StateMachineInstance->Update(DeltaTime);

		Test->TestEqual("Test current state", StateMachineInstance->GetRootStateMachine().GetSingleActiveState()->GetGuid(), CurrentGuid);
		Test->TestEqual("Context int should be unchanged", Context->GetEntryInt(), CurrentInt);
		Test->TestEqual("Context update int should have increased", Context->GetUpdateInt(), (int32)DeltaTime);

		// Is end state.
		if (bIsEndState)
		{
			Test->TestTrue("State should be end state", CurrentState->IsEndState());
			Test->TestTrue("State Machine should be in end state", StateMachineInstance->GetRootStateMachine().IsInEndState());
			break;
		}

		// Test taking the transition.
		Context->bCanTransition = true;

		Context->TestEndInt = 0;
		StateMachineInstance->Update(DeltaTime);
		Test->TestEqual("Context end int should have changed", Context->GetEndInt(), 1);
	}

	if (bShutdownStateMachine)
	{
		if (StateMachineInstance->IsActive())
		{
			StateMachineInstance->Stop();
		}

		StateMachineInstance->Shutdown();
		Test->TestFalse("State Machine should have stopped", StateMachineInstance->IsActive());
	}

	return StateMachineInstance;
}

USMInstance* TestHelpers::RunStateMachineToCompletion(FAutomationTestBase* Test, USMBlueprint* Blueprint,
	int32& LogicEntryValueOut, int32& LogicUpdateValueOut, int32& LogicEndValueOut, int32 MaxIterations, bool bShutdownStateMachine, bool bTestCompletion, bool bCompile, int32* IterationsRan)
{
	if (bCompile)
	{
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	}
	
	// Create a context we will run the state machine for.
	USMTestContext* Context = NewObject<USMTestContext>();
	USMInstance* StateMachineInstance = CreateNewStateMachineInstanceFromBP(Test, Blueprint, Context);

	StateMachineInstance->Start();
	Test->TestTrue("State Machine should have started", StateMachineInstance->IsActive());

	// Run until an end state is reached or the max iterations is hit.
	const float DeltaTime = 1.f;
	int32 CurrentIterations = 0;
	while(!StateMachineInstance->GetRootStateMachine().IsInEndState() && CurrentIterations < MaxIterations)
	{
		StateMachineInstance->Update(DeltaTime);

		if(CurrentIterations++ > MaxIterations)
		{
			break;
		}
	}

	if (bTestCompletion)
	{
		Test->TestTrue("State Machine reached end state", StateMachineInstance->GetRootStateMachine().IsInEndState());
	}
	
	if (bShutdownStateMachine)
	{
		// Will trigger last end state call.
		StateMachineInstance->Shutdown();
		Test->TestFalse("State Machine should have stopped", StateMachineInstance->IsActive());
	}

	LogicEntryValueOut = Context->GetEntryInt();
	LogicUpdateValueOut = Context->GetUpdateInt();
	LogicEndValueOut = Context->GetEndInt();

	if (IterationsRan)
	{
		*IterationsRan = CurrentIterations;
	}

	return StateMachineInstance;
}

bool ShouldAbortStateMachineRun(int32 AbortAfterStatesHit, int32 StatesHit)
{
	return AbortAfterStatesHit >= 0 && StatesHit >= AbortAfterStatesHit;
}

int32 TestHelpers::RunAllStateMachinesToCompletion(FAutomationTestBase* Test, USMInstance* Instance,
	FSMStateMachine* StateMachine, int32 AbortAfterStatesHit, int32 CheckStatesHit, bool bBindEvents)
{
	const float DeltaTime = 1.f;
	int32 StatesHit = 0;
	TArray<FSMState_Base*> StatesRemaining = StateMachine->GetStates();

	if (bBindEvents)
	{
		Instance->OnStateMachineTransitionTakenEvent.AddUniqueDynamic(Cast<USMTestContext>(Instance->GetContext()), &USMTestContext::OnTransitionTaken);
		Instance->OnStateMachineStateChangedEvent.AddUniqueDynamic(Cast<USMTestContext>(Instance->GetContext()), &USMTestContext::OnStateChanged);
	}
	
	if(!Instance->HasStarted())
	{
		Instance->Start();
		Test->TestTrue("State Machine should have started", Instance->IsActive());
	}
	
	while(!StateMachine->IsInEndState())
	{
		if(ShouldAbortStateMachineRun(AbortAfterStatesHit, StatesHit))
		{
			return StatesHit;
		}
		
		StatesHit++;
		
		FSMState_Base* ActiveState = StateMachine->GetSingleActiveState();
		FSMState_Base* NestedActiveState = Instance->GetSingleNestedActiveState();

		// Test retrieving basic state information.
		FSMStateInfo Info;
		bool bSuccess;
		Instance->TryGetStateInfo(NestedActiveState->GetGuid(), Info, bSuccess);
		Test->TestTrue("", bSuccess);
		Test->TestEqual("", Info.Guid, NestedActiveState->GetGuid());

		// Test retrieving basic transition information.
		for(FSMTransitionInfo& TransitionInfo : Info.OutgoingTransitions)
		{
			FSMTransitionInfo FoundTransition;
			Instance->TryGetTransitionInfo(TransitionInfo.Guid, FoundTransition, bSuccess);
			Test->TestTrue("", bSuccess);
			Test->TestEqual("", TransitionInfo.Guid, FoundTransition.Guid);
		}
		
		Instance->TryGetNestedActiveState(Info, bSuccess);
		Test->TestTrue("", bSuccess);
		Test->TestEqual("", Info.Guid, NestedActiveState->GetGuid());
		
		StatesRemaining.Remove(ActiveState);
		if (ActiveState->IsStateMachine())
		{
			FSMStateMachine* NestedStateMachine = (FSMStateMachine*)ActiveState;
			StatesHit += RunAllStateMachinesToCompletion(Test, Instance, NestedStateMachine, AbortAfterStatesHit < 0 ? AbortAfterStatesHit : (AbortAfterStatesHit - StatesHit), CheckStatesHit, false);
		}
		else
		{
			// Validate retrieval of exact active state from instance level. Only valid to check here. State machines may have multiple nested.
			
			Test->TestEqual("Nested active state should match active state", ActiveState, NestedActiveState);
		}
		
		Instance->Update(DeltaTime);

		// Won't get hit otherwise.
		if(StateMachine->IsInEndState())
		{
			StatesRemaining.Remove(StateMachine->GetSingleActiveState());
		}
	}

	if (CheckStatesHit >= 0)
	{
		Test->TestEqual("All states should have been hit.", StatesRemaining.Num(), CheckStatesHit);
	}

	if (bBindEvents)
	{
		USMTestContext* Context = Cast<USMTestContext>(Instance->GetContext());
		Test->TestTrue("Transition events hit", Context->TestTransitionsHit > 0);
		Test->TestTrue("State change events hit", Context->TestStatesHit > 0);
		
		Instance->OnStateMachineTransitionTakenEvent.RemoveDynamic(Cast<USMTestContext>(Instance->GetContext()), &USMTestContext::OnTransitionTaken);
		Instance->OnStateMachineStateChangedEvent.RemoveDynamic(Cast<USMTestContext>(Instance->GetContext()), &USMTestContext::OnStateChanged);
	}
	
	return StatesHit;
}

void TestHelpers::AddStateEntryLogic(FAutomationTestBase* Test, USMGraphNode_StateNode* StateNode)
{
	UEdGraph* Graph = StateNode->GetBoundGraph();
	const UEdGraphSchema_K2* GraphSchema = CastChecked<UEdGraphSchema_K2>(Graph->GetSchema());
	USMGraphK2Node_StateEntryNode* Entry = CastChecked<USMStateGraph>(Graph)->EntryNode;

	// If this is already connected to a node, such as an instance node.
	UEdGraphPin* OldEntryToPin = nullptr;
	if (Entry->Pins.Num() > 0 && Entry->Pins[0]->LinkedTo.Num() > 0)
	{
		OldEntryToPin = Entry->Pins[0]->LinkedTo[0];
	}
	
	// Add a get context node.
	UEdGraphPin* ContextOutPin = nullptr;
	UK2Node_CallFunction* GetContextNode = CreateContextGetter(Test, Graph, &ContextOutPin);

	// Add a call to execute logic on the context.
	UK2Node_CallFunction* ExecuteNode = CreateFunctionCall(Graph, USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseEntryInt)));

	// The logic self pin (make this function a method).
	UEdGraphPin* LogicSelfPin = ExecuteNode->FindPin(TEXT("self"), EGPD_Input);
	Test->TestNotNull("Expected to find ExecuteTargetPin", LogicSelfPin);

	// Convert the context 'object' type out to our context type.
	UK2Node_DynamicCast* CastNode = CreateAndLinkPureCastNode(Test, Graph, ContextOutPin, LogicSelfPin);

	// Now connect entry exec out pin to the logic exec in pin.
	Test->TestTrue("Tried to make connection from entry node to logic execute node", GraphSchema->TryCreateConnection(Entry->GetOutputPin(), ExecuteNode->GetExecPin()));

	// Wire any old logic after our function pin.
	if (OldEntryToPin)
	{
		OldEntryToPin->BreakAllPinLinks();
		Test->TestTrue("Tried to make connection from execute node to existing logic node", GraphSchema->TryCreateConnection(ExecuteNode->GetThenPin(), OldEntryToPin));
	}
}

void TestHelpers::AddStateUpdateLogic(FAutomationTestBase* Test, USMGraphNode_StateNode* StateNode)
{
	UEdGraph* Graph = StateNode->GetBoundGraph();
	const UEdGraphSchema_K2* GraphSchema = CastChecked<UEdGraphSchema_K2>(Graph->GetSchema());

	TArray<USMGraphK2Node_StateUpdateNode*> UpdateNodes;
	Graph->GetNodesOfClass<USMGraphK2Node_StateUpdateNode>(UpdateNodes);

	check(UpdateNodes.Num() == 1);

	USMGraphK2Node_StateUpdateNode* UpdateNode = UpdateNodes[0];

	// If this is already connected to a node, such as an instance node.
	UEdGraphPin* OldEntryToPin = nullptr;
	if (UpdateNode->Pins.Num() > 0 && UpdateNode->Pins[0]->LinkedTo.Num() > 0)
	{
		OldEntryToPin = UpdateNode->Pins[0]->LinkedTo[0];
	}
	
	// Find the update node's float output.
	UEdGraphPin** FloatOutputPin = UpdateNode->Pins.FindByPredicate([&](UEdGraphPin* Pin)
	{
		return Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Float;
	});
	check(FloatOutputPin);

	// Add a get context node.
	UEdGraphPin* ContextOutPin = nullptr;
	UK2Node_CallFunction* GetContextNode = CreateContextGetter(Test, Graph, &ContextOutPin);

	// Add a call to execute logic on the context.
	UK2Node_CallFunction* ExecuteNode = CreateFunctionCall(Graph, USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseUpdateInt)));

	// Find the float input pin of the execute node.
	UEdGraphPin** FloatInputPin = ExecuteNode->Pins.FindByPredicate([&](UEdGraphPin* Pin)
	{
		return Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Float;
	});
	check(FloatInputPin);

	Test->TestTrue("Tried to make float connection from update node to logic execute node", GraphSchema->TryCreateConnection(*FloatOutputPin, *FloatInputPin));

	// The logic self pin (make this function a method).
	UEdGraphPin* LogicSelfPin = ExecuteNode->FindPin(TEXT("self"), EGPD_Input);
	Test->TestNotNull("Expected to find ExecuteTargetPin", LogicSelfPin);

	// Convert the context 'object' type out to our context type.
	UK2Node_DynamicCast* CastNode = CreateAndLinkPureCastNode(Test, Graph, ContextOutPin, LogicSelfPin);

	// Now connect update exec out pin to the logic exec in pin.
	Test->TestTrue("Tried to make connection from update node to logic execute node", GraphSchema->TryCreateConnection(UpdateNode->GetOutputPin(), ExecuteNode->GetExecPin()));

	// Wire any old logic after our function pin.
	if (OldEntryToPin)
	{
		OldEntryToPin->BreakAllPinLinks();
		Test->TestTrue("Tried to make connection from execute node to existing logic node", GraphSchema->TryCreateConnection(ExecuteNode->GetThenPin(), OldEntryToPin));
	}
}

void TestHelpers::AddStateEndLogic(FAutomationTestBase* Test, USMGraphNode_StateNode* StateNode)
{
	UEdGraph* Graph = StateNode->GetBoundGraph();
	const UEdGraphSchema_K2* GraphSchema = CastChecked<UEdGraphSchema_K2>(Graph->GetSchema());

	TArray<USMGraphK2Node_StateEndNode*> EndNodes;
	Graph->GetNodesOfClass<USMGraphK2Node_StateEndNode>(EndNodes);

	check(EndNodes.Num() == 1);

	USMGraphK2Node_StateEndNode* EndNode = EndNodes[0];

	// If this is already connected to a node, such as an instance node.
	UEdGraphPin* OldEntryToPin = nullptr;
	if (EndNode->Pins.Num() > 0 && EndNode->Pins[0]->LinkedTo.Num() > 0)
	{
		OldEntryToPin = EndNode->Pins[0]->LinkedTo[0];
	}
	
	// Add a get context node.
	UEdGraphPin* ContextOutPin = nullptr;
	UK2Node_CallFunction* GetContextNode = CreateContextGetter(Test, Graph, &ContextOutPin);

	// Add a call to execute logic on the context.
	UK2Node_CallFunction* ExecuteNode = CreateFunctionCall(Graph, USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, IncreaseEndInt)));

	// The logic self pin (make this function a method).
	UEdGraphPin* LogicSelfPin = ExecuteNode->FindPin(TEXT("self"), EGPD_Input);
	Test->TestNotNull("Expected to find ExecuteTargetPin", LogicSelfPin);

	// Convert the context 'object' type out to our context type.
	UK2Node_DynamicCast* CastNode = CreateAndLinkPureCastNode(Test, Graph, ContextOutPin, LogicSelfPin);

	// Now connect end exec out pin to the logic exec in pin.
	Test->TestTrue("Tried to make connection from end node to logic execute node", GraphSchema->TryCreateConnection(EndNode->GetOutputPin(), ExecuteNode->GetExecPin()));

	// Wire any old logic after our function pin.
	if (OldEntryToPin)
	{
		OldEntryToPin->BreakAllPinLinks();
		Test->TestTrue("Tried to make connection from execute node to existing logic node", GraphSchema->TryCreateConnection(ExecuteNode->GetThenPin(), OldEntryToPin));
	}
}

void TestHelpers::AddTransitionResultLogic(FAutomationTestBase* Test, USMGraphNode_TransitionEdge* TransitionEdge)
{
	UEdGraph* Graph = TransitionEdge->GetBoundGraph();
	const UEdGraphSchema_K2* GraphSchema = CastChecked<UEdGraphSchema_K2>(Graph->GetSchema());
	USMGraphK2Node_TransitionResultNode* Result = CastChecked<USMTransitionGraph>(Graph)->ResultNode;

	// Could be connected to an instance node.
	Result->BreakAllNodeLinks();

	// Verify can't transition.
	Test->TestFalse("Transition should read as not possible to transition without any result logic", TransitionEdge->PossibleToTransition());

	// Add a get context node.
	UEdGraphPin* ContextOutPin = nullptr;
	UK2Node_CallFunction* GetContextNode = CreateContextGetter(Test, Graph, &ContextOutPin);

	// Add a call to read from the context.
	UK2Node_CallFunction* CanTransitionGetter = CreateFunctionCall(Graph, USMTestContext::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMTestContext, CanTransition)));

	// The logic self pin (make this function a method).
	UEdGraphPin* GetterPin = CanTransitionGetter->FindPin(TEXT("self"), EGPD_Input);
	Test->TestNotNull("Expected to find GetterPin", GetterPin);

	// Convert the context 'object' type out to our context type and wire it to the getter.
	UK2Node_DynamicCast* CastNode = CreateAndLinkPureCastNode(Test, Graph, ContextOutPin, GetterPin);

	UEdGraphPin** FoundPin = CanTransitionGetter->Pins.FindByPredicate([&](UEdGraphPin* Pin)
	{
		return Pin->Direction == EGPD_Output;
	});
	Test->TestNotNull("Expected to find Getter out pin", FoundPin);

	// Now connect the getter out pin to the result in pin.
	Test->TestTrue("Tried to make connection from getter node to result node", GraphSchema->TryCreateConnection(*FoundPin, Result->GetInputPin()));

	// Now verify it is possible to transition.
	Test->TestTrue("Transition should read as possible to transition", TransitionEdge->PossibleToTransition());
}

void TestHelpers::TestSetTemplate(FAutomationTestBase* Test, USMInstance* Template, const FString& DefaultStringValue, const FString& NewStringValue)
{
	bool bStringDefaultValueVerified = false;
	for (TFieldIterator<FStrProperty> It(Template->GetClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FString* DestinationPtr = (*It)->ContainerPtrToValuePtr<FString>(Template);
		Test->TestEqual("Default string value matches template", *DestinationPtr, DefaultStringValue);

		DestinationPtr->Empty();
		DestinationPtr->Append(NewStringValue);
		bStringDefaultValueVerified = true;
	}
	Test->TestTrue("Template has string property created.", bStringDefaultValueVerified);
}

void TestHelpers::SetNodeClass(FAutomationTestBase* Test, USMGraphNode_Base* Node, TSubclassOf<USMNodeInstance> Class)
{
	Node->SetNodeClass(Class);
}

UEdGraphPin* TestHelpers::GetValidCastPin(UK2Node_DynamicCast * CastNode)
{
	UEdGraphPin* Pin = CastNode->FindPin(UEdGraphSchema_K2::PN_CastSucceeded);
	check(Pin == nullptr || (Pin->Direction == EGPD_Output));
	return Pin;
}

UEdGraphPin* TestHelpers::GetCastResultPin(UK2Node_DynamicCast* CastNode)
{
	for (int32 PinIdx = 0; PinIdx < CastNode->Pins.Num(); PinIdx++)
	{
		if (CastNode->Pins[PinIdx]->Direction == EGPD_Output
			&& CastNode->Pins[PinIdx]->PinName.ToString().StartsWith(UEdGraphSchema_K2::PN_CastedValuePrefix))
		{
			return CastNode->Pins[PinIdx];
		}
	}

	return nullptr;
}

UEdGraphPin* TestHelpers::GetCastSourcePin(UK2Node_DynamicCast* CastNode)
{
	UEdGraphPin* Pin = CastNode->FindPin(UEdGraphSchema_K2::PN_ObjectToCast);
	check(Pin);
	check(Pin->Direction == EGPD_Input);
	return Pin;
}