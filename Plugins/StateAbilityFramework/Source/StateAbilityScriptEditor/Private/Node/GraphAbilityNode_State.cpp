#include "Node/GraphAbilityNode_State.h"

#include "Node/StateAbilityEditorTypes.h"
#include "Component/StateAbility/StateAbilityState.h"
#include "Component/StateAbility/Script/StateAbilityScript.h"
#include "Component/StateAbility/Script/StateAbilityScriptArchetype.h"
#include "StateAbilityScriptEditorData.h"
#include "StateTree/ScriptStateTreeView.h"

#include "UObject/SavePackage.h"
#include "UObject/LinkerLoad.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "GraphAbilityNode_State"

void UGraphAbilityNode_State::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UStateAbilityEditorTypes::PinCategory_Exec, TEXT("In"));

	UStateAbilityState* StateNode = Cast<UStateAbilityState>(NodeInstance);
	if (!StateNode)
	{
		return;
	}

	ReGeneratePins();
}

FText UGraphAbilityNode_State::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	UStateAbilityState* StateNode = Cast<UStateAbilityState>(NodeInstance);
	if (IsValid(StateNode) && StateNode->DisplayName.ToString().Len() > 0)
	{
		return FText::FromName(StateNode->DisplayName);
	}

	return FText::FromString(FName::NameToDisplayString(StateAbilityClassUtils::ClassToString(GetNodeClass()), false));
}

FText UGraphAbilityNode_State::GetTooltipText() const
{
	return LOCTEXT("GraphAbilityNodeTooltip", "This is a State Node");
}

FString UGraphAbilityNode_State::GetNodeName() const
{
	return TEXT("State Node");
}

UEdGraphPin* UGraphAbilityNode_State::GetInputPin() const
{
	return Pins[0];
}


UEdGraphPin* UGraphAbilityNode_State::GetOutputPin() const
{
	return Pins[1];
}

UClass* UGraphAbilityNode_State::GetNodeClass() const
{
	return NodeClass;
}

void UGraphAbilityNode_State::InitializeNode(UEdGraph* InGraph)
{
	// 加入ScriptArchetype，方便后续清扫，不用担心序列化问题，因为没有RF_Standalone标记，所以这里是根据引用序列化的。
	UStateAbilityScriptArchetype* ScriptArchetype = InGraph->GetTypedOuter<UStateAbilityScriptArchetype>();
	UStateAbilityNodeBase* BaseNode = UStateAbilityNodeBase::CreateInstance<UStateAbilityNodeBase>(ScriptArchetype, NodeClass);
	NodeInstance = BaseNode;

	static const FName NAME_DisplayName = "DisplayName";
	const FString& DisplayName = NodeClass->GetMetaData(NAME_DisplayName);
	if (!DisplayName.IsEmpty())
	{
		BaseNode->DisplayName = FName(DisplayName);
	}

	RootStateNode = NewObject<UStateTreeStateNode>(this, FName(), RF_Transactional);
	check(RootStateNode);
	RootStateNode->Name = FName(DisplayName);
	RootStateNode->NodeType = EScriptStateTreeNodeType::State;
	RootStateNode->EventSlotInfo.SlotType = EStateEventSlotType::StateEvent;
	RootStateNode->EventSlotInfo.EventName = FName(TEXT("Root"));

	RootStateNode->StateInstance = Cast<UStateAbilityState>(NodeInstance);

	StateTreeViewModel = MakeShared<FScriptStateTreeViewModel>(Cast<UStateAbilityScriptEditorData>(ScriptArchetype->EditorData));
	StateTreeViewModel->Init(RootStateNode);

	RootStateNode->Init(StateTreeViewModel);

	// 绑定StateTreeView相关的委托
	StateTreeViewModel->OnNodeAdded_Delegate.AddUObject(this, &UGraphAbilityNode_State::HandleModelNodeAdded);
	StateTreeViewModel->OnNodesMoved_Delegate.AddUObject(this, &UGraphAbilityNode_State::HandleModelNodesMoved);
	StateTreeViewModel->OnDetailsViewChangingProperties_Delegate.AddUObject(this, &UGraphAbilityNode_State::HandleDetailsViewChangingProperties);
	StateTreeViewModel->OnUpdateStateTree_Delegate.AddUObject(this, &UGraphAbilityNode_State::HandleUpdateStateTree);
}

void UGraphAbilityNode_State::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == UStateAbilityNodeBase::DynamicEventSlotBagName)
	{
		// @TODO：等待加入动态分支的功能
	}
}

void UGraphAbilityNode_State::PostEditUndo()
{
	Super::PostEditUndo();
	
	StateTreeViewModel->UpdateStateNodePin();
}

TSharedPtr<FScriptStateTreeViewModel> UGraphAbilityNode_State::GetOrCreateStateTreeViewModel()
{
	if (!StateTreeViewModel.IsValid())
	{
		UStateAbilityScriptArchetype* ScriptArchetype = GetGraph()->GetTypedOuter<UStateAbilityScriptArchetype>();
		StateTreeViewModel = MakeShared<FScriptStateTreeViewModel>(Cast<UStateAbilityScriptEditorData>(ScriptArchetype->EditorData));

		StateTreeViewModel->Init(RootStateNode);

		RootStateNode->OnUpdateNode(StateTreeViewModel.ToSharedRef());
	}


	return StateTreeViewModel;
}

FStackedPinInfo* UGraphAbilityNode_State::FindStackedPinInfo(UEdGraphPin* Pin)
{
	return StackedPins.Find(Pin->PinId);
}

void UGraphAbilityNode_State::ReGeneratePins()
{
	UStateAbilityScriptArchetype* ScriptArchetype = GetGraph()->GetTypedOuter<UStateAbilityScriptArchetype>();
	TSharedPtr<FAbilityScriptViewModel> ScriptViewModel = Cast<UStateAbilityScriptEditorData>(ScriptArchetype->EditorData)->GetScriptViewModel();


	// @TODO：目前仅会生成OutPins
	

	// 创建Link历史记录用于恢复现场
	TMap<UStateTreeBaseNode*, TArray<UEdGraphPin*>> LinkedToHistory;
	TArray<UEdGraphPin*> OldPins = Pins;

	// @TODO：直接移除Pins是否会导致已连接的Connection断开？
	for (UEdGraphPin* Pin : OldPins)
	{
		if (Pin->Direction == EGPD_Output)
		{
			// 查询是否是RootEventSlot所映射的pin，并记录
			if (TObjectPtr<UStateTreeBaseNode>* EventNode = RootEventPins.Find(Pin->PinId))
			{
				LinkedToHistory.Add(*EventNode, Pin->LinkedTo);
			}

			// 查询是否是折叠后的EventSlot所映射的pin，并记录
			if (FStackedPinInfo* StackedPinInfo = StackedPins.Find(Pin->PinId))
			{
				LinkedToHistory.Add(StackedPinInfo->ConnectedEventNode, Pin->LinkedTo);
			}

			ScriptViewModel->ClearRecordedEventslot(Pin);
			
			RemovePin(Pin);
		}
	}


	StackedPins.Empty();
	RootEventPins.Empty();
	// ThenExec_Event 是特殊的OutPin

	// 已经在StateTreeView中被占用的EventSlotPin会被进行堆叠
	UStateAbilityState* StateInstance = Cast<UStateAbilityState>(NodeInstance);
	TMap<FName, FConfigVars_EventSlot> EventSlots = StateInstance->GetEventSlots();
	for (TObjectPtr<UStateTreeBaseNode> ChildNode : RootStateNode->Children)
	{
		if (ChildNode->NodeType != EScriptStateTreeNodeType::Slot)
		{
			FStackedPinInfo StackPinInfo;
			StackPinInfo.RootEventNode = ChildNode;
			StackPinInfo.StackedEventNode.Add(ChildNode, ChildNode->EventSlotInfo.EventName);

			// 注意，可能存在最终无Slot结尾的情况（例如末尾的Node不存在EventSlot），此时PinName为None。
			CreateStackedPins(ChildNode, StackPinInfo, LinkedToHistory);
			continue;
		}

		if (FConfigVars_EventSlot* EventSlot = EventSlots.Find(ChildNode->EventSlotInfo.EventName))
		{
			UEdGraphPin* NewRootPin = CreatePin(EGPD_Output, UStateAbilityEditorTypes::PinCategory_Event, ChildNode->EventSlotInfo.EventName);

			// 还原 LinkedTo
			if (auto* LinkedToArray = LinkedToHistory.Find(ChildNode))
			{
				for (UEdGraphPin* LinkedToPin : *LinkedToArray)
				{
					NewRootPin->MakeLinkTo(LinkedToPin);
				}
			}

			ScriptViewModel->RecordEventSlotWithPin(NewRootPin, *EventSlot);
			RootEventPins.Add(NewRootPin->PinId, ChildNode);
		}
	}
}

void UGraphAbilityNode_State::CreateStackedPins(UStateTreeBaseNode* Node, FStackedPinInfo PrevStackedPinInfo, TMap<UStateTreeBaseNode*, TArray<UEdGraphPin*>>& LinkedToHistory)
{
	UStateAbilityScriptArchetype* ScriptArchetype = GetGraph()->GetTypedOuter<UStateAbilityScriptArchetype>();
	TSharedPtr<FAbilityScriptViewModel> ScriptViewModel = Cast<UStateAbilityScriptEditorData>(ScriptArchetype->EditorData)->GetScriptViewModel();

	switch (Node->NodeType)
	{
	case EScriptStateTreeNodeType::State:{
		UStateTreeStateNode* TreeStateNode = Cast<UStateTreeStateNode>(Node);
		UStateAbilityState* StateInstance = Cast<UStateAbilityState>(TreeStateNode->StateInstance);
		TMap<FName, FConfigVars_EventSlot> EventSlots = StateInstance->GetEventSlots();

		for (UStateTreeBaseNode* ChildNode : Node->Children)
		{
			if (ChildNode->NodeType != EScriptStateTreeNodeType::Slot)
			{
				FStackedPinInfo NewStackPinInfo = PrevStackedPinInfo;

				NewStackPinInfo.StackedEventNode.Add(ChildNode, ChildNode->EventSlotInfo.EventName);
				CreateStackedPins(ChildNode, NewStackPinInfo, LinkedToHistory);
				continue;
			}
			else if (FConfigVars_EventSlot* EventSlot = EventSlots.Find(ChildNode->EventSlotInfo.EventName))
			{
				UEdGraphPin* NewStackedPin = CreatePin(EGPD_Output, UStateAbilityEditorTypes::PinCategory_Event, ChildNode->EventSlotInfo.EventName);

				if (auto* LinkedToArray = LinkedToHistory.Find(ChildNode))
				{
					for (UEdGraphPin* LinkedToPin : *LinkedToArray)
					{
						NewStackedPin->MakeLinkTo(LinkedToPin);
					}
				}

				ScriptViewModel->RecordEventSlotWithPin(NewStackedPin, *EventSlot);
				PrevStackedPinInfo.ConnectedEventNode = ChildNode;
				StackedPins.Add(NewStackedPin->PinId, PrevStackedPinInfo);
			}
		}
		break;
	}
	case EScriptStateTreeNodeType::Shared: {
		// @TODO：目前还未支持Shared的折叠Pin，但理论上得有。
		check(0);
		break;
	}
	default: {
		check(0);
	}
	}

	
}

void UGraphAbilityNode_State::HandleModelNodeAdded(UStateTreeBaseNode* ParentNode, UStateTreeBaseNode* NewNode)
{
	ReGeneratePins();
}
void UGraphAbilityNode_State::HandleModelNodesMoved(const TSet<UStateTreeBaseNode*>& AffectedParents, const TSet<UStateTreeBaseNode*>& MovedNodes)
{
	ReGeneratePins();
}
void UGraphAbilityNode_State::HandleDetailsViewChangingProperties(UObject* SelectedObject)
{
	ReGeneratePins();
}
void UGraphAbilityNode_State::HandleUpdateStateTree()
{
	ReGeneratePins();
}
#undef LOCTEXT_NAMESPACE