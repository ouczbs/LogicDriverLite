// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#include "SMGraphNode_TransitionEdge.h"
#include "EdGraphUtilities.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraph.h"
#include "SMTransition.h"
#include "Graph/Schema/SMTransitionGraphSchema.h"
#include "Graph/SMTransitionGraph.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Helpers/SMGraphK2Node_FunctionNodes.h"
#include "Helpers/SMGraphK2Node_StateWriteNodes.h"
#include "RootNodes/SMGraphK2Node_TransitionEnteredNode.h"
#include "RootNodes/SMGraphK2Node_TransitionInitializedNode.h"
#include "RootNodes/SMGraphK2Node_TransitionShutdownNode.h"


#define LOCTEXT_NAMESPACE "SMGraphTransition"

USMGraphNode_TransitionEdge::USMGraphNode_TransitionEdge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), DelegateOwnerInstance(SMDO_This), DelegateOwnerClass(nullptr),
	  PriorityOrder_DEPRECATED(0),
	  bCanEvaluate_DEPRECATED(true), bCanEvaluateFromEvent_DEPRECATED(true), bCanEvalWithStartState_DEPRECATED(true),
	  bWasEvaluating(false)
{
	bCanRenameNode = false;
}

void USMGraphNode_TransitionEdge::SetRuntimeDefaults(FSMTransition& Transition) const
{
	if (USMTransitionInstance* Instance = Cast<USMTransitionInstance>(GetNodeTemplate()))
	{
		Transition.bAlwaysFalse = !PossibleToTransition();
		Transition.ConditionalEvaluationType = GetTransitionGraph()->GetConditionalEvaluationType();
		Transition.Priority = Instance->PriorityOrder;
		Transition.bCanEvaluate = Instance->bCanEvaluate;
		Transition.bCanEvaluateFromEvent = Instance->bCanEvaluateFromEvent;
		Transition.bCanEvalWithStartState = Instance->bCanEvalWithStartState;
		Transition.bRunParallel = false;
		Transition.bEvalIfNextStateActive = false;
		Transition.SetNodeName(GetTransitionName());
	}
}

void USMGraphNode_TransitionEdge::CopyFrom(const USMGraphNode_TransitionEdge& Transition)
{
	TransitionClass = Transition.TransitionClass;
	DelegateOwnerInstance = Transition.DelegateOwnerInstance;
	DelegateOwnerClass = Transition.DelegateOwnerClass;
	DelegatePropertyName = Transition.DelegatePropertyName;
	NodeInstanceTemplate = Transition.NodeInstanceTemplate;
}

void USMGraphNode_TransitionEdge::AllocateDefaultPins()
{
	UEdGraphPin* Inputs = CreatePin(EGPD_Input, TEXT("Transition"), TEXT("In"));
	Inputs->bHidden = true;
	UEdGraphPin* Outputs = CreatePin(EGPD_Output, TEXT("Transition"), TEXT("Out"));
	Outputs->bHidden = true;
}

FText USMGraphNode_TransitionEdge::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromString(GetTransitionName());
}

void USMGraphNode_TransitionEdge::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if (Pin->LinkedTo.Num() == 0)
	{
		// Commit suicide; transitions must always have an input and output connection
		Modify();

		// Our parent graph will have our graph in SubGraphs so needs to be modified to record that.
		if (UEdGraph* ParentGraph = GetGraph())
		{
			ParentGraph->Modify();
		}

		DestroyNode();
	}
}

void USMGraphNode_TransitionEdge::PostPlacedNewNode()
{
	SetToCurrentVersion();
	
	CreateBoundGraph();
	SetupDelegateDefaults();

	if (bGenerateTemplateOnNodePlacement)
	{
		InitTemplate();
	}
}

void USMGraphNode_TransitionEdge::PrepareForCopying()
{
	Super::PrepareForCopying();
	BoundGraph->Rename(nullptr, this, REN_DoNotDirty | REN_DontCreateRedirectors);
}

void USMGraphNode_TransitionEdge::PostPasteNode()
{
	if (!BoundGraph)
	{
		CreateBoundGraph();
	}

	for (UEdGraphNode* GraphNode : BoundGraph->Nodes)
	{
		GraphNode->CreateNewGuid();
		GraphNode->PostPasteNode();
		// Required to correct context display issues.
		GraphNode->ReconstructNode();
	}

	Super::PostPasteNode();

	// We don't want to paste nodes in that aren't fully linked (transition nodes have fixed pins as they
	// really describe the connection between two other nodes). If we find one missing link, get rid of the node.
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->LinkedTo.Num() == 0)
		{
			DestroyNode();
			break;
		}
	}
}

void USMGraphNode_TransitionEdge::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	// Enable templates
	if (PropertyName == GET_MEMBER_NAME_CHECKED(USMGraphNode_TransitionEdge, TransitionClass))
	{
		InitTemplate();
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (PropertyName == GET_MEMBER_NAME_CHECKED(USMGraphNode_TransitionEdge, DelegatePropertyName))
	{
		InitTransitionDelegate();
	}
	else if(PropertyName == GET_MEMBER_NAME_CHECKED(USMGraphNode_TransitionEdge, DelegateOwnerInstance))
	{
		DelegatePropertyName = NAME_None;
		DelegateOwnerClass = nullptr;
		InitTransitionDelegate();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(USMGraphNode_TransitionEdge, DelegateOwnerClass))
	{
		DelegatePropertyName = NAME_None;
		InitTransitionDelegate();
	}
}

void USMGraphNode_TransitionEdge::DestroyNode()
{
	Modify();
	if (BoundGraph)
	{
		BoundGraph->Modify();
	}
	
	UEdGraph* GraphToRemove = BoundGraph;

	BoundGraph = nullptr;
	Super::DestroyNode();

	if (GraphToRemove)
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(this);
		FBlueprintEditorUtils::RemoveGraph(Blueprint, GraphToRemove, EGraphRemoveFlags::Recompile);
	}
}

void USMGraphNode_TransitionEdge::ReconstructNode()
{
	Super::ReconstructNode();
	RefreshTransitionDelegate();
}

void USMGraphNode_TransitionEdge::ResetDebugState()
{
	Super::ResetDebugState();

	// Prevents a previous cycle from showing it as running.
	if (const FSMTransition* DebugNode = (FSMTransition*)GetDebugNode())
	{
		const_cast<FSMTransition*>(DebugNode)->bWasEvaluating = bWasEvaluating = false;
	}
}

void USMGraphNode_TransitionEdge::UpdateTime(float DeltaTime)
{
	const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
	if (Settings->bDisplayTransitionEvaluation)
	{
		if (const FSMTransition* DebugNode = (FSMTransition*)GetDebugNode())
		{
			if (WasEvaluating() && (DebugNode->IsActive() || DebugNode->bWasActive))
			{
				// Cancel evaluation display and let the super method reset.
				bWasEvaluating = false;
				bWasDebugActive = false;
			}
			else if (DebugNode->bIsEvaluating || DebugNode->bWasEvaluating)
			{
				// Not active but evaluating.
				bIsDebugActive = true;
				bWasEvaluating = true;
			}
			const_cast<FSMTransition*>(DebugNode)->bWasEvaluating = false;
		}
	}

	Super::UpdateTime(DeltaTime);

	if (!WasDebugNodeActive())
	{
		bWasEvaluating = false;
	}
}

void USMGraphNode_TransitionEdge::ImportDeprecatedProperties()
{
	Super::ImportDeprecatedProperties();

	if (USMTransitionInstance* Instance = Cast<USMTransitionInstance>(GetNodeTemplate()))
	{
		Instance->bCanEvaluate = bCanEvaluate_DEPRECATED;
		Instance->bCanEvaluateFromEvent = bCanEvaluateFromEvent_DEPRECATED;
		Instance->bCanEvalWithStartState = bCanEvalWithStartState_DEPRECATED;
		Instance->PriorityOrder = PriorityOrder_DEPRECATED;
	}
}

void USMGraphNode_TransitionEdge::PlaceDefaultInstanceNodes()
{
	Super::PlaceDefaultInstanceNodes();
}

void USMGraphNode_TransitionEdge::CreateBoundGraph()
{
	// Create a new state machine graph
	check(BoundGraph == NULL);
	BoundGraph = FBlueprintEditorUtils::CreateNewGraph(
		this,
		NAME_None,
		USMTransitionGraph::StaticClass(),
		USMTransitionGraphSchema::StaticClass());
	check(BoundGraph);

	// Find an interesting name
	FEdGraphUtilities::RenameGraphToNameOrCloseToName(BoundGraph, GetTransitionName());

	// Initialize the state machine graph
	const UEdGraphSchema* Schema = BoundGraph->GetSchema();
	Schema->CreateDefaultNodesForGraph(*BoundGraph);

	// Add the new graph as a child of our parent graph
	UEdGraph* ParentGraph = GetGraph();

	if (ParentGraph->SubGraphs.Find(BoundGraph) == INDEX_NONE)
	{
		ParentGraph->SubGraphs.Add(BoundGraph);
	}
}

void USMGraphNode_TransitionEdge::SetBoundGraph(UEdGraph* Graph)
{
	BoundGraph = Graph;
}

UClass* USMGraphNode_TransitionEdge::GetSelectedDelegateOwnerClass() const
{
	if (DelegateOwnerInstance == SMDO_This)
	{
		return FBlueprintEditorUtils::FindBlueprintForNodeChecked(this)->SkeletonGeneratedClass;
	}
	
	if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(DelegateOwnerClass))
	{
		if(UBlueprint* BP = Cast<UBlueprint>(BPGC->ClassGeneratedBy))
		{
			return BP->SkeletonGeneratedClass;
		}
	}

	return DelegateOwnerClass;
}

void USMGraphNode_TransitionEdge::SetupDelegateDefaults()
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(this);
	DelegateOwnerClass = Blueprint->SkeletonGeneratedClass;
}

void USMGraphNode_TransitionEdge::InitTransitionDelegate()
{
	if (!BoundGraph)
	{
		return;
	}

	// Backup existing.
	FVector2D PreviousEntryPosition;
	bool bHadPreviousNodes = false;
	UEdGraphPin* PreviousThenPin = nullptr;
	TArray<USMGraphK2Node_FunctionNode_TransitionEvent*> PreviousEventNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_FunctionNode_TransitionEvent>(BoundGraph, PreviousEventNodes);
	for (USMGraphK2Node_FunctionNode_TransitionEvent* EventNode : PreviousEventNodes)
	{
		bHadPreviousNodes = true;
		PreviousEntryPosition = FVector2D(EventNode->NodePosX, EventNode->NodePosY);
		PreviousThenPin = EventNode->GetThenPin();
	}

	// Create new.
	if (FMulticastDelegateProperty* DelegateProperty = FSMBlueprintEditorUtils::GetDelegateProperty(DelegatePropertyName, GetSelectedDelegateOwnerClass()))
	{
		// Event entry node.
		FGraphNodeCreator<USMGraphK2Node_FunctionNode_TransitionEvent> NodeCreator(*BoundGraph);
		USMGraphK2Node_FunctionNode_TransitionEvent* OurEventNode = NodeCreator.CreateNode();
		const FVector2D Position = bHadPreviousNodes ? PreviousEntryPosition : BoundGraph->GetGoodPlaceForNewNode();
		OurEventNode->NodePosX = Position.X;
		OurEventNode->NodePosY = Position.Y;
		OurEventNode->SetEventReferenceFromDelegate(DelegateProperty, DelegateOwnerInstance);
		OurEventNode->TransitionClass = TransitionClass;
		NodeCreator.Finalize();
		if (PreviousThenPin)
		{
			OurEventNode->GetThenPin()->CopyPersistentDataFromOldPin(*PreviousThenPin);
		}

		// Create return node if it doesn't exist.
		TArray<USMGraphK2Node_StateWriteNode_TransitionEventReturn*> ResultNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_StateWriteNode_TransitionEventReturn>(BoundGraph, ResultNodes);

		if (!bHadPreviousNodes || ResultNodes.Num() == 0)
		{
			FGraphNodeCreator<USMGraphK2Node_StateWriteNode_TransitionEventReturn> NodeReturnCreator(*BoundGraph);
			USMGraphK2Node_StateWriteNode_TransitionEventReturn* OurReturnNode = NodeReturnCreator.CreateNode();
			OurReturnNode->NodePosX = OurEventNode->NodePosX + OurEventNode->NodeWidth + 450;
			OurReturnNode->NodePosY = OurEventNode->NodePosY;
			NodeReturnCreator.Finalize();

			OurReturnNode->GetSchema()->TryCreateConnection(OurEventNode->GetOutputPin(), OurReturnNode->GetExecPin());
		}
	}

	// Clear existing.
	for (USMGraphK2Node_FunctionNode_TransitionEvent* EventNode : PreviousEventNodes)
	{
		FSMBlueprintEditorUtils::RemoveNode(FSMBlueprintEditorUtils::FindBlueprintForNodeChecked(this), EventNode);
	}
}

void USMGraphNode_TransitionEdge::GoToTransitionEventNode()
{
	TArray<USMGraphK2Node_FunctionNode_TransitionEvent*> PreviousEventNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_FunctionNode_TransitionEvent>(BoundGraph, PreviousEventNodes);

	if(PreviousEventNodes.Num() > 0)
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(PreviousEventNodes[0]);
	}
}

void USMGraphNode_TransitionEdge::RefreshTransitionDelegate()
{
	if (DelegatePropertyName.IsNone() || BoundGraph == nullptr)
	{
		// BoundGraph can be nullptr on copy paste without both states connected.
		return;
	}
	
	if (FMulticastDelegateProperty* DelegateProperty = FSMBlueprintEditorUtils::GetDelegateProperty(DelegatePropertyName, GetSelectedDelegateOwnerClass()))
	{
		TArray<USMGraphK2Node_FunctionNode_TransitionEvent*> PreviousEventNodes;
		FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphK2Node_FunctionNode_TransitionEvent>(BoundGraph, PreviousEventNodes);

		for (USMGraphK2Node_FunctionNode_TransitionEvent* EventNode : PreviousEventNodes)
		{
			EventNode->SetEventReferenceFromDelegate(DelegateProperty, DelegateOwnerInstance);
		}
	}
}

FString USMGraphNode_TransitionEdge::GetTransitionName() const
{
	USMGraphNode_StateNodeBase* State1 = GetFromState();
	USMGraphNode_StateNodeBase* State2 = GetToState();

	const FString State1Name = State1 ? State1->GetStateName() : "StartState";
	const FString State2Name = State2 ? State2->GetStateName() : "EndState";

	return FString::Printf(TEXT("%s to %s"), *State1Name, *State2Name);
}

void USMGraphNode_TransitionEdge::CreateConnections(USMGraphNode_StateNodeBase* Start, USMGraphNode_StateNodeBase* End)
{
	Pins[0]->Modify();
	Pins[0]->LinkedTo.Empty();

	Start->GetOutputPin()->Modify();
	Pins[0]->MakeLinkTo(Start->GetOutputPin());

	// This to next
	Pins[1]->Modify();
	Pins[1]->LinkedTo.Empty();

	End->GetInputPin()->Modify();
	Pins[1]->MakeLinkTo(End->GetInputPin());

	SetDefaultsWhenPlaced();
}

bool USMGraphNode_TransitionEdge::PossibleToTransition() const
{
	if (USMTransitionGraph* Graph = Cast<USMTransitionGraph>(BoundGraph))
	{
		return Graph->HasAnyLogicConnections();
	}

	return false;
}

USMTransitionGraph* USMGraphNode_TransitionEdge::GetTransitionGraph() const
{
	return CastChecked<USMTransitionGraph>(BoundGraph);
}

USMGraphNode_StateNodeBase* USMGraphNode_TransitionEdge::GetFromState() const
{
	if (Pins.Num() && Pins[0]->LinkedTo.Num() > 0)
	{
		return Cast<USMGraphNode_StateNodeBase>(Pins[0]->LinkedTo[0]->GetOwningNode());
	}

	return nullptr;
}

USMGraphNode_StateNodeBase* USMGraphNode_TransitionEdge::GetToState() const
{
	if (Pins.Num() > 1 && Pins[1]->LinkedTo.Num() > 0)
	{
		return Cast<USMGraphNode_StateNodeBase>(Pins[1]->LinkedTo[0]->GetOwningNode());
	}

	return nullptr;
}

bool USMGraphNode_TransitionEdge::ShouldRunParallel() const
{
	return false;
}

FLinearColor USMGraphNode_TransitionEdge::GetBackgroundColor() const
{
	const FLinearColor BaseColor = Super::GetBackgroundColor();
	
	const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
	if (Settings->bDisplayTransitionEvaluation)
	{
		if (const FSMTransition* DebugNode = (FSMTransition*)GetDebugNode())
		{
			if (DebugNode->bIsEvaluating || bWasEvaluating)
			{
				const float TimeToFade = 0.7f;
				const float DebugTime = GetDebugTime();
				if (DebugTime < TimeToFade)
				{
					return FLinearColor::LerpUsingHSV(Settings->EvaluatingTransitionColor, BaseColor, DebugTime / TimeToFade);
				}
			}
		}
	}

	return BaseColor;
}

FLinearColor USMGraphNode_TransitionEdge::GetActiveBackgroundColor() const
{
	return FSMBlueprintEditorUtils::GetEditorSettings()->ActiveTransitionColor;
}

void USMGraphNode_TransitionEdge::SetNodeClass(UClass* Class)
{
	TransitionClass = Class;
	Super::SetNodeClass(Class);
}

float USMGraphNode_TransitionEdge::GetMaxDebugTime() const
{
	return FSMBlueprintEditorUtils::GetEditorSettings()->TimeToFadeLastActiveTransition;
}

void USMGraphNode_TransitionEdge::PreCompile(FSMKismetCompilerContext& CompilerContext)
{
	Super::PreCompile(CompilerContext);

	if (!DelegatePropertyName.IsNone())
	{
		if (UClass* DelegateClass = GetSelectedDelegateOwnerClass())
		{
			if (DelegateClass->FindPropertyByName(DelegatePropertyName) == nullptr)
			{
				CompilerContext.MessageLog.Error(TEXT("Delegate property not found for transition @@."), this);
			}
			else
			{
				RefreshTransitionDelegate();
			}
		}
	}
}

FLinearColor USMGraphNode_TransitionEdge::GetTransitionColor(bool bIsHovered) const
{
	const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();

	const FLinearColor HoverColor = Settings->TransitionHoverColor;
	const FLinearColor BaseColor = GetBackgroundColor();

	return bIsHovered ? BaseColor * HoverColor : BaseColor;
}

FLinearColor USMGraphNode_TransitionEdge::Internal_GetBackgroundColor() const
{
	const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
	const FLinearColor ColorModifier = GetCustomBackgroundColor() ? *GetCustomBackgroundColor() : FLinearColor(1.f, 1.f, 1.f, 1.f);
	const FLinearColor DefaultColor = Settings->TransitionEmptyColor * ColorModifier;
	
	USMTransitionGraph* Graph = Cast<USMTransitionGraph>(BoundGraph);

	if (!Graph)
	{
		return DefaultColor;
	}

	const bool bHasResultLogic = Graph->HasAnyLogicConnections();
	// This transition will never be taken.
	if (!bHasResultLogic)
	{
		return DefaultColor;
	}

	const bool bHasTransitionTunnel = Graph->HasTransitionTunnel();
	// Regular transition.
	if (!bHasTransitionTunnel || !Settings->bEnableTransitionWithEntryLogicColor)
	{
		return Settings->TransitionValidColor * ColorModifier;
	}

	// Transition with execution logic.
	if (bHasTransitionTunnel)
	{
		return Settings->TransitionWithEntryLogicColor * ColorModifier;
	}

	return DefaultColor;
}

void USMGraphNode_TransitionEdge::SetDefaultsWhenPlaced()
{
}

#undef LOCTEXT_NAMESPACE

