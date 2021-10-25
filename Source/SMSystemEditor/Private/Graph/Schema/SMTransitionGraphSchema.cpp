// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#include "SMTransitionGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionResultNode.h"
#include "Graph/SMTransitionGraph.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionEnteredNode.h"


#define LOCTEXT_NAMESPACE "SMTransitionGraphSchema"

USMTransitionGraphSchema::USMTransitionGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMTransitionGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	// Create the ResultNode which is also the runtime node container.
	FGraphNodeCreator<USMGraphK2Node_TransitionResultNode> NodeCreator(Graph);
	USMGraphK2Node_TransitionResultNode* ResultNode = NodeCreator.CreateNode();
	
	NodeCreator.Finalize();
	SetNodeMetaData(ResultNode, FNodeMetadata::DefaultGraphNode);

	USMTransitionGraph* TypedGraph = CastChecked<USMTransitionGraph>(&Graph);
	TypedGraph->ResultNode = ResultNode;
}

void USMTransitionGraphSchema::GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const
{
	Super::GetGraphDisplayInformation(Graph, DisplayInfo);
	DisplayInfo.PlainName = FText::FromString(Graph.GetName());

	if (const USMGraphNode_TransitionEdge* Transition = Cast<const USMGraphNode_TransitionEdge>(Graph.GetOuter()))
	{
		DisplayInfo.PlainName = FText::Format(LOCTEXT("TransitionNameGraphTitle", "{0} (transition)"), FText::FromString(Transition->GetTransitionName()));
	}

	DisplayInfo.DisplayName = DisplayInfo.PlainName;
	DisplayInfo.Tooltip = DisplayInfo.PlainName;
}

void USMTransitionGraphSchema::HandleGraphBeingDeleted(UEdGraph& GraphBeingRemoved) const
{
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(&GraphBeingRemoved))
	{
		if (USMTransitionGraph* TransitionGraph = Cast<USMTransitionGraph>(&GraphBeingRemoved))
		{
			USMGraphNode_TransitionEdge* TransitionNode = TransitionGraph->GetOwningTransitionNode();
			ensure(TransitionNode);

			// Let the node delete first-- it will trigger graph removal. Helps with undo buffer transaction.
			if (TransitionNode->GetBoundGraph() == &GraphBeingRemoved)
			{
				FBlueprintEditorUtils::RemoveNode(Blueprint, TransitionNode, true);
				return;
			}

			// Remove this graph from the parent graph.
			UEdGraph* ParentGraph = TransitionNode->GetGraph();
			ParentGraph->SubGraphs.Remove(TransitionGraph);
			ParentGraph->Modify();
		}
	}

	Super::HandleGraphBeingDeleted(GraphBeingRemoved);
}

#undef LOCTEXT_NAMESPACE
