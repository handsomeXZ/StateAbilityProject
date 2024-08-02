#include "StateAbilityScriptEditorData.h"

#include "Component/StateAbility/Script/StateAbilityScript.h"
#include "StateTree/ScriptStateTreeView.h"
#include "Node/GraphAbilityNode.h"
#include "Component/StateAbility/StateAbilityNodeBase.h"
#include "Component/StateAbility/StateAbilityAction.h"
#include "Component/StateAbility/StateAbilityState.h"
#include "ConfigVarsLinker.h"

//////////////////////////////////////////////////////////////////////////
// UStateAbilityScriptEditorData
UStateAbilityScriptEditorData::UStateAbilityScriptEditorData()
{

}

void UStateAbilityScriptEditorData::Init()
{
	// 默认都提供一个Root节点
	AddRootState();
}

void UStateAbilityScriptEditorData::Save()
{
	UStateAbilityScriptArchetype* ScriptArchetype = this->GetTypedOuter<UStateAbilityScriptArchetype>();
	ScriptArchetype->Reset();

	SaveScriptStateTree();

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

	// 记录State，ActionGraph, RootUniqueID，并搜集所有 EventSlot
	bool bIsFirstNode = true;
	TraverseStateTreeNodeRecursive(TreeRoots[0], [ScriptArchetype, &bIsFirstNode](UStateTreeBaseNode* CurrentNode) {
		uint32 CurrentUniqueID = -1;

		if (CurrentNode->NodeType == EScriptStateTreeNodeType::State)
		{
			UStateTreeStateNode* StateNode = CastChecked<UStateTreeStateNode>(CurrentNode);
			UStateAbilityState* StateInstance = StateNode->StateInstance;
			CurrentUniqueID = StateInstance->UniqueID;
			ScriptArchetype->StateTemplates.Add(StateInstance);
			
			// 处理NetDeltas优化协议
			// @TODO: 暂时没有处理好Attribute属性依赖关系图，所以暂时不优化State。
			

			// 处理 Related SubState
			StateInstance->RelatedSubState.Empty();
			if (!StateInstance->bIsPersistent)
			{
				if (UStateTreeBaseNode* ParentState = FindParentState(CurrentNode))
				{
					UStateTreeStateNode* ParentStateNode = CastChecked<UStateTreeStateNode>(ParentState);
					UStateAbilityState* ParentStateInstance = ParentStateNode->StateInstance;
					ParentStateInstance->RelatedSubState.Add(CurrentUniqueID);
				}
			}

		}
		else if (CurrentNode->NodeType == EScriptStateTreeNodeType::Action)
		{
			UStateTreeActionNode* ActionNode = CastChecked<UStateTreeActionNode>(CurrentNode);

			// 因为Graph的第一个Node是EntryNode，不需要存储。
			if (ActionNode->EdGraph && ActionNode->EdGraph->Nodes.Num() > 0 && ActionNode->EdGraph->Nodes[0] && ActionNode->EdGraph->Nodes[0]->Pins.Num() > 0 && ActionNode->EdGraph->Nodes[0]->Pins[0]->LinkedTo.Num() > 0)
			{
				UGraphAbilityNode* GraphNode = Cast<UGraphAbilityNode>(ActionNode->EdGraph->Nodes[0]->Pins[0]->LinkedTo[0]->GetOwningNode());
				UStateAbilityNodeBase* AbilityNode = Cast<UStateAbilityNodeBase>(GraphNode->NodeInstance);
				CurrentUniqueID = AbilityNode->UniqueID;
				ScriptArchetype->ActionSequenceMap.Add(CurrentUniqueID, AbilityNode);
			}
		}
		else if (CurrentNode->NodeType == EScriptStateTreeNodeType::Shared)
		{
			UStateTreeSharedNode* Shared = CastChecked<UStateTreeSharedNode>(CurrentNode);
			UStateTreeBaseNode* SharedNode = Shared->SharedNode.Get();
			if (SharedNode->NodeType == EScriptStateTreeNodeType::Action)
			{
				UStateTreeActionNode* ActionNode = CastChecked<UStateTreeActionNode>(SharedNode);

				// 因为Graph的第一个Node是EntryNode，不需要存储。
				if (ActionNode->EdGraph && ActionNode->EdGraph->Nodes.Num() > 0 && ActionNode->EdGraph->Nodes[0] && ActionNode->EdGraph->Nodes[0]->Pins.Num() > 0 && ActionNode->EdGraph->Nodes[0]->Pins[0]->LinkedTo.Num() > 0)
				{
					UGraphAbilityNode* GraphNode = Cast<UGraphAbilityNode>(ActionNode->EdGraph->Nodes[0]->Pins[0]->LinkedTo[0]->GetOwningNode());
					UStateAbilityNodeBase* AbilityNode = Cast<UStateAbilityNodeBase>(GraphNode->NodeInstance);
					CurrentUniqueID = AbilityNode->UniqueID;

					// 仅需要记录UniqueID
				}
			}
			else if (SharedNode->NodeType == EScriptStateTreeNodeType::State)
			{
				UStateTreeStateNode* StateNode = CastChecked<UStateTreeStateNode>(SharedNode);
				CurrentUniqueID = StateNode->StateInstance->UniqueID;
				// 仅需要记录UniqueID
			}
		}
		else if (CurrentNode->NodeType == EScriptStateTreeNodeType::Slot)
		{
			return;
		}


		// 记录StateTreeView内，每个节点所属的EventSlot
		const FStateEventSlotInfo& EventSlotInfo = CurrentNode->EventSlotInfo;
		ScriptArchetype->EventSlotMap.Add(EventSlotInfo.UID, CurrentUniqueID);

		if (bIsFirstNode)
		{
			ScriptArchetype->RootNodeID = CurrentUniqueID;
			bIsFirstNode = false;
		}
	});

	TraverseAllGraph([ScriptArchetype, this](UEdGraph* CurrentEdGraph) {
		if (CurrentEdGraph && CurrentEdGraph->Nodes.Num() > 0 && CurrentEdGraph->Nodes[0])
		{
			// 因为Graph的第一个Node是EntryNode，不需要存储。
			UGraphAbilityNode* EntryNode = Cast<UGraphAbilityNode>(CurrentEdGraph->Nodes[0]);
			
			TraverseGraphNodeRecursive(nullptr, EntryNode, [ScriptArchetype](UGraphAbilityNode* PrevNode, UGraphAbilityNode* CurrentNode) {
				UStateAbilityNodeBase* CurAbilityNode = Cast<UStateAbilityNodeBase>(CurrentNode->NodeInstance);
				ScriptArchetype->ActionMap.Add(CurAbilityNode->UniqueID, CurAbilityNode);

				if (IsValid(PrevNode))
				{
					// 记录查找前一个节点的所有EventSlot，并根据Pin的名称查找CurNode所属的EventSlot

					UStateAbilityNodeBase* PrevAbilityNode = Cast<UStateAbilityNodeBase>(PrevNode->NodeInstance);

					// @TODO：存在浪费
					TMap<FName, FConfigVars_EventSlot> EventSlots = PrevAbilityNode->GetEventSlots();

					if (FConfigVars_EventSlot* EventSlotPtr = EventSlots.Find(CurrentNode->Pins[0]->GetFName()))
					{
						ScriptArchetype->EventSlotMap.Add(EventSlotPtr->UID, CurAbilityNode->UniqueID);
					}
				}

				// 处理NetDeltas优化协议
				switch (CurAbilityNode->NodeRepMode)
				{
				case ENodeRepMode::Local: {
					//ScriptArchetype->NetDeltasProtocal.AutonomousProxy_ActionSet.Add(CurAbilityNode->UniqueID);
					break;
				}
				case ENodeRepMode::All: {
					//ScriptArchetype->NetDeltasProtocal.AutonomousProxy_ActionSet.Add(CurAbilityNode->UniqueID);
					//ScriptArchetype->NetDeltasProtocal.SimulatedProxy_ActionSet.Add(CurAbilityNode->UniqueID);
					break;
				}
				}

			});
		}
		});
}

void UStateAbilityScriptEditorData::TraverseAllGraph(TFunction<void(UEdGraph*)> InProcessor)
{
	if (TreeRoots.Num() == 0)
	{
		return;
	}

	TraverseGraphRecursive(TreeRoots[0], InProcessor);
}

void UStateAbilityScriptEditorData::TraverseAllNodeInstance(TFunction<void(UStateAbilityNodeBase*)> InProcessor)
{
	if (TreeRoots.Num() == 0)
	{
		return;
	}

	TraverseStateTreeNodeRecursive(TreeRoots[0], [Processor = InProcessor](UStateTreeBaseNode* CurrentNode) {
		if (CurrentNode->NodeType == EScriptStateTreeNodeType::State)
		{
			Processor(CastChecked<UStateTreeStateNode>(CurrentNode)->StateInstance);
		}
	});

	TraverseAllGraph([this, Processor = InProcessor](UEdGraph* CurrentEdGraph){
		if (CurrentEdGraph && CurrentEdGraph->Nodes.Num() > 0 && CurrentEdGraph->Nodes[0] && CurrentEdGraph->Nodes[0]->Pins.Num() > 0 && CurrentEdGraph->Nodes[0]->Pins[0]->LinkedTo.Num() > 0)
		{
			UGraphAbilityNode* GraphNode = Cast<UGraphAbilityNode>(CurrentEdGraph->Nodes[0]->Pins[0]->LinkedTo[0]->GetOwningNode());	// 因为Graph的第一个Node是EntryNode，不需要存储。
			TraverseGraphNodeRecursive(nullptr, GraphNode, [Processor = Processor](UGraphAbilityNode* PrevNode, UGraphAbilityNode* CurrentNode){
				UStateAbilityNodeBase* AbilityNode = Cast<UStateAbilityNodeBase>(CurrentNode->NodeInstance);
				Processor(AbilityNode);
			});
		}
	});

}

void UStateAbilityScriptEditorData::TraverseStateTreeNodeRecursive(UStateTreeBaseNode* CurrentNode, TFunction<void(UStateTreeBaseNode* CurrentNode)> InProcessor)
{
	if (!IsValid(CurrentNode))
	{
		return;
	}

	InProcessor(CurrentNode);

	for (UStateTreeBaseNode* Child : CurrentNode->Children)
	{
		TraverseStateTreeNodeRecursive(Child, InProcessor);
	}
}

void UStateAbilityScriptEditorData::TraverseGraphNodeRecursive(UGraphAbilityNode* PrevNode, UGraphAbilityNode* CurrentNode, TFunction<void(UGraphAbilityNode* PrevNode, UGraphAbilityNode* CurrentNode)> InProcessor)
{
	// PrevNode 允许是空的

	if (!IsValid(CurrentNode))
	{
		InProcessor(PrevNode, nullptr);
		return;
	}

	InProcessor(PrevNode, CurrentNode);

	for (int32 PinIdx = 0; PinIdx < CurrentNode->Pins.Num(); PinIdx++)
	{
		UEdGraphPin* Pin = CurrentNode->Pins[PinIdx];
		if (Pin->Direction != EGPD_Output)
		{
			continue;
		}
		for (int32 LinkId = 0; LinkId < Pin->LinkedTo.Num(); ++LinkId)
		{
			UGraphAbilityNode* NextNode = Cast<UGraphAbilityNode>(Pin->LinkedTo[LinkId]->GetOwningNode());
			TraverseGraphNodeRecursive(CurrentNode, NextNode, InProcessor);
		}
	}
}

void UStateAbilityScriptEditorData::TraverseGraphRecursive(UStateTreeBaseNode* CurrentNode, TFunction<void(UEdGraph*)> InProcessor)
{
	if (!IsValid(CurrentNode))
	{
		return;
	}

	if (CurrentNode->NodeType == EScriptStateTreeNodeType::Action)
	{
		UStateTreeActionNode* ActionNode = CastChecked<UStateTreeActionNode>(CurrentNode);

		InProcessor(ActionNode->EdGraph);
	}

	for (UStateTreeBaseNode* Child : CurrentNode->Children)
	{
		TraverseGraphRecursive(Child, InProcessor);
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
	UStateAbilityScriptArchetype* ScriptArchetype = this->GetTypedOuter<UStateAbilityScriptArchetype>();
	UPackage* Package = ScriptArchetype->GetPackage();

	TraverseAllNodeInstance([ScriptArchetype, &NodeInstances](UStateAbilityNodeBase* StateAbilityNode) {
		NodeInstances.Add(StateAbilityNode);
		//if (UConfigVarsData* ConfigVarsData = FConfigVarsReader::FindOrLoadLazyData(ScriptArchetype, StateAbilityNode->GetDataClass(), StateAbilityNode->UniqueID, true))
		//{
		//	NodeInstances.Add(ConfigVarsData);
		//}
	});
}

bool UStateAbilityScriptEditorData::CanRemoveNestedObject(UObject* TestObject)
{
	return !TestObject->IsA(UEdGraphNode::StaticClass()) &&
		!TestObject->IsA(UEdGraph::StaticClass()) &&
		!TestObject->IsA(UEdGraphSchema::StaticClass()) &&
		!TestObject->IsA(UConfigVarsLinker::StaticClass()) &&
		!TestObject->IsA(UStateAbilityScriptEditorData::StaticClass()) &&
		!TestObject->IsA(UStateAbilityScriptClass::StaticClass()) &&
		!TestObject->IsA(UStateAbilityScript::StaticClass());
}

void UStateAbilityScriptEditorData::ResetObjectOwner(UObject* NewOwner, UObject* Object, ERenameFlags AdditionalFlags)
{
	if (Object && NewOwner && Object->GetOuter()->GetClass() != NewOwner->GetClass())
	{
		// 1. 不能ResetLoader，否则Linker会被清空。
		// 2. 不要创建重定向。
		Object->Rename(nullptr, NewOwner, AdditionalFlags | REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);
	}
}

UStateTreeBaseNode& UStateAbilityScriptEditorData::AddRootState()
{
	UStateTreeBaseNode* RootNode = NewObject<UStateTreeBaseNode>(this, FName(), RF_Transactional);
	check(RootNode);
	RootNode->Name = FName(TEXT("Root"));
	RootNode->NodeType = EScriptStateTreeNodeType::Slot;
	RootNode->EventSlotInfo.SlotType = EStateEventSlotType::ActionEvent;
	RootNode->EventSlotInfo.EventName = FName(TEXT("Root"));
	TreeRoots = { RootNode };
	return *RootNode;
}