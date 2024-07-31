#include "Node/SASGraphNode_Action.h"

#include "Node/SAEditorTypes.h"
#include "Component/StateAbility/StateAbilityAction.h"
#include "Component/StateAbility/Script/StateAbilityScript.h"
#include "Component/StateAbility/Script/StateAbilityScriptArchetype.h"

#include "UObject/SavePackage.h"
#include "UObject/LinkerLoad.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "SASGraphNode_Action"

void USASGraphNode_Action::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, USASEditorTypes::PinCategory_Action, TEXT("In"));

	UStateAbilityAction* ActionNode = Cast<UStateAbilityAction>(NodeInstance);
	if (!ActionNode)
	{
		return;
	}

	TMap<FName, FConfigVars_EventSlot> EventSlots = ActionNode->GetEventSlots();
	for (auto& EventPair : EventSlots)
	{
		CreatePin(EGPD_Output, USASEditorTypes::PinCategory_Action, EventPair.Key);
	}
}

FText USASGraphNode_Action::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	UStateAbilityAction* MyNode = Cast<UStateAbilityAction>(NodeInstance);
	if (MyNode->DisplayName.ToString().Len() > 0)
	{
		return FText::FromName(MyNode->DisplayName);
	}

	return FText::FromString(FName::NameToDisplayString(SASClassUtils::ClassToString(GetNodeClass()), false));
}

FText USASGraphNode_Action::GetTooltipText() const
{
	return LOCTEXT("SASNodeTooltip", "This is a Action Node");
}

FString USASGraphNode_Action::GetNodeName() const
{
	return TEXT("Action Node");
}

UEdGraphPin* USASGraphNode_Action::GetInputPin() const
{
	return Pins[0];
}


UEdGraphPin* USASGraphNode_Action::GetOutputPin() const
{
	return Pins[1];
}

UClass* USASGraphNode_Action::GetNodeClass() const
{
	return NodeClass;
}

void USASGraphNode_Action::InitializeNode(UEdGraph* InGraph)
{
	// 加入ScriptArchetype，方便后续清扫，不用担心序列化问题，因为没有RF_Standalone标记，所以这里是根据引用序列化的。
	UStateAbilityScriptArchetype* ScriptArchetype = InGraph->GetTypedOuter<UStateAbilityScriptArchetype>();
	UStateAbilityNodeBase* BaseNode = UStateAbilityNodeBase::CreateInstance<UStateAbilityNodeBase>(Cast<UStateAbilityScript>(ScriptArchetype->GeneratedScriptClass->GetDefaultObject(false)), NodeClass);
	NodeInstance = BaseNode;

	static const FName NAME_DisplayName = "DisplayName";
	const FString& DisplayName = NodeClass->GetMetaData(NAME_DisplayName);
	if (!DisplayName.IsEmpty())
	{
		BaseNode->DisplayName = FName(DisplayName);
	}
}

void USASGraphNode_Action::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == UStateAbilityNodeBase::DynamicEventSlotBagName)
	{
		OnUpdateGraphNode.Broadcast();
	}
}


#undef LOCTEXT_NAMESPACE