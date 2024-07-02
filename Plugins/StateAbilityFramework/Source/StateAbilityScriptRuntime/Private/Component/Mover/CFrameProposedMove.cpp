#include "Component/Mover/CFrameProposedMove.h"

void FCFrameProposedMove::Clear()
{
	MixMode = ECFrameMoveMixMode::AdditiveVelocity;
	bHasTargetLocation = false;

	LinearVelocity = FVector::ZeroVector;
	AngularVelocity = FRotator::ZeroRotator;

	MovePlaneVelocity = FVector::ZeroVector;

	TargetLocation = FVector::ZeroVector;
}