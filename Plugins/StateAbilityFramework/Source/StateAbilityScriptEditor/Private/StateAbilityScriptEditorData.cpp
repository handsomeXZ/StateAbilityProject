#include "StateAbilityScriptEditorData.h"

#include "Component/StateAbility/Script/StateAbilityScript.h"
#include "StateTree/ScriptStateTreeView.h"
#include "Graph/StateAbilityScriptGraph.h"
#include "StateAbilityScriptEdGraphSchema.h"
#include "Node/GraphAbilityNode.h"
#include "Node/GraphAbilityNode_Entry.h"
#include "Node/GraphAbilityNode_State.h"
#include "Node/GraphAbilityNode_Action.h"
#include "Component/StateAbility/StateAbilityNodeBase.h"
#include "Component/StateAbility/StateAbilityAction.h"
#include "Component/StateAbility/StateAbilityState.h"
#include "ConfigVarsLinker.h"

//////////////////////////////////////////////////////////////////////////
// FAbilityScriptViewModel
FAbilityScriptViewModel::FAbilityScriptViewModel(TSharedRef<FUICommandList> InToolkitCommands)
	: ToolkitCommands(InToolkitCommands)
{
	
}

void FAbilityScriptViewModel::RecordEventSlotWithPin(UEdGraphPin* Pin, const FConfigVars_EventSlot& EventSlot)
{
	PinToEventSlot.Add(Pin, EventSlot);
}

void FAbilityScriptViewModel::ClearRecordedEventslot(UEdGraphPin* Pin)
{
	PinToEventSlot.Remove(Pin);
}

FConfigVars_EventSlot* FAbilityScriptViewModel::FindEventSlotByPin(UEdGraphPin* Pin)
{
	return PinToEventSlot.Find(Pin);
}

//////////////////////////////////////////////////////////////////////////
// UStateAbilityScriptEditorData
UStateAbilityScriptEditorData::UStateAbilityScriptEditorData()
{

}

void UStateAbilityScriptEditorData::Init()
{
	UStateAbilityScriptArchetype* ScriptArchetype = this->GetTypedOuter<UStateAbilityScriptArchetype>();
	StateTreeGraph = UStateAbilityScriptGraph::CreateNewGraph(ScriptArchetype, NAME_None, UStateAbilityScriptGraph::StaticClass(), UStateAbilityScriptEdGraphSchema::StaticClass());
}

void UStateAbilityScriptEditorData::OpenEdit(TSharedRef<FUICommandList> InToolkitCommands)
{
	ScriptViewModel = MakeShared<FAbilityScriptViewModel>(InToolkitCommands);
}

void UStateAbilityScriptEditorData::Save()
{
	UStateAbilityScriptArchetype* ScriptArchetype = this->GetTypedOuter<UStateAbilityScriptArchetype>();
	ScriptArchetype->Reset();

	SaveScriptStateTree();

	RemoveOrphanedObjects();
}

void UStateAbilityScriptEditorData::TryRemoveOrphanedObjects()
{
	RemoveOrphanedObjects();
}

UStateTreeBaseNode* FindParentState(UStateTreeBaseNode* Node)
{
	if (!Node->Parent)
	{
		return nullptr;
	}

	if (Node->Parent->NodeType == EScriptStateTreeNodeType::State)
	{
		return Node->Parent;
	}
	else
	{
		return FindParentState(Node->Parent);
	}
}

void UStateAbilityScriptEditorData::SaveScriptStateTree()
{
	UStateAbilityScriptArchetype* ScriptArchetype = this->GetTypedOuter<UStateAbilityScriptArchetype>();
	UPackage* Package = ScriptArchetype->GetPackage();

	ScriptArchetype->PakUID = Package->GetPersistentGuid();

	// we can't look at pins until pin references have been fixed up post undo:
	UEdGraphPin::ResolveAllPinReferences();

	// 重置NetDeltas优化协议
	//ScriptArchetype->NetDeltasProtocal.Reset();


	// 外层循环遍历StateTreeGraph。Graph的第一个Node是EntryNode，不用记录。
	TraverseGraphNodeRecursive(nullptr, Cast<UGraphAbilityNode>(StateTreeGraph->Nodes[0]), [this, ScriptArchetype, ScriptViewModel = ScriptViewModel](UGraphAbilityNode* PrevGraphNode, UGraphAbilityNode* CurrentGraphNode) {
		
		SaveEventSlotToNode(CurrentGraphNode);

		// 重置NodeInstance所有权
		CurrentGraphNode->ResetNodeOwner();

		if (UGraphAbilityNode_State* GraphNode_State = Cast<UGraphAbilityNode_State>(CurrentGraphNode))
		{
			// 处理NetDeltas优化协议
			// @TODO: 暂时没有处理好Attribute属性依赖关系图，所以暂时不优化State。

			UStateTreeStateNode* RootStateNode = GraphNode_State->GetRootStateNode();

			TraverseStateTreeNodeRecursive(nullptr, RootStateNode, [this, ScriptArchetype, ScriptViewModel = ScriptViewModel](UStateTreeBaseNode* PrevNode, UStateTreeBaseNode* CurrentNode) {
				if (PrevNode == nullptr && CurrentNode->NodeType == EScriptStateTreeNodeType::State)
				{
					// 处理StateTree根节点
					UStateTreeStateNode* RootStateNode = CastChecked<UStateTreeStateNode>(CurrentNode);
					UStateAbilityState* StateInstance = RootStateNode->StateInstance;
					uint32 CurrentUniqueID = StateInstance->UniqueID;
					ScriptArchetype->StateTemplates.Add(StateInstance);

				}
				else if (CurrentNode->NodeType == EScriptStateTreeNodeType::State)
				{
					UStateTreeStateNode* ParentStateNode = CastChecked<UStateTreeStateNode>(PrevNode);
					UStateTreeStateNode* CurStateNode = CastChecked<UStateTreeStateNode>(CurrentNode);
					UStateAbilityState* ParentStateInstance = ParentStateNode->StateInstance;
					UStateAbilityState* StateInstance = CurStateNode->StateInstance;
					uint32 CurrentUniqueID = StateInstance->UniqueID;
					ScriptArchetype->StateTemplates.Add(StateInstance);

					SaveEventSlotToNode(CurrentNode);

					// 处理 Related SubState
					ParentStateInstance->RelatedSubState.Empty();
					if (!StateInstance->bIsPersistent)
					{
						ParentStateInstance->RelatedSubState.Add(CurrentUniqueID);
					}
				}
			});

		}
		else if (UGraphAbilityNode_Action* GraphNode_Action = Cast<UGraphAbilityNode_Action>(CurrentGraphNode))
		{
			UStateAbilityNodeBase* ActionInstance = Cast<UStateAbilityNodeBase>(GraphNode_Action->NodeInstance);
			ScriptArchetype->ActionMap.Add(ActionInstance->UniqueID, ActionInstance);
		}
		
		if (UGraphAbilityNode_Entry* GraphNode_Entry = Cast<UGraphAbilityNode_Entry>(PrevGraphNode))
		{
			UStateAbilityNodeBase* NodeInstance = Cast<UStateAbilityNodeBase>(CurrentGraphNode->NodeInstance);
			ScriptArchetype->RootNodeID = NodeInstance->UniqueID;
		}
	});


}

void UStateAbilityScriptEditorData::TraverseStateTreeNodeRecursive(UStateTreeBaseNode* PrevNode, UStateTreeBaseNode* CurrentNode, TFunction<void(UStateTreeBaseNode* PrevNode, UStateTreeBaseNode* CurrentNode)> InProcessor)
{
	if (!IsValid(CurrentNode))
	{
		return;
	}

	InProcessor(PrevNode, CurrentNode);

	for (UStateTreeBaseNode* Child : CurrentNode->Children)
	{
		TraverseStateTreeNodeRecursive(CurrentNode, Child, InProcessor);
	}
}

void UStateAbilityScriptEditorData::TraverseGraphNodeRecursive(UGraphAbilityNode* PrevGraphNode, UGraphAbilityNode* CurrentGraphNode, TFunction<void(UGraphAbilityNode* PrevGraphNode, UGraphAbilityNode* CurrentGraphNode)> InProcessor)
{
	// PrevNode 允许是空的

	if (!IsValid(CurrentGraphNode))
	{
		InProcessor(PrevGraphNode, nullptr);
		return;
	}

	InProcessor(PrevGraphNode, CurrentGraphNode);

	for (int32 PinIdx = 0; PinIdx < CurrentGraphNode->Pins.Num(); PinIdx++)
	{
		UEdGraphPin* Pin = CurrentGraphNode->Pins[PinIdx];
		if (Pin->Direction != EGPD_Output)
		{
			continue;
		}
		for (int32 LinkId = 0; LinkId < Pin->LinkedTo.Num(); ++LinkId)
		{
			UGraphAbilityNode* NextGraphNode = Cast<UGraphAbilityNode>(Pin->LinkedTo[LinkId]->GetOwningNode());
			TraverseGraphNodeRecursive(CurrentGraphNode, NextGraphNode, InProcessor);
		}
	}
}

void UStateAbilityScriptEditorData::RemoveOrphanedObjects()
{
	UStateAbilityScriptArchetype* ScriptArchetype = this->GetTypedOuter<UStateAbilityScriptArchetype>();
	UPackage* Package = ScriptArchetype->GetPackage();

	// 搜集所有的相关对象
	TSet<UObject*> NodeInstances;
	CollectAllNodeInstancesAndPersistObjects(NodeInstances);
	NodeInstances.Remove(nullptr);

	//获取资源中所有节点的列表，并丢弃未使用的节点
	TArray<UObject*> AllInners;
	const bool bIncludeNestedObjects = false;

	// 清扫Script下的遗失对象
	GetObjectsWithOuter(ScriptArchetype, AllInners, bIncludeNestedObjects);
	for (auto InnerIt = AllInners.CreateConstIterator(); InnerIt; ++InnerIt)
	{
		UObject* TestObject = *InnerIt;
		if (!NodeInstances.Contains(TestObject) && CanRemoveNestedObject(TestObject))
		{
			TestObject->SetFlags(RF_Transient);
			TestObject->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);
		}
	}

	GetObjectsWithOuter(StateTreeGraph, AllInners, bIncludeNestedObjects);
	for (auto InnerIt = AllInners.CreateConstIterator(); InnerIt; ++InnerIt)
	{
		UObject* TestObject = *InnerIt;
		if (!NodeInstances.Contains(TestObject) && CanRemoveNestedObject(TestObject))
		{
			TestObject->SetFlags(RF_Transient);
			TestObject->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);
		}
	}

	//// 重新标记 ScriptArchetype 内的 ConfigVarsDataBags
	//ScriptArchetype->ConfigVarsDataBags.Empty();
	//for (UObject* Obj : NodeInstances)
	//{
	//	UStateAbilityNodeBase* AbilityNode = Cast<UStateAbilityNodeBase>(Obj);
	//	if (AbilityNode && !AbilityNode->HasAnyFlags(RF_Transient))
	//	{
	//		ScriptArchetype->AddDataExportIndex(AbilityNode->UniqueID, AbilityNode->ConfigVarsBag.GetExportIndex());
	//	}
	//}
}

void UStateAbilityScriptEditorData::CollectAllNodeInstancesAndPersistObjects(TSet<UObject*>& NodeInstances)
{
	//TraverseGraphNodeRecursive(nullptr, Cast<UGraphAbilityNode>(StateTreeGraph->Nodes[0]), [this, &NodeInstances](UGraphAbilityNode* PrevGraphNode, UGraphAbilityNode* CurrentGraphNode) {
	//});

	for (auto Node : StateTreeGraph->Nodes)
	{
		UGraphAbilityNode* CurrentGraphNode = Cast<UGraphAbilityNode>(Node);
		if (CurrentGraphNode)
		{
			NodeInstances.Add(CurrentGraphNode);
			NodeInstances.Add(CurrentGraphNode->NodeInstance);

			if (UGraphAbilityNode_State* GraphNode_State = Cast<UGraphAbilityNode_State>(CurrentGraphNode))
			{
				// 处理NetDeltas优化协议
				// @TODO: 暂时没有处理好Attribute属性依赖关系图，所以暂时不优化State。

				UStateTreeStateNode* RootStateNode = GraphNode_State->GetRootStateNode();
				TraverseStateTreeNodeRecursive(nullptr, RootStateNode, [this, &NodeInstances](UStateTreeBaseNode* PrevNode, UStateTreeBaseNode* CurrentNode) {

					NodeInstances.Add(CurrentNode);
					if (CurrentNode->NodeType == EScriptStateTreeNodeType::State)
					{
						UStateTreeStateNode* CurStateNode = CastChecked<UStateTreeStateNode>(CurrentNode);
						NodeInstances.Add(CurStateNode->StateInstance);
					}

				});
			}
		}
	}
}

bool UStateAbilityScriptEditorData::CanRemoveNestedObject(UObject* TestObject)
{
	return !TestObject->IsA(UEdGraph::StaticClass()) &&
		!TestObject->IsA(UEdGraphSchema::StaticClass()) &&
		!TestObject->IsA(UConfigVarsLinker::StaticClass()) &&
		!TestObject->IsA(UStateAbilityScriptEditorData::StaticClass()) &&
		!TestObject->IsA(UStateAbilityScriptClass::StaticClass()) &&
		!TestObject->IsA(UStateAbilityScript::StaticClass());
}

void UStateAbilityScriptEditorData::SaveEventSlotToNode(UObject* Node)
{
	if (!IsValid(Node))
	{
		return;
	}

	UStateAbilityScriptArchetype* ScriptArchetype = this->GetTypedOuter<UStateAbilityScriptArchetype>();

	if (UGraphAbilityNode* GraphNode = Cast<UGraphAbilityNode>(Node))
	{
		UStateAbilityNodeBase* NodeInstance = Cast<UStateAbilityNodeBase>(GraphNode->NodeInstance);

		UEdGraphPin* InPin = GraphNode->Pins[0];
		for (UEdGraphPin* FromPin : InPin->LinkedTo)
		{
			if (FConfigVars_EventSlot* EventSlot = ScriptViewModel->FindEventSlotByPin(FromPin))
			{
				ScriptArchetype->EventSlotMap.Add(EventSlot->UID, NodeInstance->UniqueID);
			}
		}
	}
	else if (UStateTreeStateNode* StateNode = Cast<UStateTreeStateNode>(Node))
	{
		// 记录StateTreeView内，每个节点所属的EventSlot
		const FStateEventSlotInfo& EventSlotInfo = StateNode->EventSlotInfo;
		ScriptArchetype->EventSlotMap.Add(EventSlotInfo.UID, StateNode->StateInstance->UniqueID);
	}
}