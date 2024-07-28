// Fill out your copyright notice in the Description page of Project Settings.
#include "Component/StateAbility/StateAbilityState.h"

#include "Component/StateAbility/Script/StateAbilityScript.h"


UStateAbilityState::UStateAbilityState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsPersistent(false)
	, AttributeStruct(nullptr)
{
	AttributeStruct = FAttribute_State::StaticStruct();
}

void UStateAbilityState::Initialize(FAttributeEntityBuildParam& BuildParam)
{
	BuildParam.ArchetypeFragment.Add(AttributeStruct);
	AttributeBag.Initialize(BuildParam);

	FAttribute_State& StateAttribute = AttributeBag.Get<FAttribute_State>();
	StateAttribute.Stage = EStateAbilityStateStage::Initialized;

	OnInitialize();

	EnqueueEvent(OnInitialize_Event);
}

void UStateAbilityState::Activate()
{
	FAttribute_State& StateAttribute = AttributeBag.Get<FAttribute_State>();
	StateAttribute.Stage = EStateAbilityStateStage::Activated;

	OnActivate();

	EnqueueEvent(OnActivate_Event);
}

void UStateAbilityState::Deactivate()
{
	FAttribute_State& StateAttribute = AttributeBag.Get<FAttribute_State>();
	StateAttribute.Stage = EStateAbilityStateStage::Unactivated;

	OnDeactivate();

	EnqueueEvent(OnDeactivate_Event);
}

void UStateAbilityState::TimerEvent()
{
	OnTimerEvent();
}

void UStateAbilityState::FixedTick(float DeltaTime, uint32 RCF, uint32 ICF)
{
	OnFixedTick(DeltaTime, RCF, ICF);
}
