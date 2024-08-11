
#include "Graph/StateAbilityScriptGraph.h"

#include "Node/GraphAbilityNode.h"
#include "Component/StateAbility/StateAbilityNodeBase.h"
#include "Component/StateAbility/StateAbilityAction.h"
#include "Component/StateAbility/StateAbilityBranch.h"
#include "Component/StateAbility/Script/StateAbilityScript.h"
#include "StateAbilityScriptEditorData.h"
#include "SchemaAction/SGraphEdAbilitySchemaActions.h"

#include "SGraphNode.h"
#include "SGraphPanel.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "StateAbilityScriptGraph"


struct FCompareNodeYLocation
{
	bool operator()(const UEdGraphPin& A, const UEdGraphPin& B) const
	{
		const UEdGraphNode* NodeA = A.GetOwningNode();
		const UEdGraphNode* NodeB = B.GetOwningNode();

		if (NodeA->NodePosY == NodeB->NodePosY)
		{
			return NodeA->NodePosX < NodeB->NodePosX;
		}

		return NodeA->NodePosY < NodeB->NodePosY;
	}
};

/////////////////////////////////////////////////////
// UStateAbilityScriptGraph

UStateAbilityScriptGraph::UStateAbilityScriptGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAllowDeletion = false;
	bAllowRenaming = true;
}

void UStateAbilityScriptGraph::OnCompile()
{
	UEdGraphPin::ResolveAllPinReferences();
	if (Nodes.Num() > 0 && Nodes[0] && Nodes[0]->Pins.Num() > 0 && Nodes[0]->Pins[0]->LinkedTo.Num() > 0)
	{
		UGraphAbilityNode* GraphNode = Cast<UGraphAbilityNode>(Nodes[0]->Pins[0]->LinkedTo[0]->GetOwningNode());	// 因为Graph的第一个Node是EntryNode，不需要存储。

		UStateAbilityScriptArchetype* ScriptArchetype = CastChecked<UStateAbilityScriptArchetype>(GetOuter());
		UPackage* Package = ScriptArchetype->GetPackage();
		//UConfigVarsLazyDataBag* LazyDataBag = FindObjectFast<UConfigVarsLazyDataBag>(Package, FName("ConfigVarsLazyDataBag"), true);

		EventSlotsMap.Empty();

		CompileNode_Recursive(ScriptArchetype, GraphNode);
	}
}

void UStateAbilityScriptGraph::CompileNode_Recursive(UStateAbilityScriptArchetype* ScriptArchetype, UGraphAbilityNode* RootGraphNode)
{
	if (!IsValid(RootGraphNode))
	{
		return;
	}
	if (UStateAbilityEventSlot* EventSlot = Cast<UStateAbilityEventSlot>(RootGraphNode->NodeInstance))
	{
		// 搜集EventSlotsMap，后面StateTree需要用到
		// 目前EventSlot后面不会有后继Node
		EventSlotsMap.Add(EventSlot->EditorGetUID(ScriptArchetype->GetDefaultScript()), EventSlot);
		return;
	}

	/**
	 * Search linked NextNode
	 */
	for (int32 PinIdx = 0; PinIdx < RootGraphNode->Pins.Num(); PinIdx++)
	{
		UEdGraphPin* Pin = RootGraphNode->Pins[PinIdx];
		if (Pin->Direction != EGPD_Output)
		{
			continue;
		}
		for (int32 LinkId = 0; LinkId < Pin->LinkedTo.Num(); ++LinkId)
		{
			UGraphAbilityNode* GraphNode = Cast<UGraphAbilityNode>(Pin->LinkedTo[LinkId]->GetOwningNode());

			CompileNode_Recursive(ScriptArchetype, GraphNode);
		}
	}
}

void UStateAbilityScriptGraph::NotifyGraphChanged(const FEdGraphEditAction& Action)
{
	if (Action.Action == GRAPHACTION_RemoveNode)
	{
		for (const UEdGraphNode* Node : Action.Nodes)
		{
			UStateAbilityScriptArchetype* ScriptArchetype = CastChecked<UStateAbilityScriptArchetype>(GetOuter());

			const UGraphAbilityNode* GraphNode = Cast<UGraphAbilityNode>(Node);
			UStateAbilityNodeBase* NextAbilityNode = Cast<UStateAbilityNodeBase>(GraphNode->NodeInstance);
			if (IsValid(NextAbilityNode))
			{
				NextAbilityNode->ClearFlags(RF_Public | RF_Standalone);
				NextAbilityNode->SetFlags(RF_Transient);
				NextAbilityNode->Rename(nullptr, GetTransientPackage());
			}
		}
	}

	Super::NotifyGraphChanged(Action);
}

void UStateAbilityScriptGraph::UpdateAsset(EUpdateFlags UpdateFlags)
{
	Super::UpdateAsset(UpdateFlags);

	UStateAbilityScriptArchetype* ScriptArchetype = CastChecked<UStateAbilityScriptArchetype>(GetOuter());
	UStateAbilityScriptEditorData* EditorData = Cast<UStateAbilityScriptEditorData>(ScriptArchetype->EditorData);
	check(EditorData);

	// we can't look at pins until pin references have been fixed up post undo:
	UEdGraphPin::ResolveAllPinReferences();

	if (UpdateFlags == EUpdateFlags::RebuildGraph)
	{
		EditorData->TryRemoveOrphanedObjects();
	}
}

void UStateAbilityScriptGraph::Release()
{
	if (Nodes.Num() > 0 && Nodes[0] && Nodes[0]->Pins.Num() > 0 && Nodes[0]->Pins[0]->LinkedTo.Num() > 0)
	{
		UGraphAbilityNode* GraphNode = Cast<UGraphAbilityNode>(Nodes[0]->Pins[0]->LinkedTo[0]->GetOwningNode());	// 因为Graph的第一个Node是EntryNode，不需要存储。

		UStateAbilityScriptArchetype* ScriptArchetype = CastChecked<UStateAbilityScriptArchetype>(GetOuter());

		Release_Recursive(ScriptArchetype, GraphNode);
	}
}

void UStateAbilityScriptGraph::Release_Recursive(UStateAbilityScriptArchetype* ScriptArchetype, UGraphAbilityNode* RootGraphNode)
{
	if (!IsValid(RootGraphNode))
	{
		return;
	}

	if (UStateAbilityNodeBase* AbilityNode = Cast<UStateAbilityNodeBase>(RootGraphNode->NodeInstance))
	{
		//if (UConfigVarsData* ConfigVarsData = FConfigVarsReader::FindOrLoadLazyData(ScriptArchetype, AbilityNode->GetDataClass(), AbilityNode->UniqueID, true))
		//{
		//	ConfigVarsData->ClearFlags(RF_Public | RF_Standalone);
		//	ConfigVarsData->SetFlags(RF_Transient);
		//	ConfigVarsData->Rename(nullptr, GetTransientPackage());
		//	ConfigVarsData->MarkAsGarbage();
		//}
	}
}

UStateAbilityScriptGraph* UStateAbilityScriptGraph::CreateNewGraph(UObject* ParentScope, const FName& GraphName, TSubclassOf<class UEdGraph> GraphClass, TSubclassOf<class UEdGraphSchema> SchemaClass)
{
	UStateAbilityScriptGraph* EdGraph = Cast<UStateAbilityScriptGraph>(FBlueprintEditorUtils::CreateNewGraph(ParentScope, GraphName, GraphClass, SchemaClass));
	EdGraph->Init();
	return EdGraph;
}

#undef LOCTEXT_NAMESPACE
