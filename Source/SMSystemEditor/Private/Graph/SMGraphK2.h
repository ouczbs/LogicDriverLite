// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "SMInstance.h"
#include "SMGraphK2.generated.h"


UCLASS()
class SMSYSTEMEDITOR_API USMGraphK2 : public UEdGraph
{
	GENERATED_UCLASS_BODY()

	/** Checks if any of the root nodes of this graph are wired to a logic pin. */
	virtual bool HasAnyLogicConnections() const;

	/** Checks the graph node owning this graph and returns the runtime state. */
	virtual FSMNode_Base* GetRuntimeNode() const { return nullptr; }
};
