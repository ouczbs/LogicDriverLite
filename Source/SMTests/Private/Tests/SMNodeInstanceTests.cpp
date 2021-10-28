// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#include "Blueprints/SMBlueprint.h"
#include "SMTestHelpers.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "SMTestContext.h"
#include "Factory/SMBlueprintFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Graph/SMGraph.h"
#include "Graph/SMStateGraph.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"


#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Run a state machine consisting of 100 custom state classes with custom transitions.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstancesRunStateMachineTest, "SMTests.NodeInstancesRunStateMachine", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

	bool FNodeInstancesRunStateMachineTest::RunTest(const FString& Parameters)
{
	FAssetHandler NewAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(this, NewAsset, false))
	{
		return false;
	}

	USMBlueprint* NewBP = NewAsset.GetObjectAs<USMBlueprint>();

	// Find root state machine.
	USMGraphK2Node_StateMachineNode* RootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(NewBP);

	// Find the state machine graph.
	USMGraph* StateMachineGraph = RootStateMachineNode->GetStateMachineGraph();

	// Total states to test.
	const int32 TotalStates = 100;

	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());
	TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);

	return true;
}

/**
 * Verify node instance struct wrapper methods work properly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNodeInstanceMethodsTest, "SMTests.NodeInstanceMethods", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNodeInstanceMethodsTest::RunTest(const FString& Parameters)
{
	FAssetHandler NewAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(this, NewAsset, false))
	{
		return false;
	}

	USMBlueprint* NewBP = NewAsset.GetObjectAs<USMBlueprint>();

	// Find root state machine.
	USMGraphK2Node_StateMachineNode* RootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(NewBP);

	// Find the state machine graph.
	USMGraph* StateMachineGraph = RootStateMachineNode->GetStateMachineGraph();

	// Total states to test.
	int32 TotalStates = 2;

	{
		UEdGraphPin* LastStatePin = nullptr;
		//Verify default instances load correctly.
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateInstance::StaticClass(), USMTransitionInstance::StaticClass());
		int32 A, B, C;
		TestHelpers::RunStateMachineToCompletion(this, NewBP, A, B, C);
		FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(StateMachineGraph);
	}
	
	// Load test instances.
	UEdGraphPin* LastStatePin = nullptr;
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());
	FKismetEditorUtilities::CompileBlueprint(NewBP);
	
	USMTestContext* Context = NewObject<USMTestContext>();
	USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);
	
	FSMState_Base* InitialState = StateMachineInstance->GetRootStateMachine().GetSingleInitialState();
	USMStateInstance_Base* NodeInstance = CastChecked<USMStateInstance_Base>(InitialState->GetNodeInstance());
	InitialState->bAlwaysUpdate = true; // Needed since we are manually switching states later.

	{
		// Test root and entry nodes.
		
		USMStateMachineInstance* RootStateMachineInstance = StateMachineInstance->GetRootStateMachineInstance();
		TestNotNull("Root node not null", RootStateMachineInstance);
		TestEqual("Root node discoverable", RootStateMachineInstance, Cast<USMStateMachineInstance>(StateMachineInstance->GetRootStateMachine().GetNodeInstance()));

		TArray<USMStateInstance_Base*> EntryStates;
		RootStateMachineInstance->GetEntryStates(EntryStates);
		check(EntryStates.Num() == 1);

		TestEqual("Entry states discoverable", EntryStates[0], NodeInstance);
	}
	
	TestEqual("Correct state machine", NodeInstance->GetStateMachineInstance(), StateMachineInstance);
	TestEqual("Guids correct", NodeInstance->GetGuid(), InitialState->GetGuid());
	TestEqual("Name correct", NodeInstance->GetNodeName(), InitialState->GetNodeName());
	
	TestFalse("Initial state not active", NodeInstance->IsActive());
	StateMachineInstance->Start();
	TestTrue("Initial state active", NodeInstance->IsActive());

	InitialState->TimeInState = 3;
	TestEqual("Time correct", NodeInstance->GetTimeInState(), InitialState->TimeInState);

	FSMStateInfo StateInfo;
	NodeInstance->GetStateInfo(StateInfo);

	TestEqual("State info guids correct", StateInfo.Guid, InitialState->GetGuid());
	TestEqual("State info instance correct", StateInfo.NodeInstance, Cast<USMNodeInstance>(NodeInstance));
	TestFalse("Not a state machine", NodeInstance->IsStateMachine());
	TestFalse("Not in end state", NodeInstance->IsInEndState());
	TestFalse("Has not updated", NodeInstance->HasUpdated());
	TestNull("No transition to take", NodeInstance->GetTransitionToTake());

	USMStateInstance_Base* NextState = CastChecked<USMStateInstance_Base>(InitialState->GetOutgoingTransitions()[0]->GetToState()->GetNodeInstance());

	// Test searching nodes.
	TArray<USMNodeInstance*> FoundNodes;
	NodeInstance->GetAllNodesOfType(FoundNodes, USMStateInstance::StaticClass());

	TestEqual("All nodes found", FoundNodes.Num(), TotalStates);
	TestEqual("Correct state found", FoundNodes[0], Cast<USMNodeInstance>(NodeInstance));
	TestEqual("Correct state found", FoundNodes[1], Cast<USMNodeInstance>(NextState));

	// Verify state machine instance methods to retrieve node instances are correct.
	TArray<USMStateInstance_Base*> StateInstances;
	StateMachineInstance->GetAllStateInstances(StateInstances);
	TestEqual("All states found", StateInstances.Num(), StateMachineInstance->GetStateMap().Num());
	for (USMStateInstance_Base* StateInstance : StateInstances)
	{
		USMStateInstance_Base* FoundStateInstance = StateMachineInstance->GetStateInstanceByGuid(StateInstance->GetGuid());
		TestEqual("State instance retrieved from sm instance", FoundStateInstance, StateInstance);
	}
	
	TArray<USMTransitionInstance*> TransitionInstances;
	StateMachineInstance->GetAllTransitionInstances(TransitionInstances);
	TestEqual("All transitions found", TransitionInstances.Num(), StateMachineInstance->GetTransitionMap().Num());
	for (USMTransitionInstance* TransitionInstance : TransitionInstances)
	{
		USMTransitionInstance* FoundTransitionInstance = StateMachineInstance->GetTransitionInstanceByGuid(TransitionInstance->GetGuid());
		TestEqual("Transition instance retrieved from sm instance", FoundTransitionInstance, TransitionInstance);
	}
	
	// Test transition instance.
	USMTransitionInstance* NextTransition = CastChecked<USMTransitionInstance>(InitialState->GetOutgoingTransitions()[0]->GetNodeInstance());
	{
		TArray<USMTransitionInstance*> Transitions;
		NodeInstance->GetOutgoingTransitions(Transitions);

		TestEqual("One outgoing transition", Transitions.Num(), 1);
		USMTransitionInstance* TransitionInstance = Transitions[0];

		TestEqual("Transition instance correct", TransitionInstance, NextTransition);
		
		FSMTransitionInfo TransitionInfo;
		TransitionInstance->GetTransitionInfo(TransitionInfo);

		TestEqual("Transition info instance correct", TransitionInfo.NodeInstance, Cast<USMNodeInstance>(NextTransition));

		TestEqual("Prev state correct", TransitionInstance->GetPreviousStateInstance(), NodeInstance);
		TestEqual("Next state correct", TransitionInstance->GetNextStateInstance(), NextState);
	}
	
	NodeInstance->SwitchToLinkedState(NextState);

	TestFalse("State no longer active", NodeInstance->IsActive());
	TestTrue("Node has updated from bAlwaysUpdate", NodeInstance->HasUpdated());
	TestEqual("Transition to take set", NodeInstance->GetTransitionToTake(), NextTransition);
	
	USMTransitionInstance* PreviousTransition = CastChecked<USMTransitionInstance>(((FSMState_Base*)NextState->GetOwningNode())->GetIncomingTransitions()[0]->GetNodeInstance());
	{
		TestEqual("Previous transition is correct instance", PreviousTransition, NextTransition);
		
		TArray<USMTransitionInstance*> Transitions;
		NextState->GetIncomingTransitions(Transitions);

		TestEqual("One incoming transition", Transitions.Num(), 1);
		USMTransitionInstance* TransitionInstance = Transitions[0];

		TestEqual("Transition instance correct", TransitionInstance, PreviousTransition);

		FSMTransitionInfo TransitionInfo;
		TransitionInstance->GetTransitionInfo(TransitionInfo);

		TestEqual("Transition info instance correct", TransitionInfo.NodeInstance, Cast<USMNodeInstance>(PreviousTransition));

		TestEqual("Prev state correct", TransitionInstance->GetPreviousStateInstance(), NodeInstance);
		TestEqual("Next state correct", TransitionInstance->GetNextStateInstance(), NextState);
	}

	NodeInstance = CastChecked<USMStateInstance_Base>(StateMachineInstance->GetSingleActiveState()->GetNodeInstance());
	TestTrue("Is end state", NodeInstance->IsInEndState());

	//  Test nested reference FSM can retrieve transitions.
	{
		LastStatePin = nullptr;
		FSMBlueprintEditorUtils::RemoveAllNodesFromGraph(StateMachineGraph, NewBP);
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);

		USMGraphNode_StateMachineStateNode* NestedFSM = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_StateMachineStateNode>(CastChecked<USMGraphNode_StateNodeBase>(StateMachineGraph->EntryNode->GetOutputNode()));
		FKismetEditorUtilities::CompileBlueprint(NewBP);

		USMBlueprint* NewReferencedBlueprint = FSMBlueprintEditorUtils::ConvertStateMachineToReference(NestedFSM, false, nullptr, nullptr);

		Context = NewObject<USMTestContext>();
		StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);
		USMStateMachineInstance* FSMClass = CastChecked< USMStateMachineInstance>(StateMachineInstance->GetRootStateMachine().GetSingleInitialState()->GetNodeInstance());

		TArray<USMTransitionInstance*> Transitions;
		FSMClass->GetOutgoingTransitions(Transitions);
		TestEqual("Outgoing transitions found of reference FSM", Transitions.Num(), 1);
	}
	
	return true;
}

/**
 * Test nested state machines with a state machine class set evaluate graphs properly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStateMachineClassInstanceTest, "SMTests.StateMachineClassInstance", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

	bool FStateMachineClassInstanceTest::RunTest(const FString& Parameters)
{
	FAssetHandler NewAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(this, NewAsset, false))
	{
		return false;
	}

	USMBlueprint* NewBP = NewAsset.GetObjectAs<USMBlueprint>();

	// Find root state machine.
	USMGraphK2Node_StateMachineNode* RootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(NewBP);

	// Find the state machine graph.
	USMGraph* StateMachineGraph = RootStateMachineNode->GetStateMachineGraph();

	// Total states to test.
	int32 TotalStates = 2;

	UEdGraphPin* LastStatePin = nullptr;

	// Build state machine.
	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());

	USMGraphNode_StateMachineStateNode* NestedFSMNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_StateMachineStateNode>(CastChecked<USMGraphNode_Base>(StateMachineGraph->GetEntryNode()->GetOutputNode()));
	USMGraphNode_StateMachineStateNode* NestedFSMNode2 = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_StateMachineStateNode>(NestedFSMNode->GetNextNode());

	TestHelpers::SetNodeClass(this, NestedFSMNode, USMStateMachineTestInstance::StaticClass());
	TestHelpers::SetNodeClass(this, NestedFSMNode2, USMStateMachineTestInstance::StaticClass());

	FKismetEditorUtilities::CompileBlueprint(NewBP);
	
	USMTestContext* Context = NewObject<USMTestContext>();
	USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

	FSMState_Base* InitialState = StateMachineInstance->GetRootStateMachine().GetSingleInitialState();
	USMStateMachineTestInstance* NodeInstance = CastChecked<USMStateMachineTestInstance>(InitialState->GetNodeInstance());
	InitialState->bAlwaysUpdate = true; // Needed since we are manually switching states later.

	TestEqual("Correct state machine", NodeInstance->GetStateMachineInstance(), StateMachineInstance);
	TestEqual("Guids correct", NodeInstance->GetGuid(), InitialState->GetGuid());
	TestEqual("Name correct", NodeInstance->GetNodeName(), InitialState->GetNodeName());
	
	TestFalse("Initial state not active", NodeInstance->IsActive());
	
	StateMachineInstance->Start();

	TestTrue("Initial state active", NodeInstance->IsActive());
	InitialState->TimeInState = 3;
	TestEqual("Time correct", NodeInstance->GetTimeInState(), InitialState->TimeInState);
	
	FSMStateInfo StateInfo;
	NodeInstance->GetStateInfo(StateInfo);

	TestEqual("State info guids correct", StateInfo.Guid, InitialState->GetGuid());
	TestEqual("State info instance correct", StateInfo.NodeInstance, Cast<USMNodeInstance>(NodeInstance));
	TestTrue("Is a state machine", NodeInstance->IsStateMachine());
	TestFalse("Has not updated", NodeInstance->HasUpdated());
	TestNull("No transition to take", NodeInstance->GetTransitionToTake());

	USMStateMachineTestInstance* NextState = CastChecked<USMStateMachineTestInstance>(InitialState->GetOutgoingTransitions()[0]->GetToState()->GetNodeInstance());

	// Test transition instance.
	USMTransitionInstance* NextTransition = CastChecked<USMTransitionInstance>(InitialState->GetOutgoingTransitions()[0]->GetNodeInstance());
	{
		TArray<USMTransitionInstance*> Transitions;
		NodeInstance->GetOutgoingTransitions(Transitions);

		TestEqual("One outgoing transition", Transitions.Num(), 1);
		USMTransitionInstance* TransitionInstance = Transitions[0];

		TestEqual("Transition instance correct", TransitionInstance, NextTransition);

		FSMTransitionInfo TransitionInfo;
		TransitionInstance->GetTransitionInfo(TransitionInfo);

		TestEqual("Transition info instance correct", TransitionInfo.NodeInstance, Cast<USMNodeInstance>(NextTransition));

		TestEqual("Prev state correct", Cast<USMStateMachineTestInstance>(TransitionInstance->GetPreviousStateInstance()), NodeInstance);
		TestEqual("Next state correct", Cast<USMStateMachineTestInstance>(TransitionInstance->GetNextStateInstance()), NextState);
	}
	
	StateMachineInstance->Update(0.f);

	TestFalse("State no longer active", NodeInstance->IsActive());
	TestTrue("Node has updated from bAlwaysUpdate", NodeInstance->HasUpdated());
	TestEqual("Transition to take set", NodeInstance->GetTransitionToTake(), NextTransition);

	NodeInstance = CastChecked<USMStateMachineTestInstance>(StateMachineInstance->GetSingleActiveState()->GetNodeInstance());
	TestTrue("Is end state", NodeInstance->IsInEndState());

	StateMachineInstance->Stop();
	
	return true;
}

/**
 * Test nested state machine references with a state machine class set.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStateMachineClassInstanceReferenceTest, "SMTests.StateMachineClassInstanceReference", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

	bool FStateMachineClassInstanceReferenceTest::RunTest(const FString& Parameters)
{
	FAssetHandler NewAsset;
	if (!TestHelpers::TryCreateNewStateMachineAsset(this, NewAsset, false))
	{
		return false;
	}

	USMBlueprint* NewBP = NewAsset.GetObjectAs<USMBlueprint>();

	// Find root state machine.
	USMGraphK2Node_StateMachineNode* RootStateMachineNode = FSMBlueprintEditorUtils::GetRootStateMachineNode(NewBP);

	// Find the state machine graph.
	USMGraph* StateMachineGraph = RootStateMachineNode->GetStateMachineGraph();

	UEdGraphPin* LastStatePin = nullptr;

	const int32 NestedStateCount = 1;
	
	USMGraphNode_StateMachineStateNode* NestedFSMNode = TestHelpers::BuildNestedStateMachine(this, StateMachineGraph, NestedStateCount, &LastStatePin, nullptr);
	
	UEdGraphPin* FromPin = NestedFSMNode->GetOutputPin();
	USMGraphNode_StateMachineStateNode* NestedFSMNode2 = TestHelpers::BuildNestedStateMachine(this, StateMachineGraph, NestedStateCount, &FromPin, nullptr);

	TestHelpers::SetNodeClass(this, NestedFSMNode, USMStateMachineTestInstance::StaticClass());
	TestHelpers::SetNodeClass(this, NestedFSMNode2, USMStateMachineTestInstance::StaticClass());
	TestHelpers::SetNodeClass(this, NestedFSMNode->GetNextTransition(), USMTransitionTestInstance::StaticClass());
	
	// Now convert the state machine to a reference.
	USMBlueprint* NewReferencedBlueprint = FSMBlueprintEditorUtils::ConvertStateMachineToReference(NestedFSMNode, false, nullptr, nullptr);
	TestNotNull("New referenced blueprint created", NewReferencedBlueprint);
	TestTrue("Nested state machine has had all nodes removed.", NestedFSMNode->GetBoundGraph()->Nodes.Num() == 1);

	FKismetEditorUtilities::CompileBlueprint(NewReferencedBlueprint);

	// Store handler information so we can delete the object.
	FString ReferencedPath = NewReferencedBlueprint->GetPathName();
	FAssetHandler ReferencedAsset(NewReferencedBlueprint->GetName(), USMBlueprint::StaticClass(), NewObject<USMBlueprintFactory>(), &ReferencedPath);
	ReferencedAsset.Object = NewReferencedBlueprint;

	UPackage* Package = FAssetData(NewReferencedBlueprint).GetPackage();
	ReferencedAsset.Package = Package;

	FKismetEditorUtilities::CompileBlueprint(NewBP);

	USMTestContext* Context = NewObject<USMTestContext>();
	USMInstance* StateMachineInstance = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context);

	// Locate the node instance of the reference.
	
	FSMStateMachine* InitialState = (FSMStateMachine*)StateMachineInstance->GetRootStateMachine().GetSingleInitialState();
	USMStateMachineTestInstance* NodeInstance = Cast<USMStateMachineTestInstance>(InitialState->GetNodeInstance());

	TestNotNull("Node instance from reference found", NodeInstance);

	if (!NodeInstance)
	{
		return false;
	}
	
	InitialState->bAlwaysUpdate = true; // Needed since we are manually switching states later.

	TestFalse("Initial state not active", NodeInstance->IsActive());

	StateMachineInstance->Start();

	TestTrue("Initial state active", NodeInstance->IsActive());

	FSMStateInfo StateInfo;
	NodeInstance->GetStateInfo(StateInfo);

	TestEqual("State info instance correct", StateInfo.NodeInstance, Cast<USMNodeInstance>(NodeInstance));
	TestTrue("Is a state machine", NodeInstance->IsStateMachine());
	TestFalse("Has not updated", NodeInstance->HasUpdated());
	TestNull("No transition to take", NodeInstance->GetTransitionToTake());

	USMStateMachineTestInstance* NextState = CastChecked<USMStateMachineTestInstance>(InitialState->GetOutgoingTransitions()[0]->GetToState()->GetNodeInstance());

	// Test transition instance.
	USMTransitionTestInstance* NextTransition = CastChecked<USMTransitionTestInstance>(InitialState->GetOutgoingTransitions()[0]->GetNodeInstance());
	{
		TArray<USMTransitionInstance*> Transitions;
		NodeInstance->GetOutgoingTransitions(Transitions);

		TestEqual("One outgoing transition", Transitions.Num(), 1);
		USMTransitionInstance* TransitionInstance = Transitions[0];

		TestEqual("Transition instance correct", Cast<USMTransitionTestInstance>(TransitionInstance), NextTransition);

		FSMTransitionInfo TransitionInfo;
		TransitionInstance->GetTransitionInfo(TransitionInfo);

		TestEqual("Transition info instance correct", TransitionInfo.NodeInstance, Cast<USMNodeInstance>(NextTransition));

		TestEqual("Prev state correct", Cast<USMStateMachineTestInstance>(TransitionInstance->GetPreviousStateInstance()), NodeInstance);
		TestEqual("Next state correct", Cast<USMStateMachineTestInstance>(TransitionInstance->GetNextStateInstance()), NextState);
	}

	NextTransition->bCanTransition = true;
	StateMachineInstance->Update(0.f);

	TestFalse("State no longer active", NodeInstance->IsActive());
	TestTrue("Node has updated from bAlwaysUpdate", NodeInstance->HasUpdated());
	TestEqual("Transition to take set", Cast<USMTransitionTestInstance>(NodeInstance->GetTransitionToTake()), NextTransition);

	
	// Second node instance test (Normal fsm)
	{
		USMStateMachineTestInstance* SecondNodeInstance = CastChecked<USMStateMachineTestInstance>(StateMachineInstance->GetSingleActiveState()->GetNodeInstance());
		TestTrue("Is end state", SecondNodeInstance->IsInEndState());

		StateMachineInstance->Stop();
	}
	
	ReferencedAsset.DeleteAsset(this);
	return true;
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS