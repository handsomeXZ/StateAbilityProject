#include "Component/Mover/CFrameMovementMixer.h"


void UCFrameMovementMixer::MixProposedMoves(const FCFrameMovementContext& Context, const FCFrameProposedMove& MoveToMix, FCFrameProposedMove& OutCumulativeMove)
{

	// Combine movement parameters from layered moves into what the mode wants to do
	if (MoveToMix.MixMode == ECFrameMoveMixMode::OverrideAll)
	{
		OutCumulativeMove = MoveToMix;
	}
	else if (MoveToMix.MixMode == ECFrameMoveMixMode::AdditiveVelocity)
	{
		OutCumulativeMove.LinearVelocity += MoveToMix.LinearVelocity;
		OutCumulativeMove.AngularVelocity += MoveToMix.AngularVelocity;
	}
	else if (MoveToMix.MixMode == ECFrameMoveMixMode::OverrideVelocity)
	{
		OutCumulativeMove.LinearVelocity = MoveToMix.LinearVelocity;
		OutCumulativeMove.AngularVelocity = MoveToMix.AngularVelocity;
	}
	else
	{
		ensureMsgf(false, TEXT("Unhandled move mix mode was found."));
	}
}