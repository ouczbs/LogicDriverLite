// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#include "SMGraphK2.h"
#include "Nodes/RootNodes/SMGraphK2Node_RootNode.h"
#include "Utilities/SMBlueprintEditorUtils.h"


USMGraphK2::USMGraphK2(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USMGraphK2::HasAnyLogicConnections() const
{
	TArray<USMGraphK2Node_RootNode*> RootNodeList;

	// We want to find the node even if it's buried in a nested graph.
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_RootNode>(const_cast<USMGraphK2*>(this), RootNodeList);

	for (USMGraphK2Node_RootNode* RootNode : RootNodeList)
	{
		if (RootNode->GetOutputNode())
		{
			return true;
		}
	}

	return false;
}