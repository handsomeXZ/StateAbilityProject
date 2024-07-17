#include "Component/Mover/MoveLibrary/CFrameAirMoveUtils.h"

#include "MoveLibrary/FloorQueryUtils.h"

#include "Component/CFrameMoverComponent.h"
#include "Component/Mover/CFrameProposedMove.h"

void FCFrameAirMoveUtils::ComputeControlledFreeMove(const FCFrameFreeMoveParams& InParams, OUT FCFrameProposedMove& OutProposedMove)
{
	const FPlane MovementPlane(FVector::ZeroVector, FVector::UpVector);

	FCFrameComputeVelocityParams ComputeVelocityParams;
	ComputeVelocityParams.DeltaSeconds = InParams.DeltaSeconds;
	ComputeVelocityParams.InitialVelocity = InParams.PriorVelocity;
	ComputeVelocityParams.MoveDirectionIntent = InParams.MoveInput;
	ComputeVelocityParams.MaxSpeed = InParams.MaxSpeed;
	ComputeVelocityParams.TurningBoost = InParams.TurningBoost;
	ComputeVelocityParams.Deceleration = InParams.Deceleration;
	ComputeVelocityParams.Acceleration = InParams.Acceleration;

	OutProposedMove.LinearVelocity = FCFrameMovementUtils::ComputeVelocity(ComputeVelocityParams);
	OutProposedMove.MovePlaneVelocity = FCFrameMovementUtils::ConstrainToPlane(OutProposedMove.LinearVelocity, MovementPlane, false);

	// JAH TODO: this is where we can perform turning, based on aux settings. For now, just snap to the intended final orientation.
	FVector IntendedFacingDir = InParams.OrientationIntent.RotateVector(FVector::ForwardVector).GetSafeNormal();
	OutProposedMove.AngularVelocity = FCFrameMovementUtils::ComputeAngularVelocity(InParams.PriorOrientation, IntendedFacingDir.ToOrientationRotator(), InParams.DeltaSeconds, InParams.TurningRate);
}

bool FCFrameAirMoveUtils::IsValidLandingSpot(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, const FVector& Location, const FHitResult& Hit, float FloorSweepDistance, float WalkableFloorZ, FFloorCheckResult& OutFloorResult)
{
	OutFloorResult.Clear();

	if (!Hit.bBlockingHit)
	{
		return false;
	}

	if (Hit.bStartPenetrating)
	{
		return false;
	}

	// Reject unwalkable floor normals.
	if (!UFloorQueryUtils::IsHitSurfaceWalkable(Hit, WalkableFloorZ))
	{
		return false;
	}

	// Make sure floor test passes here.
	UFloorQueryUtils::FindFloor(UpdatedComponent, UpdatedPrimitive,
		FloorSweepDistance, WalkableFloorZ,
		Location, OutFloorResult);

	if (!OutFloorResult.IsWalkableFloor())
	{
		return false;
	}

	return true;
}

float FCFrameAirMoveUtils::TryMoveToFallAlongSurface(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, class UCFrameMoverComponent* MoverComponent, const FVector& Delta, float PctOfDeltaToMove, const FQuat Rotation, const FVector& Normal, FHitResult& Hit, bool bHandleImpact, float FloorSweepDistance, float MaxWalkSlopeCosine, FFloorCheckResult& OutFloorResult)
{
	OutFloorResult.Clear();

	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}

	float PctOfTimeUsed = 0.f;
	const FVector OldHitNormal = Normal;

	FVector SlideDelta = FCFrameMovementUtils::ComputeSlideDelta(Delta, PctOfDeltaToMove, Normal, Hit);

	if ((SlideDelta | Delta) > 0.f)
	{
		// First sliding attempt along surface
		FCFrameMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, SlideDelta, Rotation, true, Hit, ETeleportType::None);

		PctOfTimeUsed = Hit.Time;
		if (Hit.IsValidBlockingHit())
		{
			// Notify first impact
			if (MoverComponent && bHandleImpact)
			{
				MoverComponent->HandleImpact(Hit, NAME_None, SlideDelta);
			}

			// Check if we landed
			if (!IsValidLandingSpot(UpdatedComponent, UpdatedPrimitive, UpdatedPrimitive->GetComponentLocation(),
				Hit, FloorSweepDistance, MaxWalkSlopeCosine, OutFloorResult))
			{
				// We've hit another surface during our first move, so let's try to slide along both of them together

				// Compute new slide normal when hitting multiple surfaces.
				SlideDelta = FCFrameMovementUtils::ComputeTwoWallAdjustedDelta(SlideDelta, Hit, OldHitNormal);

				// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
				if (!SlideDelta.IsNearlyZero(FCFrameMovementUtils::SMALL_MOVE_DISTANCE) && (SlideDelta | Delta) > 0.f)
				{
					// Perform second move, taking 2 walls into account
					FCFrameMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, SlideDelta, Rotation, true, Hit, ETeleportType::None);
					PctOfTimeUsed += (Hit.Time * (1.f - PctOfTimeUsed));

					// Notify second impact
					if (MoverComponent && bHandleImpact && Hit.bBlockingHit)
					{
						MoverComponent->HandleImpact(Hit, NAME_None, SlideDelta);
					}

					// Check if we've landed, to acquire floor result
					IsValidLandingSpot(UpdatedComponent, UpdatedPrimitive, UpdatedPrimitive->GetComponentLocation(),
						Hit, FloorSweepDistance, MaxWalkSlopeCosine, OutFloorResult);
				}
			}
		}

		return FMath::Clamp(PctOfTimeUsed, 0.f, 1.f);
	}

	return 0.f;
}