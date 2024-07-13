#include "Component/Mover/CFrameMovementContext.h"

#include "CommandFrameManager.h"
#include "Component/CFrameMoverComponent.h"
#include "Component/Mover/CFrameMoveStateAdapter.h"

void FCFrameMovementContext::Init(UCFrameMoverComponent* InMoverComp, float InDeltaTime, uint32 InRCF, uint32 InICF)
{
	DeltaTime = InDeltaTime;
	RCF = InRCF;
	ICF = InICF;

	MoverComp = InMoverComp;
	UpdatedComponent = MoverComp->GetUpdatedComponent();
	UpdatedPrimitive = MoverComp->GetPrimitiveComponent();
	MoveStateAdapter = MoverComp->GetMovementConfig().MoveStateAdapter;

	CFrameManager = MoverComp->GetCommandFrameManager();
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
	CFrameManager = nullptr;
	CombinedMove.Clear();
}

void FCFrameMovementContext::ResetAllData()
{
	ResetFrameData();

	PersistentDataBuffer.Empty();
}

bool FCFrameMovementContext::IsDataValid()
{
	return IsValid(MoverComp) && 
		IsValid(MoveStateAdapter) &&
		IsValid(CFrameManager);
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