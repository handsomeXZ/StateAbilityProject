// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "InputAction.h"

#include "CommandFrameInput.generated.h"

USTRUCT()
struct FCommandFrameInputAtom
{
	GENERATED_BODY()
public:
	FCommandFrameInputAtom() {}
	FCommandFrameInputAtom(float InLastTriggeredWorldTime, float InElapsedProcessedTime, float InElapsedTriggeredTime, const UInputAction* InInputAction, ETriggerEvent InTriggerEvent, const FInputActionValue& InValue);
	FCommandFrameInputAtom(const FInputActionInstance& ActionData);

	// @TODO：在序列化数据前，遍历所有Atom并执行。这种能提高多少速度？
	// 尝试将存储的数据转为a single memory blob，或者说是POD？
	void BulkMemoryBlob(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	bool Serialize(FArchive& Ar);
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	// 对于有可靠地址的InputAction，网络同步时是直接传递的ObjectPath，可以通过LoadObject查到。
	// 但是直接传输FString路径是有开销的 (FName受限于NamePool，在 C/S 不相同)，所以理想情况下，可以在每次序列化数据时，查找FNetworkGUID，如果不存在，则继续序列化路径，并利用 1个bit 来记录类型。
	// 当服务器收到Input包后，会解析是否存在FNetworkGUID，如果不存在，则会在Delta包内回传InputAction，帮助建立FNetworkGUID映射。
	union
	{
		const UInputAction* InputAction;
		FNetworkGUID IA_NetGUID;
	};

	// Trigger state
	ETriggerEvent TriggerEvent = ETriggerEvent::None;

	// The last time that this evaluated to a Triggered State
	float LastTriggeredWorldTime = 0.0f;

	// Total trigger processing/evaluation time (How long this action has been in event Started, Ongoing, or Triggered
	float ElapsedProcessedTime = 0.f;

	// Triggered time (How long this action has been in event Triggered only)
	float ElapsedTriggeredTime = 0.f;

	EInputActionValueType ValueType;

	FVector Value;

	// 不需要这个，因为Client经过验证后的都符合。
	// ETriggerEventInternal TriggerEventInternal = ETriggerEventInternal(0);
};

template<>
struct TStructOpsTypeTraits<FCommandFrameInputAtom> : public TStructOpsTypeTraitsBase2<FCommandFrameInputAtom>
{
	enum
	{
		WithSerializer		= true,
		WithNetSerializer	= true,
	};
};

USTRUCT(BlueprintType)
struct FCommandFrameInputActionInstance : public FInputActionInstance
{
	GENERATED_BODY()
public:
	FCommandFrameInputActionInstance();
	FCommandFrameInputActionInstance(const FCommandFrameInputAtom& InputAtom);

};