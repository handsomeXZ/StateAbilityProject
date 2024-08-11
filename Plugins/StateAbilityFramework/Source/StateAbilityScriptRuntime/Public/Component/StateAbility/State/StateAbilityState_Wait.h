#pragma once

#include "CoreMinimal.h"

#include "Component/StateAbility/StateAbilityState.h"

#include "StateAbilityState_Wait.generated.h"

UCLASS(BlueprintType, DisplayName = "Delay")
class STATEABILITYSCRIPTRUNTIME_API UStateAbilityState_Wait : public UStateAbilityState
{
	GENERATED_BODY()
public:
	UStateAbilityState_Wait() {}
private:
	UPROPERTY(meta = (DisplayName = "OnFinish"))
	FConfigVars_EventSlot OnFinish_Event;
};
