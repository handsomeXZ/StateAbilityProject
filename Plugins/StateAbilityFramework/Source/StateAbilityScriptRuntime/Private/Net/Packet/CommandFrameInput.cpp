#include "Net/Packet/CommandFrameInput.h"

#include "Serialization/Archive.h"

#include "InputAction.h"

FCommandFrameInputAtom::FCommandFrameInputAtom(float InLastTriggeredWorldTime, float InElapsedProcessedTime, float InElapsedTriggeredTime, const UInputAction* InInputAction, ETriggerEvent InTriggerEvent, const FInputActionValue& InValue)
{
	LastTriggeredWorldTime = InLastTriggeredWorldTime;
	ElapsedProcessedTime = InElapsedProcessedTime;
	ElapsedTriggeredTime = InElapsedTriggeredTime;

	InputAction = InInputAction;
	TriggerEvent = InTriggerEvent;

	ValueType = InValue.GetValueType();
	Value = InValue.Get<FVector>();
}

FCommandFrameInputAtom::FCommandFrameInputAtom(const FInputActionInstance& ActionData)
{
	InputAction = ActionData.GetSourceAction();
	TriggerEvent = ActionData.GetTriggerEvent();

	LastTriggeredWorldTime = ActionData.GetLastTriggeredWorldTime();

	ValueType = ActionData.GetValue().GetValueType();
	Value = ActionData.GetValue().Get<FVector>();

	ElapsedProcessedTime = ActionData.GetElapsedTime();
	ElapsedTriggeredTime = ActionData.GetTriggeredTime();
}

void FCommandFrameInputAtom::BulkMemoryBlob(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	// 借助Connection来处理UObject


}

bool FCommandFrameInputAtom::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		UInputAction* IA = const_cast<UInputAction*>(InputAction);
		Ar << IA;
	}
	else if (Ar.IsLoading())
	{
		UInputAction* IA;
		Ar << IA;
		InputAction = IA;
	}
	Ar << TriggerEvent;
	Ar << LastTriggeredWorldTime;
	Ar << ElapsedProcessedTime;
	Ar << ElapsedTriggeredTime;
	Ar << ValueType;
	Ar << Value;

	return true;
}

bool FCommandFrameInputAtom::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	if (Ar.IsSaving())
	{
		UInputAction* IA = const_cast<UInputAction*>(InputAction);
		Ar << IA;
	}
	else if (Ar.IsLoading())
	{
		UInputAction* IA;
		Ar << IA;
		InputAction = IA;
	}
	Ar << TriggerEvent;
	Ar << LastTriggeredWorldTime;
	Ar << ElapsedProcessedTime;
	Ar << ElapsedTriggeredTime;
	Ar << ValueType;
	Ar << Value;

	bOutSuccess = true;

	return true;
}

//////////////////////////////////////////////////////////////////////////
// FCommandFrameInputActionInstance
FCommandFrameInputActionInstance::FCommandFrameInputActionInstance()
	: Super()
{
	
}

FCommandFrameInputActionInstance::FCommandFrameInputActionInstance(const FCommandFrameInputAtom& InputAtom)
	: Super(InputAtom.InputAction)
{
	TriggerEvent = InputAtom.TriggerEvent;

	LastTriggeredWorldTime = InputAtom.LastTriggeredWorldTime;

	ElapsedProcessedTime = InputAtom.ElapsedProcessedTime;

	ElapsedTriggeredTime = InputAtom.ElapsedTriggeredTime;

	Value = InputAtom.Value;
}