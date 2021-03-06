// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "SMGraphK2Node_Base.generated.h"


#define STATE_MACHINE_HELPER_CATEGORY TEXT("Logic Driver|State Machines|Graph Nodes")

UCLASS()
class SMEDITOR_API USMGraphK2Node_Base : public UK2Node
{
	GENERATED_UCLASS_BODY()

public:
	// UK2Node
	virtual ERedirectType DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const override;
	// ~UK2Node
	
	virtual UEdGraphPin* GetInputPin() const;
	virtual UEdGraphPin* GetOutputPin() const;
	virtual UEdGraphNode* GetOutputNode() const;
	virtual UEdGraphPin* GetThenPin() const;

	/** In case validation needs to occur before being moved to the consolidated event graph. */
	virtual void PreCompileValidate(class FCompilerResultsLog& MessageLog) {}
	virtual void PostCompileValidate(class FCompilerResultsLog& MessageLog) {}
	
	/** Restrict all collapse options from showing up in the context menu. */
	virtual bool CanCollapseNode() const { return true; }
	/** Restricts just function and macro context options. */
	virtual bool CanCollapseToFunctionOrMacro() const { return true; }
};
