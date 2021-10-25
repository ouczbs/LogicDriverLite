// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#include "Blueprints/SMBlueprint.h"
#include "SMTestHelpers.h"
#include "SMBlueprintFactory.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "SMTestContext.h"
#include "EdGraph/EdGraph.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Graph/SMGraph.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/Helpers/SMGraphK2Node_StateReadNodes.h"


#if WITH_DEV_AUTOMATION_TESTS

#if PLATFORM_DESKTOP

/**
 * Collapse states down to a nested state machine.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollapseStateMachineTest, "SMTests.CollapseStateMachine", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

	bool FCollapseStateMachineTest::RunTest(const FString& Parameters)
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
	int32 TotalStates = 5;

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);
	if (!NewAsset.SaveAsset(this))
	{
		return false;
	}
	TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);

	// Let the last node on the graph be the node after the new state machine.
	USMGraphNode_StateNodeBase* AfterNode = CastChecked<USMGraphNode_StateNodeBase>(LastStatePin->GetOwningNode());

	// Let the second node from the beginning be the node leading to the new state machine.
	USMGraphNode_StateNodeBase* BeforeNode = AfterNode->GetPreviousNode()->GetPreviousNode()->GetPreviousNode();
	check(BeforeNode);

	// The two states in between will become a state machine.
	TSet<UObject*> SelectedNodes;
	USMGraphNode_StateNodeBase* SMStartNode = BeforeNode->GetNextNode();
	USMGraphNode_StateNodeBase* SMEndNode = SMStartNode->GetNextNode();
	SelectedNodes.Add(SMStartNode);
	SelectedNodes.Add(SMEndNode);

	TestEqual("Start SM Node connects from before node", BeforeNode, SMStartNode->GetPreviousNode());
	TestEqual("Before Node connects to start SM node", SMStartNode, BeforeNode->GetNextNode());

	TestEqual("End SM Node connects from after node", AfterNode, SMEndNode->GetNextNode());
	TestEqual("After Node connects to end SM node", SMEndNode, AfterNode->GetPreviousNode());

	FSMBlueprintEditorUtils::CollapseNodesAndCreateStateMachine(SelectedNodes);

	TotalStates -= 1;

	TestNotEqual("Start SM Node no longer connects to before node", BeforeNode, SMStartNode->GetPreviousNode());
	TestNotEqual("Before Node no longer connects to start SM node", SMStartNode, BeforeNode->GetNextNode());

	TestNotEqual("End SM Node no longer connects from after node", AfterNode, SMEndNode->GetNextNode());
	TestNotEqual("After Node no longer connects to end SM node", SMEndNode, AfterNode->GetPreviousNode());

	USMGraphNode_StateMachineStateNode* NewSMNode = Cast<USMGraphNode_StateMachineStateNode>(BeforeNode->GetNextNode());
	TestNotNull("State Machine node created in proper location", NewSMNode);

	if (NewSMNode == nullptr)
	{
		return false;
	}

	TestEqual("New SM Node connects to correct node", NewSMNode->GetNextNode(), AfterNode);

	TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);

	return NewAsset.DeleteAsset(this);
}

/**
 * Assemble a hierarchical state machine and convert the nested state machine to a reference, then run and wait for the referenced state machine to finish.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceStateMachineTest, "SMTests.ReferenceStateMachine", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

	bool FReferenceStateMachineTest::RunTest(const FString& Parameters)
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
	int32 TotalStates = 0;
	int32 TotalTopLevelStates = 0;
	UEdGraphPin* LastStatePin = nullptr;

	// Build top level state machine.
	{
		const int32 CurrentStates = 2;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, CurrentStates, &LastStatePin);
		if (!NewAsset.SaveAsset(this))
		{
			return false;
		}
		TotalStates += CurrentStates;
		TotalTopLevelStates += CurrentStates;
	}

	// Build a nested state machine.
	UEdGraphPin* EntryPointForNestedStateMachine = LastStatePin;
	USMGraphNode_StateMachineStateNode* NestedStateMachineNode = TestHelpers::CreateNewNode<USMGraphNode_StateMachineStateNode>(this, StateMachineGraph, EntryPointForNestedStateMachine);

	UEdGraphPin* LastNestedPin = nullptr;
	{
		const int32 CurrentStates = 10;
		TestHelpers::BuildLinearStateMachine(this, Cast<USMGraph>(NestedStateMachineNode->GetBoundGraph()), CurrentStates, &LastNestedPin, USMStateTestInstance::StaticClass(), USMTransitionTestInstance::StaticClass());
		NestedStateMachineNode->GetBoundGraph()->Rename(TEXT("Nested_State_Machine_For_Reference"));
		LastStatePin = NestedStateMachineNode->GetOutputPin();

		TotalStates += CurrentStates;
		TotalTopLevelStates += 1;
	}

	// Add logic to the state machine transition.
	USMGraphNode_TransitionEdge* TransitionToNestedStateMachine = CastChecked<USMGraphNode_TransitionEdge>(NestedStateMachineNode->GetInputPin()->LinkedTo[0]->GetOwningNode());
	TestHelpers::AddTransitionResultLogic(this, TransitionToNestedStateMachine);

	// Add more top level.
	{
		const int32 CurrentStates = 10;
		TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, CurrentStates, &LastStatePin);
		if (!NewAsset.SaveAsset(this))
		{
			return false;
		}
		TotalStates += CurrentStates;
		TotalTopLevelStates += CurrentStates;
	}

	TestTrue("Nested state machine has correct node count", NestedStateMachineNode->GetBoundGraph()->Nodes.Num() > 1);

	// Now convert the state machine to a reference.
	USMBlueprint* NewReferencedBlueprint = FSMBlueprintEditorUtils::ConvertStateMachineToReference(NestedStateMachineNode, false, nullptr, nullptr);
	TestNotNull("New referenced blueprint created", NewReferencedBlueprint);
	TestTrue("Nested state machine has had all nodes removed.", NestedStateMachineNode->GetBoundGraph()->Nodes.Num() == 1);

	FKismetEditorUtilities::CompileBlueprint(NewReferencedBlueprint);

	// Store handler information so we can delete the object.
	FString ReferencedPath = NewReferencedBlueprint->GetPathName();
	FAssetHandler ReferencedAsset(NewReferencedBlueprint->GetName(), USMBlueprint::StaticClass(), NewObject<USMBlueprintFactory>(), &ReferencedPath);
	ReferencedAsset.Object = NewReferencedBlueprint;

	UPackage* Package = FAssetData(NewReferencedBlueprint).GetPackage();
	ReferencedAsset.Package = Package;

	// This will run the nested machine only up to the first state.
	TestHelpers::TestLinearStateMachine(this, NewBP, TotalTopLevelStates);

	int32 ExpectedEntryValue = TotalTopLevelStates;

	// Run the same machine until an end state is reached. The result should be the same as the top level machine won't wait for the nested machine.
	{
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);

		TestEqual("State Machine generated value", EntryHits, ExpectedEntryValue);
		TestEqual("State Machine generated value", UpdateHits, 0);
		TestEqual("State Machine generated value", EndHits, ExpectedEntryValue);
	}

	// Now let's try waiting for the nested state machine. Clear the graph except for the result node.
	{
		USMGraphNode_TransitionEdge* TransitionFromNestedStateMachine = CastChecked<USMGraphNode_TransitionEdge>(NestedStateMachineNode->GetOutputPin()->LinkedTo[0]->GetOwningNode());
		UEdGraph* TransitionGraph = TransitionFromNestedStateMachine->GetBoundGraph();
		TransitionGraph->Nodes.Empty();
		TransitionGraph->GetSchema()->CreateDefaultNodesForGraph(*TransitionGraph);

		TestHelpers::AddSpecialBooleanTransitionLogic<USMGraphK2Node_StateMachineReadNode_InEndState>(this, TransitionFromNestedStateMachine);
		ExpectedEntryValue = TotalStates;

		// Run the same machine until an end state is reached. This time the result should be modified by all nested states.
		int32 EntryHits = 0; int32 UpdateHits = 0; int32 EndHits = 0;
		TestHelpers::RunStateMachineToCompletion(this, NewBP, EntryHits, UpdateHits, EndHits);

		TestEqual("State Machine generated value", EntryHits, ExpectedEntryValue);
		TestEqual("State Machine generated value", UpdateHits, 0);
		TestEqual("State Machine generated value", EndHits, ExpectedEntryValue);
	}

	// Verify it can't reference itself.
	bool bReferenceSelf = NestedStateMachineNode->ReferenceStateMachine(NewBP, true);
	TestFalse("State Machine should not have been allowed to reference itself", bReferenceSelf);

	// Finally let's check circular references and make sure it doesn't stack overflow.
	bReferenceSelf = NestedStateMachineNode->ReferenceStateMachine(NewBP, false);
	TestTrue("State Machine has been overridden to reference itself", bReferenceSelf);

	FKismetEditorUtilities::CompileBlueprint(NewBP);

	// As long as generating the state machine is successful we are fine. Running it would cause a stack overflow because we have no exit conditions.
	// That doesn't need to be tested as that is up to user implementation.
	USMTestContext* Context = NewObject<USMTestContext>();

	AddExpectedError(FString("Attempted to generate state machine with circular referencing"));
	USMInstance* ReferencesNoReuse = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context, false); // Don't test node map that will stack overflow.
	const int32 NoReuse = ReferencesNoReuse->GetAllReferencedInstances(true).Num();
	
	// Now check legacy behavior if the same reference is reused.
	NestedStateMachineNode->bUseTemplate = false;
	NestedStateMachineNode->bReuseReference = true;

	FKismetEditorUtilities::CompileBlueprint(NewBP);

	/*
	if (PLATFORM_WINDOWS) // Linux may stack overflow here. Really this should be investigated more but this is around unsupported behavior as it is.
	{
		USMInstance* ReferencesWithReuse = TestHelpers::CreateNewStateMachineInstanceFromBP(this, NewBP, Context, false);
		// Didn't crash? Great!

		const int32 WithReuse = ReferencesWithReuse->GetAllReferencedInstances(true).Num();
		// We expect more references because with circular referencing and instancing state machine generation aborts with an error message.
		// When reusing instances this gets around that behavior.
		// ------------
		// Changed with guid paths... Specifically CalculatePathGuid under FSMStateMachine helps prevent stack overflow.
		TestTrue("References reused", WithReuse < NoReuse);
	}
	*/
	ReferencedAsset.DeleteAsset(this);
	return NewAsset.DeleteAsset(this);
}

/**
 * Replace a node in the state machine.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReplaceNodesTest, "SMTests.ReplaceNodes", EAutomationTestFlags::ApplicationContextMask |
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

	bool FReplaceNodesTest::RunTest(const FString& Parameters)
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
	int32 TotalStates = 5;

	UEdGraphPin* LastStatePin = nullptr;

	TestHelpers::BuildLinearStateMachine(this, StateMachineGraph, TotalStates, &LastStatePin);
	if (!NewAsset.SaveAsset(this))
	{
		return false;
	}
	TestHelpers::TestLinearStateMachine(this, NewBP, TotalStates);

	// Let the last node on the graph be the node after the new node.
	USMGraphNode_StateNodeBase* AfterNode = CastChecked<USMGraphNode_StateNodeBase>(LastStatePin->GetOwningNode());

	// Let node prior to the one we are replacing.
	USMGraphNode_StateNodeBase* BeforeNode = AfterNode->GetPreviousNode()->GetPreviousNode();
	check(BeforeNode);

	// The node we are replacing is the second to last node.
	USMGraphNode_StateNodeBase* NodeToReplace = AfterNode->GetPreviousNode();
	TestTrue("Node is state", NodeToReplace->IsA<USMGraphNode_StateNode>());

	// State machine -- can't easily test converting to reference but that is just setting a null reference.
	USMGraphNode_StateMachineStateNode* StateMachineNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_StateMachineStateNode>(NodeToReplace);
	TestTrue("Node removed", NodeToReplace->GetNextNode() == nullptr && NodeToReplace->GetPreviousNode() == nullptr && NodeToReplace->GetBoundGraph() == nullptr);
	TestTrue("Node is state machine", StateMachineNode->IsA<USMGraphNode_StateMachineStateNode>());
	TestFalse("Node is not reference", StateMachineNode->IsStateMachineReference());
	TestEqual("Connected to original next node", StateMachineNode->GetNextNode(), AfterNode);
	TestEqual("Connected to original previous node", StateMachineNode->GetPreviousNode(), BeforeNode);

	int32 A, B;
	TestHelpers::RunStateMachineToCompletion(this, NewBP, TotalStates, A, B);
	
	// Conduit
	NodeToReplace = StateMachineNode;
	USMGraphNode_ConduitNode* ConduitNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_ConduitNode>(NodeToReplace);
	TestTrue("Node removed", NodeToReplace->GetNextNode() == nullptr && NodeToReplace->GetPreviousNode() == nullptr && NodeToReplace->GetBoundGraph() == nullptr);
	TestTrue("Node is conduit", ConduitNode->IsA<USMGraphNode_ConduitNode>());
	TestEqual("Connected to original next node", ConduitNode->GetNextNode(), AfterNode);
	TestEqual("Connected to original previous node", ConduitNode->GetPreviousNode(), BeforeNode);

	// Back to state
	NodeToReplace = ConduitNode;
	USMGraphNode_StateNode* StateNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_StateNode>(NodeToReplace);
	TestTrue("Node removed", NodeToReplace->GetNextNode() == nullptr && NodeToReplace->GetPreviousNode() == nullptr && NodeToReplace->GetBoundGraph() == nullptr);
	TestTrue("Node is state", StateNode->IsA<USMGraphNode_StateNode>());
	TestEqual("Connected to original next node", StateNode->GetNextNode(), AfterNode);
	TestEqual("Connected to original previous node", StateNode->GetPreviousNode(), BeforeNode);
	TestHelpers::RunStateMachineToCompletion(this, NewBP, TotalStates, A, B);

	
	return NewAsset.DeleteAsset(this);
}

#endif

#endif //WITH_DEV_AUTOMATION_TESTS