#include "Node/GraphAbilityNode_Action.h"

#include "Node/StateAbilityEditorTypes.h"
#include "Component/StateAbility/StateAbilityAction.h"
#include "Component/StateAbility/Script/StateAbilityScript.h"
#include "Component/StateAbility/Script/StateAbilityScriptArchetype.h"

#include "UObject/SavePackage.h"
#include "UObject/LinkerLoad.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "GraphAbilityNode_Action"

void UGraphAbilityNode_Action::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UStateAbilityEditorTypes::PinCategory_Action, TEXT("In"));

	UStateAbilityAction* ActionNode = Cast<UStateAbilityAction>(NodeInstance);
	if (!ActionNode)
	{
		return;
	}

	TMap<FName, FConfigVars_EventSlot> EventSlots = ActionNode->GetEventSlots();
	for (auto& EventPair : EventSlots)
	{
		CreatePin(EGPD_Output, UStateAbilityEditorTypes::PinCategory_Action, EventPair.Key);
	}
}

FText UGraphAbilityNode_Action::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	UStateAbilityAction* MyNode = Cast<UStateAbilityAction>(NodeInstance);
	if (MyNode->DisplayName.ToString().Len() > 0)
	{
		return FText::FromName(MyNode->DisplayName);
	}

	return FText::FromString(FName::NameToDisplayString(StateAbilityClassUtils::ClassToString(GetNodeClass()), false));
}

FText UGraphAbilityNode_Action::GetTooltipText() const
{
	return LOCTEXT("SASNodeTooltip", "This is a Action Node");
}

FString UGraphAbilityNode_Action::GetNodeName() const
{
	return TEXT("Action Node");
}

UEdGraphPin* UGraphAbilityNode_Action::GetInputPin() const
{
	return Pins[0];
}


UEdGraphPin* UGraphAbilityNode_Action::GetOutputPin() const
{
	return Pins[1];
}

UClass* UGraphAbilityNode_Action::GetNodeClass() const
{
	return NodeClass;
}

void UGraphAbilityNode_Action::InitializeNode(UEdGraph* InGraph)
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

void UGraphAbilityNode_Action::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == UStateAbilityNodeBase::DynamicEventSlotBagName)
	{
		OnUpdateGraphNode.Broadcast();
	}
}


#undef LOCTEXT_NAMESPACE