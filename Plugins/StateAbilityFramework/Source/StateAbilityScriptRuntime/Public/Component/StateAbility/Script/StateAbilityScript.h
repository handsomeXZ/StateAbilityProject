#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"

#include "Attribute/AttributeBag.h"
#include "Component/StateAbility/StateAbilityAction.h"
#include "Component/StateAbility/StateAbilityState.h"
#include "Component/StateAbility/Script/StateAbilityScriptArchetype.h"
#include "Component/StateAbility/Script/StateAbilityScriptNetProto.h"

#include "StateAbilityScript.generated.h"

class UStateAbilityComponent;
class UStateAbilityScript;

UENUM()
enum class EStateAbilityScriptStage : uint8
{
	Ready,
	Initialized,
	Activated,
};

UCLASS()
class STATEABILITYSCRIPTRUNTIME_API UStateAbilityScript : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY()
	FAttributeDynamicBag AttributeBag;

	// 待处理的事件
	TArray<uint32> PendingEvents;
	UPROPERTY(Transient)
	TMap<uint32, UStateAbilityState*> StateInstanceMap;
	// 已激活的State
	UPROPERTY(Transient)
	TArray<UStateAbilityState*> ActivtatedState;
	// 已激活的Cue
	//UPROPERTY(Transient)
	//TMap<FGameplayTag, UStateAbilityScriptCue*> ActivtatedScriptCueMap;
	//UPROPERTY(Transient)
	//TObjectPtr<UStateAbilityComponent> OwnerComponent;
	UPROPERTY(Transient)
	EStateAbilityScriptStage Stage;

	// 每帧搜集的Action执行记录队列
	TArray<uint32> ExecutedActionHistoryQueue;

	void Initialize(UStateAbilityComponent* OwnerComp);
	void ActivateScript();
	void DeactivateScript();
	void FixedTick(float DeltaTime, uint32 RCF, uint32 ICF);

	// State Op
	void ActivateState(UStateAbilityState* State);
	void ActivateState(uint32 StateID);
	void DeactivateState(UStateAbilityState* State);
	void DeactivateState(uint32 StateID);
	UStateAbilityState* GetStateInstance(uint32 StateID);
	void EnqueueEvent(const FGuid& EventSlotID);

	// Action Op
	void MarkActionExecuted(const uint32 UniqueID) { ExecutedActionHistoryQueue.Add(UniqueID); }
	void ClearExecutedActionHistoryQueue() { ExecutedActionHistoryQueue.Empty(); }
	bool HasActionExecutedHistory() { return !ExecutedActionHistoryQueue.IsEmpty(); }

	UStateAbilityScriptArchetype* GetScriptArchetype() { return ScriptArchetype; }
	const UStateAbilityScriptArchetype* GetScriptArchetype() const { return ScriptArchetype; }
	FAttributeDynamicBag& GetAttributeBag() { return AttributeBag; }

	void MarkAllDirty();

	virtual bool IsNameStableForNetworking() const override;
	virtual bool IsSupportedForNetworking() const override;

#if WITH_EDITOR
	void SetScriptArchetype(UStateAbilityScriptArchetype* InScriptArchetype)
	{
		ScriptArchetype = InScriptArchetype;
	}
#endif

protected:
	void ExeucteNode(uint32 NodeID);
	void ExecuteActionSequence(UStateAbilityNodeBase* Node);


private:
	friend class UStateAbilityComponent;
	UPROPERTY()
	TObjectPtr<UStateAbilityScriptArchetype> ScriptArchetype;
};
