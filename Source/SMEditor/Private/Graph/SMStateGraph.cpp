// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#include "SMStateGraph.h"


USMStateGraph::USMStateGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), EntryNode(nullptr)
{
}

USMGraphNode_StateNodeBase* USMStateGraph::GetOwningStateNode() const
{
	return Cast<USMGraphNode_StateNodeBase>(GetOuter());
}
