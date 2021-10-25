// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#include "SMConduitGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Utilities/SMBlueprintEditorUtils.h"


USMConduitGraph::USMConduitGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), ResultNode(nullptr)
{
}

bool USMConduitGraph::HasAnyLogicConnections() const
{
	// Check if there are any pins wired to this boolean input.
	TArray<USMGraphK2Node_ConduitResultNode*> RootNodeList;

	// We want to find the node even if it's buried in a nested graph.
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_ConduitResultNode>(const_cast<USMConduitGraph*>(this), RootNodeList);

	for (USMGraphK2Node_RootNode* RootNode : RootNodeList)
	{
		if (RootNode->GetInputPin()->LinkedTo.Num() || RootNode->GetInputPin()->DefaultValue.ToBool())
		{
			return true;
		}
	}

	return false;
}
