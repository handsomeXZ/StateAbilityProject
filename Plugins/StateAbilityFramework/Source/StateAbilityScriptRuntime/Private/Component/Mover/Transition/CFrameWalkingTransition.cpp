#include "Component/Mover/Transition/CFrameWalkingTransition.h"

#include "Component/Mover/Mode/CFrameWalkingMode.h"
#include "Component/Mover/Mode/CFrameFallingMode.h"

UCFrameMoveModeTransition_WalkToFall::UCFrameMoveModeTransition_WalkToFall()
	: Super(UCFrameWalkingMode::StaticClass(), UCFrameFallingMode::StaticClass())
{
	
}

bool UCFrameMoveModeTransition_WalkToFall::OnEvaluate(FCFrameMovementContext& Context) const
{
	bool Result = Super::OnEvaluate(Context);

	UCFrameWalkingMode* WalkingMode = Cast<UCFrameWalkingMode>(Context.CurrentMode);
	check(WalkingMode);

	return !WalkingMode->bIsOnWalkableFloor;
}