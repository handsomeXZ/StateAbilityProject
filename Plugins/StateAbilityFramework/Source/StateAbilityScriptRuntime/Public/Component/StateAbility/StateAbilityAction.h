// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Component/StateAbility/StateAbilityNodeBase.h"
#include "StateAbilityAction.generated.h"

class UStateAbilityScript;

struct FActionExecContext
{
	enum class EResult : uint8
	{
		Success,
		Error,
		Cancel,
	};
	FActionExecContext(UStateAbilityScript* InScript);

	void EnqueueImmediateEvent(const FConfigVars_EventSlot& Event);

	void EnqueuePendingEvent(const FConfigVars_EventSlot& Event);

	EResult Result;
	UStateAbilityScript* Script;
private:
	friend class UStateAbilityScript;

	TArray<FConfigVars_EventSlot> ImmediateEvents;
};

UCLASS(Abstract)
class STATEABILITYSCRIPTRUNTIME_API UStateAbilityAction : public UStateAbilityNodeBase
{
	GENERATED_UCLASS_BODY()
public:
	void Execute(FActionExecContext& Conext) const;
	virtual void OnExecute(FActionExecContext& Conext) const {}

private:
	UPROPERTY(meta = (DisplayName = "Exec"))
	FConfigVars_EventSlot Then_Event;
};

// 特殊的Action，在运行时并不会包含任何数据，仅作为标记存在。
UCLASS(BlueprintType, Blueprintable, meta = (DataStruct = "/Script/StateAbilityScriptRuntime.ConfigVars_EventSlot"))
class STATEABILITYSCRIPTRUNTIME_API UStateAbilityEventSlot : public UStateAbilityNodeBase
{
	GENERATED_UCLASS_BODY()
public:
	UStateAbilityEventSlot();
	const FGuid GetUID(UStateAbilityScript* InScript) const;

#if WITH_EDITOR
	const FGuid EditorGetUID(UStateAbilityScript* InScript) const;
#endif
};