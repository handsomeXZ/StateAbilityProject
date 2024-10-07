// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"

#include "Attribute/AttributeBag/AttributeBagUtils.h"
#include "Component/StateAbility/StateAbilityNodeBase.h"

#include "StateAbilityState.generated.h"

UENUM()
enum class EStateAbilityStateStage : uint8
{
	Ready,
	Initialized,
	Activated,
	Unactivated,
};

USTRUCT()
struct FAttribute_State : public FAttributeReactiveBagDataBase
{
	GENERATED_BODY()

	REACTIVE_BODY(FAttribute_State);
	REACTIVE_ATTRIBUTE(EStateAbilityStateStage, Stage);
};

UCLASS(Abstract)
class STATEABILITYSCRIPTRUNTIME_API UStateAbilityState : public UStateAbilityNodeBase
{
	GENERATED_UCLASS_BODY()
public:
	/* 生命周期不受父级State的控制 */
	UPROPERTY(EditAnywhere, Category = "特殊", meta = (DisplayName = "持久的"))
	bool bIsPersistent;

	template<typename TStruct>
	void InitializeAttribute();

	void Initialize(FAttributeEntityBuildParam& BuildParam);
	void Activate();
	void Deactivate();
	void TimerEvent();
	void FixedTick(float DeltaTime, uint32 RCF, uint32 ICF);

	virtual void OnInitialize() {}
	virtual void OnActivate() {}
	virtual void OnDeactivate() {}
	virtual void OnTimerEvent() {}
	virtual void OnFixedTick(float DeltaTime, uint32 RCF, uint32 ICF) {}

	
	TConstArrayView<uint32> GetRelatedSubState() const { return RelatedSubState; }

	FAttributeReactiveBag& GetAttributeBag() { return AttributeBag; }
	void MarkAllDirty() { AttributeBag.MarkAllDirty(); }

protected:
	UPROPERTY(meta = (NotDynamicAttributeBag))
	FAttributeReactiveBag AttributeBag;
private:
	friend class UStateAbilityScriptEditorData;

	UPROPERTY(meta = (DisplayName = "OnInitialize"))
	FConfigVars_EventSlot OnInitialize_Event;
	UPROPERTY(meta = (DisplayName = "OnActivate"))
	FConfigVars_EventSlot OnActivate_Event;
	UPROPERTY(meta = (DisplayName = "OnDeactivate"))
	FConfigVars_EventSlot OnDeactivate_Event;

	UPROPERTY()
	TArray<uint32> RelatedSubState;
};

template<typename TStruct>
inline void UStateAbilityState::InitializeAttribute()
{
	AttributeBag.InitializeReactive<TStruct>();
}
