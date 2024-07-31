// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Component/StateAbility/StateAbilityAction.h"
#include "StateAbilityBranch.generated.h"


UCLASS(BlueprintType, meta = (DisplayName = "If", InheritedDataStruct, DataStruct = "/Script/StateAbilityScriptRuntime.ConfigVars_Bool"))
class STATEABILITYSCRIPTRUNTIME_API UStateAbilityCondition : public UStateAbilityAction
{
	GENERATED_UCLASS_BODY()
public:
	virtual void OnExecute(FActionExecContext& Conext) const override;

protected:
	UPROPERTY(meta = (DisplayName = "True"))
	FConfigVars_EventSlot True_Event;

	UPROPERTY(meta = (DisplayName = "False"))
	FConfigVars_EventSlot False_Event;
};
