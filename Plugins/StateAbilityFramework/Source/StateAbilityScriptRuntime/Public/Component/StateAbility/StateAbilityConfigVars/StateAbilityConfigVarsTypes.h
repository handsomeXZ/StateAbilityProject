#pragma once

#include "CoreMinimal.h"

#include "InstancedStruct.h"
#include "Templates/ValueOrError.h"

#include "StateAbilityConfigVarsTypes.generated.h"


USTRUCT()
struct FConfigVars_EventSlot
{
	GENERATED_BODY()
public:
	FConfigVars_EventSlot();
	FConfigVars_EventSlot(const FConfigVars_EventSlot& Other);
	FConfigVars_EventSlot& operator=(const FConfigVars_EventSlot& Other);

	UPROPERTY(VisibleAnywhere, Category = "配置")
	FGuid UID;
};

USTRUCT(BlueprintType)
struct FConfigVars_EventSlotBag
{
	GENERATED_BODY()
public:
	FConfigVars_EventSlotBag() { EventSlots.Empty(); }

	UPROPERTY()
	TMap<FName, FConfigVars_EventSlot> EventSlots;
};

USTRUCT()
struct FConfigVars_Bool
{
	GENERATED_BODY()
public:
	FConfigVars_Bool() {}

	virtual bool GetValue() const { return false; }
};

USTRUCT(BlueprintType, DisplayName = "BoolValue")
struct FConfigVars_BoolValue : public FConfigVars_Bool
{
	GENERATED_BODY()
public:
	FConfigVars_BoolValue() {}

	virtual bool GetValue() const override { return Value; }

	UPROPERTY(EditAnywhere, Category = "配置")
	bool Value = false;
};