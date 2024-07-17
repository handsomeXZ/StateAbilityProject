#include "Component/Mover/CFrameMovementMode.h"

#include "Component/CFrameMoverComponent.h"
#include "Component/Mover/CFrameMovementContext.h"
#include "Component/Mover/CFrameMoveStateAdapter.h"
#include "Component/Mover/MoveLibrary/CFrameBasedMoveUtils.h"
#include "Component/Mover/MoveLibrary/CFrameMovementUtils.h"

namespace CFrameMovementModeUtils
{
	void CalculateControlInput(OUT FVector& ControlInputVector, OUT FVector& OrientationIntent, FCFrameMovementContext& Context, ECFrameMoveInputType MoveInputType, FVector LastAffirmativeMoveInput,
			bool bOrientRotationToMovement, bool bMaintainLastInputOrientation, bool bShouldRemainVertical, bool bUseBaseRelativeMovement)
	{
		
		UCFrameMoverComponent* MoverComp = Context.MoverComp;
		UCFrameMoveStateAdapter* MoveStateAdapter = Context.MoveStateAdapter;
		UPrimitiveComponent* MovementBase = MoveStateAdapter->GetMovementBase();
		FName MovementBaseBoneName = MoveStateAdapter->GetMovementBaseBoneName();

		//////////////////////////////////////////////////////////////////////////
		// OrientationIntent

		static float RotationMagMin(1e-3);
		const bool bHasAffirmativeMoveInput = (ControlInputVector.Size() >= RotationMagMin);

		// Figure out intended orientation
		OrientationIntent = FVector::ZeroVector;
		if (MoveInputType == ECFrameMoveInputType::DirectionalIntent && bHasAffirmativeMoveInput)
		{
			if (bOrientRotationToMovement)
			{
				// set the intent to the actors movement direction
				OrientationIntent = ControlInputVector;
			}
			else
			{
				// set intent to the the control rotation - often a player's camera rotation
				OrientationIntent = MoveStateAdapter->GetControlRotation().Vector();
			}

			LastAffirmativeMoveInput = ControlInputVector;

		}
		else if (bMaintainLastInputOrientation)
		{
			// 没有移动intent，所以使用最后已知的 affirmative move input
			OrientationIntent = LastAffirmativeMoveInput;
		}

		if (bShouldRemainVertical)
		{
			// 如果角色应该保持垂直，则取消任何 z intent
			OrientationIntent = OrientationIntent.GetSafeNormal2D();
		}

		//////////////////////////////////////////////////////////////////////////

		if (bUseBaseRelativeMovement)
		{
			FVector RelativeMoveInput, RelativeOrientDir;

			FCFrameBasedMoveUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, ControlInputVector, RelativeMoveInput);
			FCFrameBasedMoveUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, OrientationIntent, RelativeOrientDir);

			ControlInputVector = RelativeMoveInput;
			OrientationIntent = RelativeOrientDir;
		}


	}
}

UCFrameMovementMode::UCFrameMovementMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, InputComponent(nullptr)
{

}

void UCFrameMovementMode::OnRegistered(FName InModeName)
{
	ModeName = InModeName;
}

void UCFrameMovementMode::SetupInputComponent(UInputComponent* InInputComponent)
{
	InputComponent = InInputComponent;
}

UCFrameMoverComponent* UCFrameMovementMode::GetMoverComp()
{
	return CastChecked<UCFrameMoverComponent>(GetOuter());
}