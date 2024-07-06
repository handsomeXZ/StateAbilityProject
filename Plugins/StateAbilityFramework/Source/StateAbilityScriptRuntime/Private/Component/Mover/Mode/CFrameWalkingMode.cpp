#include "Component/Mover/Mode/CFrameWalkingMode.h"

#include "InputActionValue.h"
#include "InputAction.h"

#include "Net/CommandEnhancedInput.h"
#include "Component/CFrameMoverComponent.h"
#include "Component/Mover/CFrameMovementContext.h"
#include "Component/Mover/CFrameMoveStateAdapter.h"
#include "Component/Mover/MoveLibrary/CFrameMovementUtils.h"
#include "Component/Mover/MoveLibrary/CFrameGroundMoveUtils.h"
#include "Component/Mover/MoveLibrary/CFrameBasedMoveUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogCFrameWalkingMode, Log, All)

void UCFrameWalkingMode::OnRegistered(FName InModeName)
{
	Super::OnRegistered(InModeName);
}

void UCFrameWalkingMode::OnUnregistered()
{
	Super::OnUnregistered();
}

void UCFrameWalkingMode::GenerateMove(FCFrameMovementContext& Context, FCFrameProposedMove& OutProposedMove)
{
	UCFrameMoverComponent* MoverComp = Context.MoverComp;
	UCFrameMoveStateAdapter* MoveStateAdapter = Context.MoveStateAdapter;
	UPrimitiveComponent* MovementBase = MoveStateAdapter->GetMovementBase();
	FName MovementBaseBoneName = MoveStateAdapter->GetMovementBaseBoneName();

	FVector FrameInputVector = ConsumeControlInputVector();

	//////////////////////////////////////////////////////////////////////////
	// OrientationIntent

	static float RotationMagMin(1e-3);
	const bool bHasAffirmativeMoveInput = (FrameInputVector.Size() >= RotationMagMin);

	// Figure out intended orientation
	FVector OrientationIntent = FVector::ZeroVector;
	if (MoveInputType == ECFrameMoveInputType::DirectionalIntent && bHasAffirmativeMoveInput)
	{
		if (bOrientRotationToMovement)
		{
			// set the intent to the actors movement direction
			OrientationIntent = FrameInputVector;
		}
		else
		{
			// set intent to the the control rotation - often a player's camera rotation
			OrientationIntent = MoveStateAdapter->GetControlRotation().Vector();
		}

		LastAffirmativeMoveInput = FrameInputVector;

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

		FCFrameBasedMoveUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, FrameInputVector, RelativeMoveInput);
		FCFrameBasedMoveUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, OrientationIntent, RelativeOrientDir);

		FrameInputVector = RelativeMoveInput;
		OrientationIntent = RelativeOrientDir;
	}

	//////////////////////////////////////////////////////////////////////////
	FFloorCheckResult LastFloorResult;
	FVector MovementNormal;
	if (Context.GetPersistentData(CFrameContextDataKey::LastFloorResult, LastFloorResult) && LastFloorResult.IsWalkableFloor())
	{
		MovementNormal = LastFloorResult.HitResult.ImpactNormal;
	}
	else
	{
		MovementNormal = MoverComp->GetUpDirection();
	}

	//////////////////////////////////////////////////////////////////////////

	FRotator IntendedOrientation_WorldSpace;
	// 如果输入没有intent改变orientation，使用当前的Orientation
	if (FrameInputVector.IsNearlyZero() || OrientationIntent.IsNearlyZero())
	{
		IntendedOrientation_WorldSpace = MoveStateAdapter->GetOrientation_WorldSpace();
	}
	else if(bUseBaseRelativeMovement && MovementBase)
	{
		FVector OrientIntentDirWorldSpace;
		FCFrameBasedMoveUtils::TransformBasedDirectionToWorld(MovementBase, MovementBaseBoneName, OrientationIntent, OrientIntentDirWorldSpace);
		IntendedOrientation_WorldSpace = OrientIntentDirWorldSpace.ToOrientationRotator();
	}
	else
	{
		IntendedOrientation_WorldSpace = OrientationIntent.ToOrientationRotator();
	}


	//////////////////////////////////////////////////////////////////////////


	FCFrameGroundMoveParams Params;

	Params.MoveInputType = MoveInputType;
	Params.MoveInput = FrameInputVector;

	Params.OrientationIntent = IntendedOrientation_WorldSpace;
	Params.PriorVelocity = FVector::VectorPlaneProject(MoveStateAdapter->GetVelocity_WorldSpace(), MovementNormal);
	Params.PriorOrientation = MoveStateAdapter->GetOrientation_WorldSpace();
	Params.GroundNormal = MovementNormal;
	Params.TurningRate = TurningRate;
	Params.TurningBoost = TurningBoost;
	Params.MaxSpeed = MaxSpeed;
	Params.Acceleration = Acceleration;
	Params.Deceleration = Deceleration;
	Params.DeltaSeconds = Context.DeltaTime;

	if (Params.MoveInput.SizeSquared() > 0.f && !FCFrameMovementUtils::IsExceedingMaxSpeed(Params.PriorVelocity, MaxSpeed))
	{
		Params.Friction = GroundFriction;
	}
	else
	{
		Params.Friction = bUseSeparateBrakingFriction ? BrakingFriction : GroundFriction;
		Params.Friction *= BrakingFrictionFactor;
	}

	UE_LOG(LogCFrameWalkingMode, Log, TEXT("GenerateMove:	FrameInputVector[%s] OrientationIntent[%s]"), *(FrameInputVector.ToCompactString()), *(IntendedOrientation_WorldSpace.ToCompactString()));

	FCFrameGroundMoveUtils::ComputeControlledGroundMove(Params, OUT OutProposedMove);


	if (OutProposedMove.LinearVelocity.IsNearlyZero() && OutProposedMove.AngularVelocity.IsNearlyZero() && OutProposedMove.MovePlaneVelocity.IsNearlyZero())
	{
		UE_LOG(LogCFrameWalkingMode, Log, TEXT("GenerateMove:	The player stopped moving?"));
	}
	else
	{
		UE_LOG(LogCFrameWalkingMode, Log, TEXT("GenerateMove:	OutProposedMove RCF[%d] ICF[%d] LinearVelocity[%s] AngularVelocity[%s] MovePlaneVelocity[%s]."),
			Context.RCF, Context.ICF, *(OutProposedMove.LinearVelocity.ToCompactString()), *(OutProposedMove.AngularVelocity.ToCompactString()), *(OutProposedMove.MovePlaneVelocity.ToCompactString()));
	}
	/*if (TurnGenerator)
	{
		OutProposedMove.AngularVelocity = ITurnGeneratorInterface::Execute_GetTurn(TurnGenerator, IntendedOrientation_WorldSpace, StartState, *StartingSyncState, TimeStep, OutProposedMove, SimBlackboard);
	}*/

}

void UCFrameWalkingMode::Execute(FCFrameMovementContext& Context)
{

	UCFrameMoverComponent* MoverComp = Context.MoverComp;
	USceneComponent* UpdatedComponent = Context.UpdatedComponent;
	UPrimitiveComponent* UpdatedPrimitive = Context.UpdatedPrimitive;
	UCFrameMoveStateAdapter* MoveStateAdapter = Context.MoveStateAdapter;
	FCFrameProposedMove ProposedMove = Context.CombinedMove;

	if (ProposedMove.LinearVelocity.IsNearlyZero() && ProposedMove.AngularVelocity.IsNearlyZero())
	{
		UE_LOG(LogCFrameWalkingMode, Log, TEXT("Execute:	The player stopped moving?"));
	}
	else
	{
		UE_LOG(LogCFrameWalkingMode, Log, TEXT("Execute:	ProposedMove RCF[%d] ICF[%d] LinearVelocity[%s] AngularVelocity[%s]."), 
			Context.RCF, Context.ICF, *(ProposedMove.LinearVelocity.ToCompactString()), *(ProposedMove.AngularVelocity.ToCompactString()));
	}
	

	//////////////////////////////////////////////////////////////////////////

	FFloorCheckResult CurrentFloor;

	// If we don't have cached floor information, we need to search for it again
	if (!Context.GetPersistentData(CFrameContextDataKey::LastFloorResult, CurrentFloor))
	{
		UFloorQueryUtils::FindFloor(UpdatedComponent, UpdatedPrimitive,
			FloorSweepDistance, MaxWalkSlopeCosine,
			UpdatedPrimitive->GetComponentLocation(), CurrentFloor);
	}

	//////////////////////////////////////////////////////////////////////////

	const FRotator StartingOrient = MoveStateAdapter->GetOrientation_WorldSpace();
	FRotator TargetOrient = StartingOrient;

	bool bIsOrientationChanging = false;

	// Apply orientation changes (if any)
	if (!ProposedMove.AngularVelocity.IsZero())
	{
		TargetOrient += (ProposedMove.AngularVelocity * Context.DeltaTime);
		bIsOrientationChanging = (TargetOrient != StartingOrient);
	}

	//////////////////////////////////////////////////////////////////////////

	const FQuat TargetOrientQuat = TargetOrient.Quaternion();
	FHitResult MoveHitResult(1.f);

	const FVector OrigMoveDelta = ProposedMove.LinearVelocity * Context.DeltaTime;
	FVector CurMoveDelta = OrigMoveDelta;

	FOptionalFloorCheckResult StepUpFloorResult;	// passed to sub-operations, so we can use their final floor results if they did a test

	bool bDidAttemptMovement = false;

	float PercentTimeAppliedSoFar = MoveHitResult.Time;
	bool bWasFirstMoveBlocked = false;



	if (!CurMoveDelta.IsNearlyZero() || bIsOrientationChanging)
	{
		// Attempt to move the full amount first
		bDidAttemptMovement = true;
		bool bMoved = FCFrameMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, CurMoveDelta, TargetOrientQuat, true, MoveHitResult, ETeleportType::None);
		float LastMoveSeconds = Context.DeltaTime;

		if (MoveHitResult.bStartPenetrating)
		{
			// We started by being stuck in geometry and need to resolve it first
			// TODO: try to resolve starting stuck in geometry
			bWasFirstMoveBlocked = true;
		}
		else if (MoveHitResult.IsValidBlockingHit())
		{
			// We impacted something (possibly a ramp, possibly a barrier)
			bWasFirstMoveBlocked = true;
			PercentTimeAppliedSoFar = MoveHitResult.Time;

			// Check if the blockage is a walkable ramp rising in front of us
			// JAH TODO: change this normal.Z test to consider vs the movement plane (not just Z=0 plane)
			if ((MoveHitResult.Time > 0.f) && (MoveHitResult.Normal.Z > UE_KINDA_SMALL_NUMBER) &&
				UFloorQueryUtils::IsHitSurfaceWalkable(MoveHitResult, MaxWalkSlopeCosine))
			{
				// It's a walkable ramp, so cut up the move and attempt to move the remainder of it along the ramp's surface, possibly generating another hit
				const float PercentTimeRemaining = 1.f - PercentTimeAppliedSoFar;
				CurMoveDelta = FCFrameGroundMoveUtils::ComputeDeflectedMoveOntoRamp(CurMoveDelta * PercentTimeRemaining, MoveHitResult, MaxWalkSlopeCosine, CurrentFloor.bLineTrace);
				FCFrameMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, CurMoveDelta, TargetOrientQuat, true, MoveHitResult, ETeleportType::None);
				LastMoveSeconds = PercentTimeRemaining * LastMoveSeconds;

				const float SecondHitPercent = MoveHitResult.Time * PercentTimeRemaining;
				PercentTimeAppliedSoFar = FMath::Clamp(PercentTimeAppliedSoFar + SecondHitPercent, 0.f, 1.f);
			}

			if (MoveHitResult.IsValidBlockingHit())
			{
				// If still blocked, try to step up onto the blocking object OR slide along it

				// JAH TODO: Take movement bases into account
				if (FCFrameGroundMoveUtils::CanStepUpOnHitSurface(MoveHitResult)) // || (CharacterOwner->GetMovementBase() != nullptr && Hit.HitObjectHandle == CharacterOwner->GetMovementBase()->GetOwner()))
				{
					// hit a barrier or unwalkable surface, try to step up and onto it
					const FVector PreStepUpLocation = UpdatedComponent->GetComponentLocation();
					const FVector DownwardDir = -MoverComp->GetUpDirection();

					if (!FCFrameGroundMoveUtils::TryMoveToStepUp(UpdatedComponent, UpdatedPrimitive, MoverComp, DownwardDir, MaxStepHeight, MaxWalkSlopeCosine, FloorSweepDistance, OrigMoveDelta * (1.f - PercentTimeAppliedSoFar), MoveHitResult, CurrentFloor, false, &StepUpFloorResult))
					{
						MoverComp->HandleImpact(MoveHitResult, GetModeName(), OrigMoveDelta);
						float PercentAvailableToSlide = 1.f - PercentTimeAppliedSoFar;
						float SlideAmount = FCFrameGroundMoveUtils::TryWalkToSlideAlongSurface(UpdatedComponent, UpdatedPrimitive, MoverComp, OrigMoveDelta, PercentAvailableToSlide, TargetOrientQuat, MoveHitResult.Normal, MoveHitResult, true, MaxWalkSlopeCosine, MaxStepHeight);
						PercentTimeAppliedSoFar += PercentAvailableToSlide * SlideAmount;
					}
				}
				else if (MoveHitResult.Component.IsValid() && !MoveHitResult.Component.Get()->CanCharacterStepUp(Cast<APawn>(MoveHitResult.GetActor())))
				{
					MoverComp->HandleImpact(MoveHitResult, GetModeName(), OrigMoveDelta);
					float PercentAvailableToSlide = 1.f - PercentTimeAppliedSoFar;
					float SlideAmount = FCFrameGroundMoveUtils::TryWalkToSlideAlongSurface(UpdatedComponent, UpdatedPrimitive, MoverComp, OrigMoveDelta, 1.f - PercentTimeAppliedSoFar, TargetOrientQuat, MoveHitResult.Normal, MoveHitResult, true, MaxWalkSlopeCosine, MaxStepHeight);
					PercentTimeAppliedSoFar += PercentAvailableToSlide * SlideAmount;
				}
			}
		}

		// Search for the floor we've ended up on
		UFloorQueryUtils::FindFloor(UpdatedComponent, UpdatedPrimitive,
			FloorSweepDistance, MaxWalkSlopeCosine,
			UpdatedPrimitive->GetComponentLocation(), CurrentFloor);

		if (CurrentFloor.IsWalkableFloor())
		{
			FCFrameGroundMoveUtils::TryMoveToAdjustHeightAboveFloor(UpdatedComponent, UpdatedPrimitive, CurrentFloor, MaxWalkSlopeCosine);
		}

		if (!CurrentFloor.IsWalkableFloor() && !CurrentFloor.HitResult.bStartPenetrating)
		{
			// No floor or not walkable, so let's let the airborne movement mode deal with it
			CaptureFinalState(Context, bDidAttemptMovement, CurrentFloor);
			return;
		}
	}
	else
	{
		// If the actor isn't moving we still need to check if they have a valid floor
		UFloorQueryUtils::FindFloor(UpdatedComponent, UpdatedPrimitive,
			FloorSweepDistance, MaxWalkSlopeCosine,
			UpdatedPrimitive->GetComponentLocation(), CurrentFloor);

		FHitResult Hit(CurrentFloor.HitResult);
		if (Hit.bStartPenetrating)
		{
			// The floor check failed because it started in penetration
			// We do not want to try to move downward because the downward sweep failed, rather we'd like to try to pop out of the floor.
			Hit.TraceEnd = Hit.TraceStart + FVector(0.f, 0.f, 2.4f);
			FVector RequestedAdjustment = FCFrameMovementUtils::ComputePenetrationAdjustment(Hit);

			const EMoveComponentFlags IncludeBlockingOverlapsWithoutEvents = (MOVECOMP_NeverIgnoreBlockingOverlaps | MOVECOMP_DisableBlockingOverlapDispatch);
			EMoveComponentFlags MoveComponentFlags = MOVECOMP_NoFlags;
			MoveComponentFlags = (MoveComponentFlags | IncludeBlockingOverlapsWithoutEvents);
			FCFrameMovementUtils::TryMoveToResolvePenetration(UpdatedComponent, UpdatedPrimitive, MoveComponentFlags, RequestedAdjustment, Hit, UpdatedComponent->GetComponentQuat());
		}

		if (!CurrentFloor.IsWalkableFloor() && !Hit.bStartPenetrating)
		{
			// No floor or not walkable, so let's let the airborne movement mode deal with it
			CaptureFinalState(Context, bDidAttemptMovement, CurrentFloor);
			return;
		}
	}

	CaptureFinalState(Context, bDidAttemptMovement, CurrentFloor);

}

void UCFrameWalkingMode::SetupInputComponent(UInputComponent* InInputComponent)
{
	Super::SetupInputComponent(InInputComponent);

	if (UCommandEnhancedInputComponent* CFrameInputComp = Cast<UCommandEnhancedInputComponent>(InInputComponent))
	{
		CFrameInputComp->BindCommandInput(IA_Move, ETriggerEvent::Triggered, this, &UCFrameWalkingMode::OnMoveTriggered);
		CFrameInputComp->BindCommandInput(IA_Move, ETriggerEvent::Completed, this, &UCFrameWalkingMode::OnMoveCompleted);
	}
	else if (UEnhancedInputComponent* InputComp = Cast<UEnhancedInputComponent>(InInputComponent))
	{
		InputComp->BindAction(IA_Move, ETriggerEvent::Triggered, this, &UCFrameWalkingMode::OnMoveTriggered);
		InputComp->BindAction(IA_Move, ETriggerEvent::Completed, this, &UCFrameWalkingMode::OnMoveCompleted);
	}
}

void UCFrameWalkingMode::OnMoveTriggered(const FInputActionValue& Value)
{
	UCFrameMoverComponent* MoverComp = GetMoverComp();
	UCFrameMoveStateAdapter* MoveStateAdapter = MoverComp->GetMovementConfig().MoveStateAdapter;
	FRotator Rotator = MoveStateAdapter->GetControlRotation();
	{
		// UKismetMathLibrary::GetRightVector
		FVector WorldDirection = FRotationMatrix(FRotator(Rotator.Pitch, 0, Rotator.Roll)).GetScaledAxis(EAxis::Y);
		ControlInputVector += WorldDirection * Value.Get<FVector2D>().X;
	}
	{
		// UKismetMathLibrary::GetForwardVector
		Rotator.Roll = 0.0f;
		Rotator.Pitch = 0.0f;
		FVector WorldDirection = Rotator.Vector();
		ControlInputVector += WorldDirection * Value.Get<FVector2D>().Y;
	}
}

void UCFrameWalkingMode::OnMoveCompleted(const FInputActionValue& Value)
{
	ControlInputVector = FVector::ZeroVector;
}

void UCFrameWalkingMode::CaptureFinalState(FCFrameMovementContext& Context, bool bDidAttemptMovement, const FFloorCheckResult& FloorResult)
{
	Context.MoveStateAdapter->UpdateMoveFrame();

	FCFrameRelativeBaseInfo PriorBaseInfo;
	const bool bHasPriorBaseInfo = Context.GetPersistentData(CFrameContextDataKey::LastFoundDynamicMovementBase, PriorBaseInfo);

	FCFrameRelativeBaseInfo CurrentBaseInfo = UpdateFloorAndBaseInfo(Context, FloorResult);

	// If we're on a dynamic base and we're not trying to move, keep using the same relative actor location. This prevents slow relative 
	//  drifting that can occur from repeated floor sampling as the base moves through the world.
	if (CurrentBaseInfo.HasRelativeInfo() && bHasPriorBaseInfo && !bDidAttemptMovement
		&& PriorBaseInfo.UsesSameBase(CurrentBaseInfo))
	{
		CurrentBaseInfo.ContactLocalPosition = PriorBaseInfo.ContactLocalPosition;
	}

	// TODO: Update Main/large movement record with substeps from our local record

	if (CurrentBaseInfo.HasRelativeInfo())
	{
		Context.SetPersistentData(CFrameContextDataKey::LastFoundDynamicMovementBase, CurrentBaseInfo);

		Context.MoveStateAdapter->SetMovementBase(CurrentBaseInfo.MovementBase.Get(), CurrentBaseInfo.BoneName);
	}
	else
	{
		Context.InvalidPersistentData(CFrameContextDataKey::LastFoundDynamicMovementBase);

		// no movement base
		Context.MoveStateAdapter->SetMovementBase(nullptr, NAME_None);
	}

	Context.UpdatedComponent->ComponentVelocity = Context.MoveStateAdapter->GetVelocity_WorldSpace();
}

FVector UCFrameWalkingMode::ConsumeControlInputVector()
{
	FVector Result = ControlInputVector;
	ControlInputVector = FVector::ZeroVector;
	return Result;
}

FCFrameRelativeBaseInfo UCFrameWalkingMode::UpdateFloorAndBaseInfo(FCFrameMovementContext& Context, const FFloorCheckResult& FloorResult) const
{
	FCFrameRelativeBaseInfo ReturnBaseInfo;

	Context.SetPersistentData(CFrameContextDataKey::LastFloorResult, FloorResult);

	if (FloorResult.IsWalkableFloor() && FCFrameBasedMoveUtils::IsADynamicBase(FloorResult.HitResult.GetComponent()))
	{
		ReturnBaseInfo.SetFromFloorResult(FloorResult);
	}

	return ReturnBaseInfo;
}
