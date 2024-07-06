#include "Component/Mover/CFrameMovementContext.h"

#include "Component/CFrameMoverComponent.h"

void FCFrameMovementContext::Init(UCFrameMoverComponent* InMoverComp, float InDeltaTime, uint32 InRCF, uint32 InICF)
{
	DeltaTime = InDeltaTime;
	RCF = InRCF;
	ICF = InICF;

	MoverComp = InMoverComp;
	UpdatedComponent = MoverComp->GetUpdatedComponent();
	UpdatedPrimitive = MoverComp->GetPrimitiveComponent();
	MoveStateAdapter = MoverComp->GetMovementConfig().MoveStateAdapter;
}

void FCFrameMovementContext::ResetFrameData()
{
	DeltaTime = 0.0f;
	RCF = 0;
	ICF = 0;

	MoverComp = nullptr;
	UpdatedComponent = nullptr;
	UpdatedPrimitive = nullptr;
	MoveStateAdapter = nullptr;
	CombinedMove.Clear();
}

void FCFrameMovementContext::ResetAllData()
{
	ResetFrameData();

	PersistentDataBuffer.Empty();
}

FStructView FCFrameMovementContext::GetPersistentData(FName Key)
{
	return PersistentDataBuffer.FindOrAdd(Key).Data;
}
void FCFrameMovementContext::RemovePersistentData(FName Key)
{
	PersistentDataBuffer.Remove(Key);
}
void FCFrameMovementContext::InvalidPersistentData(FName Key)
{
	PersistentDataBuffer.FindOrAdd(Key).bIsHidden = true;
}
bool FCFrameMovementContext::HasValidPersistentData(FName Key)
{
	return !PersistentDataBuffer.FindOrAdd(Key).bIsHidden;
}