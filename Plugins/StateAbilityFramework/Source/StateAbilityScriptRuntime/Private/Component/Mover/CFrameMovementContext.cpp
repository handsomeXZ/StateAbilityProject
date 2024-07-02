#include "Component/Mover/CFrameMovementContext.h"

void FCFrameMovementContext::Init(float InDeltaTime, uint32 InRCF, uint32 InICF)
{
	DeltaTime = InDeltaTime;
	RCF = InRCF;
	ICF = InICF;
}

void FCFrameMovementContext::ResetFrameData()
{
	DeltaTime = 0.0f;
	RCF = 0;
	ICF = 0;

	CombinedMove.Clear();
}

void FCFrameMovementContext::ResetAllData()
{
	ResetFrameData();

	LastFloorResult.Clear();
}