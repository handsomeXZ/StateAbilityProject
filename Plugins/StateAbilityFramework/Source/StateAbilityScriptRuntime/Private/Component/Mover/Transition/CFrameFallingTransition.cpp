#include "Component/Mover/Transition/CFrameFallingTransition.h"

#include "Component/Mover/Mode/CFrameWalkingMode.h"
#include "Component/Mover/Mode/CFrameFallingMode.h"

UCFrameMoveModeTransition_FallToWalk::UCFrameMoveModeTransition_FallToWalk()
	: Super(UCFrameFallingMode::StaticClass(), UCFrameWalkingMode::StaticClass())
{

}

bool UCFrameMoveModeTransition_FallToWalk::OnEvaluate(FCFrameMovementContext& Context) const
{
	bool Result = Super::OnEvaluate(Context);

	UCFrameFallingMode* FallingMode = Cast<UCFrameFallingMode>(Context.CurrentMode);
	check(FallingMode);

	return FallingMode->bIsOnWalkableFloor;
}