#include "Component/StateAbility/Script/StateAbilityScript.h"

#include "Component/CFrameStateAbilityComponent.h"
#include "Component/StateAbility/Script/StateAbilityScriptArchetype.h"

DEFINE_LOG_CATEGORY_STATIC(LogStateAbilityScript, Log, All);


//////////////////////////////////////////////////////////////////////////
// UStateAbilityScript
UStateAbilityScript::UStateAbilityScript(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	//, OwnerComponent(nullptr)
	, Stage(EStateAbilityScriptStage::Ready)
	, ScriptArchetype(nullptr)
{

}

bool UStateAbilityScript::IsNameStableForNetworking() const
{
	return HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject);		// All ScriptInstance rely on Archetype to create, Is Dynamic.
}

bool UStateAbilityScript::IsSupportedForNetworking() const
{
	return true;		// All Scripts are supported for networking.
}

void UStateAbilityScript::Initialize(UStateAbilityComponent* OwnerComp)
{
	if (Stage >= EStateAbilityScriptStage::Initialized)
	{
		return;
	}

	//OwnerComponent = OwnerComp;
	//ensureAlwaysMsgf(OwnerComponent != nullptr, TEXT("The Owner is not a StateAbilityComponent, which should be an error!!!"));

	// 根据 State 模板创建实例
	for (UStateAbilityState* StateTemplate : ScriptArchetype->StateTemplates)
	{
		UStateAbilityState* StateInstance = NewObject<UStateAbilityState>(this, StateTemplate->GetClass(), NAME_None, EObjectFlags::RF_NoFlags, StateTemplate, false, nullptr);
		StateInstanceMap.Add(StateInstance->UniqueID, StateInstance);
	}

	// 应该在任何Attribute可能发生变化前进行绑定，因为OwnerComponent需要对所有拥有的Attribute进行监听更改来发送。
	//BindAllProvider(OwnerComponent);

	FAttributeEntityBuildParam BuildParam;

	for (auto StatePair : StateInstanceMap)
	{
		StatePair.Value->Initialize(BuildParam);
	}

	Stage = EStateAbilityScriptStage::Initialized;
}

void UStateAbilityScript::ActivateScript()
{
	if (Stage >= EStateAbilityScriptStage::Activated)
	{
		return;
	}

	ExeucteNode(GetScriptArchetype()->RootNodeID);

	Stage = EStateAbilityScriptStage::Activated;
}

void UStateAbilityScript::DeactivateScript()
{
	for (UStateAbilityState* State : ActivtatedState)
	{
		State->Deactivate();
	}

	ActivtatedState.Empty();

}

void UStateAbilityScript::MarkAllDirty()
{
	AttributeBag.MarkAllDirty();
	for (auto& StateInstancePair : StateInstanceMap)
	{
		StateInstancePair.Value->MarkAllDirty();
	}
}

void UStateAbilityScript::ExeucteNode(uint32 NodeID)
{
	if (UStateAbilityState** StateInstancePtr = StateInstanceMap.Find(NodeID))
	{
		// State
		ActivateState(*StateInstancePtr);
	}
	else if (UStateAbilityNodeBase** HeadNode = ScriptArchetype->ActionSequenceMap.Find(NodeID))
	{
		// ActionSequence
		ExecuteActionSequence(*HeadNode);
	}
}

void UStateAbilityScript::ExecuteActionSequence(UStateAbilityNodeBase* Node)
{
	if (IsValid(Node))
	{
		return;
	}

	// 每个Action序列都是一个执行栈，会创建一个新的Context
	FActionExecContext ActionExecContext(this);

	if (UStateAbilityAction* Action = Cast<UStateAbilityAction>(Node))
	{
		Action->Execute(ActionExecContext);
		
		for(auto It = ActionExecContext.ImmediateEvents.rbegin(); It != ActionExecContext.ImmediateEvents.rend(); ++It)
		{
			if (uint32* NodeID = ScriptArchetype->EventSlotMap.Find((*It).UID))
			{
				// 立即执行
				ExeucteNode(*NodeID);
			}
		}
	}
	else if (UStateAbilityEventSlot* EventSlot = Cast<UStateAbilityEventSlot>(Node))
	{
		const FGuid EventSlotID = EventSlot->GetUID(this);
		EnqueueEvent(EventSlotID);
	}
}

void UStateAbilityScript::EnqueueEvent(const FGuid& EventSlotID)
{
	if (uint32* NodeID = ScriptArchetype->EventSlotMap.Find(EventSlotID))
	{
		PendingEvents.Add(*NodeID);
	}
}

void UStateAbilityScript::FixedTick(float DeltaTime, uint32 RCF, uint32 ICF)
{
	ClearExecutedActionHistoryQueue();

	for (uint32 StateID : PendingEvents)
	{
		if (UStateAbilityState** StateInstance = StateInstanceMap.Find(StateID))
		{
			ActivateState(*StateInstance);
		}
	}
	PendingEvents.Empty();

	for (UStateAbilityState* StateInstance : ActivtatedState)
	{
		StateInstance->FixedTick(DeltaTime, RCF, ICF);
	}
}

void UStateAbilityScript::ActivateState(UStateAbilityState* State)
{
	State->Activate();
	ActivtatedState.AddUnique(State);
}

void UStateAbilityScript::ActivateState(uint32 StateID)
{
	if (UStateAbilityState** StateInstance = StateInstanceMap.Find(StateID))
	{
		ActivateState(*StateInstance);
	}
}

void UStateAbilityScript::DeactivateState(UStateAbilityState* State)
{
	State->Deactivate();
	ActivtatedState.Remove(State);

	for (const uint32 SubStateID : State->GetRelatedSubState())
	{
		if (UStateAbilityState** StateInstance = StateInstanceMap.Find(SubStateID))
		{
			DeactivateState(*StateInstance);
		}
	}
}

void UStateAbilityScript::DeactivateState(uint32 StateID)
{
	if (UStateAbilityState** StateInstance = StateInstanceMap.Find(StateID))
	{
		DeactivateState(*StateInstance);
	}
}

UStateAbilityState* UStateAbilityScript::GetStateInstance(uint32 StateID)
{
	if (UStateAbilityState** StateInstance = StateInstanceMap.Find(StateID))
	{
		return *StateInstance;
	}
	return nullptr;
}
