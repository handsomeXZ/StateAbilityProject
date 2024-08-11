// Fill out your copyright notice in the Description page of Project Settings.


#include "StateAbilityScriptEdGraphSchema.h"

#include "Node/StateAbilityEditorTypes.h"
#include "Node/GraphAbilityNode_Entry.h"
#include "Node/GraphAbilityNode_Action.h"
#include "Node/GraphAbilityNode_State.h"
#include "Component/StateAbility/StateAbilityNodeBase.h"
#include "Component/StateAbility/StateAbilityAction.h"
#include "Component/StateAbility/StateAbilityState.h"
#include "StateAbilityScriptEditor.h"
#include "SchemaAction/SGraphEdAbilitySchemaActions.h"

#include "AIGraphTypes.h"

#include "Framework/Commands/GenericCommands.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "StateAbilityScriptEdGraphSchema"

int32 UStateAbilityScriptEdGraphSchema::CurrentCacheRefreshID = 0;

namespace StateAbilityScriptEdGraphSchemaUtils
{
	class FNodeVisitorCycleChecker
	{
	public:
		/** Check whether a loop in the graph would be caused by linking the passed-in nodes */
		bool CheckForLoop(UEdGraphNode* StartNode, UEdGraphNode* EndNode)
		{
			VisitedNodes.Add(EndNode);
			return TraverseInputNodesToRoot(StartNode);
		}

	private:
		/**
		 * Helper function for CheckForLoop()
		 * @param	Node	The node to start traversal at
		 * @return true if we reached a root node (i.e. a node with no input pins), false if we encounter a node we have already seen
		 */
		bool TraverseInputNodesToRoot(UEdGraphNode* Node)
		{
			VisitedNodes.Add(Node);

			// Follow every input pin until we cant any more ('root') or we reach a node we have seen (cycle)
			for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
			{
				UEdGraphPin* MyPin = Node->Pins[PinIndex];

				if (MyPin->Direction == EGPD_Input)
				{
					for (int32 LinkedPinIndex = 0; LinkedPinIndex < MyPin->LinkedTo.Num(); ++LinkedPinIndex)
					{
						UEdGraphPin* OtherPin = MyPin->LinkedTo[LinkedPinIndex];
						if (OtherPin)
						{
							UEdGraphNode* OtherNode = OtherPin->GetOwningNode();
							if (VisitedNodes.Contains(OtherNode))
							{
								return false;
							}
							else
							{
								return TraverseInputNodesToRoot(OtherNode);
							}
						}
					}
				}
			}

			return true;
		}

		TSet<UEdGraphNode*> VisitedNodes;
	};
}

UStateAbilityScriptEdGraphSchema::UStateAbilityScriptEdGraphSchema(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UStateAbilityScriptEdGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	// Create the entry、exit tunnels
	FGraphNodeCreator<UGraphAbilityNode_Entry> EntryNodeCreator(Graph);
	UGraphAbilityNode_Entry* EntryNode = EntryNodeCreator.CreateNode();
	EntryNodeCreator.Finalize();
	SetNodeMetaData(EntryNode, FNodeMetadata::DefaultGraphNode);

	//FGraphNodeCreator<USDTGraphNode_DExit> ExitNodeCreator(Graph);
	//USDTGraphNode_DExit* ExitNode = ExitNodeCreator.CreateNode();
	//ExitNode->NodeClass = USDTNode_DExit::StaticClass();
	//ExitNode->NodePosX = 200;
	//ExitNodeCreator.Finalize();
	//SetNodeMetaData(ExitNode, FNodeMetadata::DefaultGraphNode);
}

void UStateAbilityScriptEdGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	const FName PinCategory = ContextMenuBuilder.FromPin ?
		ContextMenuBuilder.FromPin->PinType.PinCategory :
		UStateAbilityEditorTypes::PinCategory_Defualt;

	const bool bNoParent = (ContextMenuBuilder.FromPin == NULL);
	const bool bOnlyEntry = (PinCategory == UStateAbilityEditorTypes::PinCategory_Entry);
	const bool bOnlyExec = (PinCategory == UStateAbilityEditorTypes::PinCategory_Exec);
	const bool bOnlyEvent = (PinCategory == UStateAbilityEditorTypes::PinCategory_Event);

	const bool bAllowAction = bNoParent || bOnlyEntry || bOnlyExec || bOnlyEvent;
	const bool bAllowState = bNoParent || bOnlyEntry || bOnlyExec || bOnlyEvent;

	FStateAbilityScriptEditorModule& EditorModule = FModuleManager::GetModuleChecked<FStateAbilityScriptEditorModule>(TEXT("StateAbilityScriptEditor"));
	FGraphNodeClassHelper* ClassCache = EditorModule.GetClassCache().Get();

	if (bAllowAction)
	{
		TArray<FGraphNodeClassData> GatheredClasses;
		ClassCache->GatherClasses(UStateAbilityAction::StaticClass(), GatheredClasses);

		for (auto& ClassData : GatheredClasses)
		{
			const FText ActionTypeName = FText::FromString(FName::NameToDisplayString(ClassData.ToString(), false));

			TSharedPtr<FSAbilitySchemaAction_NewNode> ActionSchema = TSharedPtr<FSAbilitySchemaAction_NewNode>(
				new FSAbilitySchemaAction_NewNode(LOCTEXT("NewNode", "ActionSchema"), ActionTypeName, FText::GetEmpty(), 0)
			);
			UGraphAbilityNode_Action* GraphNode = NewObject<UGraphAbilityNode_Action>(ContextMenuBuilder.OwnerOfTemporaries);
			GraphNode->NodeClass = ClassData.GetClass();

			ActionSchema->NodeTemplate = GraphNode;

			ContextMenuBuilder.AddAction(ActionSchema);
		}
	}

	if (bAllowState)
	{
		TArray<FGraphNodeClassData> GatheredClasses;
		ClassCache->GatherClasses(UStateAbilityState::StaticClass(), GatheredClasses);

		for (auto& ClassData : GatheredClasses)
		{
			const FText StateTypeName = FText::FromString(FName::NameToDisplayString(ClassData.ToString(), false));

			TSharedPtr<FSAbilitySchemaState_NewNode> StateSchema = TSharedPtr<FSAbilitySchemaState_NewNode>(
				new FSAbilitySchemaState_NewNode(LOCTEXT("NewNode", "StateSchema"), StateTypeName, FText::GetEmpty(), 0)
			);
			UGraphAbilityNode_State* GraphNode = NewObject<UGraphAbilityNode_State>(ContextMenuBuilder.OwnerOfTemporaries);
			GraphNode->NodeClass = ClassData.GetClass();

			StateSchema->NodeTemplate = GraphNode;

			ContextMenuBuilder.AddAction(StateSchema);
		}
	}
}

void UStateAbilityScriptEdGraphSchema::GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{

	if (Context->Node)
	{
		{
			FToolMenuSection& Section = Menu->AddSection("GraphSchemaNodeActions", LOCTEXT("ClassActionsMenuHeader", "Node Actions"));
			Section.AddMenuEntry(FGenericCommands::Get().Delete);
			Section.AddMenuEntry(FGenericCommands::Get().Cut);
			Section.AddMenuEntry(FGenericCommands::Get().Copy);
			Section.AddMenuEntry(FGenericCommands::Get().Duplicate);
			Section.AddMenuEntry(FGenericCommands::Get().SelectAll);
		}
	}


	Super::GetContextMenuActions(Menu, Context);
}

FLinearColor UStateAbilityScriptEdGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	/*const UGraphEditorSettings* Settings = GetDefault<UGraphEditorSettings>();*/
	if (PinType.PinCategory == UStateAbilityEditorTypes::PinCategory_Entry)
	{
		return FLinearColor::White;
	}

	return FLinearColor::Black;
}

const FPinConnectionResponse UStateAbilityScriptEdGraphSchema::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	if (!PinA || !PinB)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT(""));
	}

	// Make sure the pins are not on the same node
	if (PinA->GetOwningNode() == PinB->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorSameNode", "Both are on the same node"));
	}

	// Compare the directions
	if (PinA->Direction == EGPD_Input)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorInput", "Can't connect node start from input node"));
	}
	else if ((PinA->Direction == EGPD_Input) && (PinB->Direction == EGPD_Input))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorInput", "Can't connect input node to input node"));
	}
	else if ((PinB->Direction == EGPD_Output) && (PinA->Direction == EGPD_Output))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorOutput", "Can't connect output node to output node"));
	}

	const bool bPinAIsEntry = PinA->PinType.PinCategory == UStateAbilityEditorTypes::PinCategory_Entry;
	const bool bPinAIsAction = PinA->PinType.PinCategory == UStateAbilityEditorTypes::PinCategory_Exec;

	const bool bPinBIsEntry = PinB->PinType.PinCategory == UStateAbilityEditorTypes::PinCategory_Entry;
	const bool bPinBIsAction = PinB->PinType.PinCategory == UStateAbilityEditorTypes::PinCategory_Exec;

	// check for cycles
	StateAbilityScriptEdGraphSchemaUtils::FNodeVisitorCycleChecker CycleChecker;
	if (!CycleChecker.CheckForLoop(PinA->GetOwningNode(), PinB->GetOwningNode()))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorcycle", "Can't create a graph cycle"));
	}
	
	if (PinA->Direction == EGPD_Output && PinA->LinkedTo.Num() > 0)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, LOCTEXT("PinConnectReplace", "Replace connection"));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("PinConnect", "Connect nodes"));
}

bool UStateAbilityScriptEdGraphSchema::TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	// Modify the Pin LinkTo, which will affect FConnectionDrawingPolicy::DetermineLinkGeometry()
	if (PinB->Direction == PinA->Direction)
	{
		if (UGraphAbilityNode* Node = Cast<UGraphAbilityNode>(PinB->GetOwningNode()))
		{
			if (PinA->Direction == EGPD_Input)
			{
				PinB = Node->GetOutputPin();
			}
			else
			{
				PinB = Node->GetInputPin();
			}
		}
	}

	const bool bModified = UEdGraphSchema::TryCreateConnection(PinA, PinB);

	return bModified;
}

TSharedPtr<FEdGraphSchemaAction> UStateAbilityScriptEdGraphSchema::GetCreateCommentAction() const
{
	return TSharedPtr<FEdGraphSchemaAction>(static_cast<FEdGraphSchemaAction*>(new FSAbilitySchemaAction_AddComment));
}

bool UStateAbilityScriptEdGraphSchema::IsCacheVisualizationOutOfDate(int32 InVisualizationCacheID) const
{
	return CurrentCacheRefreshID != InVisualizationCacheID;
}

int32 UStateAbilityScriptEdGraphSchema::GetCurrentVisualizationCacheID() const
{
	return CurrentCacheRefreshID;
}

void UStateAbilityScriptEdGraphSchema::ForceVisualizationCacheClear() const
{
	++CurrentCacheRefreshID;
}

#undef LOCTEXT_NAMESPACE