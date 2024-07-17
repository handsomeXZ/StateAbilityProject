#include "Component/Mover/Mode/CFrameFallingMode.h"

#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "InputAction.h"

#include "Net/CommandEnhancedInput.h"
#include "Component/CFrameMoverComponent.h"
#include "Component/Mover/CFrameMovementContext.h"
#include "Component/Mover/CFrameMoveStateAdapter.h"
#include "Component/Mover/MoveLibrary/CFrameMovementUtils.h"
#include "Component/Mover/MoveLibrary/CFrameGroundMoveUtils.h"
#include "Component/Mover/MoveLibrary/CFrameAirMoveUtils.h"

UCFrameFallingMode::UCFrameFallingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

void UCFrameFallingMode::OnActivated()
{
	Super::OnActivated();

	check(IsValid(InputComponent));

	if (UCommandEnhancedInputComponent* CFrameInputComp = Cast<UCommandEnhancedInputComponent>(InputComponent))
	{
		MoveTriggeredHandle = CFrameInputComp->BindCommandInput(IA_Move, ETriggerEvent::Triggered, this, &UCFrameFallingMode::OnMoveTriggered).GetHandle();
		MoveCompletedHandle = CFrameInputComp->BindCommandInput(IA_Move, ETriggerEvent::Completed, this, &UCFrameFallingMode::OnMoveCompleted).GetHandle();
	}
	else if (UEnhancedInputComponent* InputComp = Cast<UEnhancedInputComponent>(InputComponent))
	{
		MoveTriggeredHandle = InputComp->BindAction(IA_Move, ETriggerEvent::Triggered, this, &UCFrameFallingMode::OnMoveTriggered).GetHandle();
		MoveCompletedHandle = InputComp->BindAction(IA_Move, ETriggerEvent::Completed, this, &UCFrameFallingMode::OnMoveCompleted).GetHandle();
	}
}

void UCFrameFallingMode::OnDeactivated()
{
	Super::OnDeactivated();


	if (UCommandEnhancedInputComponent* CFrameInputComp = Cast<UCommandEnhancedInputComponent>(InputComponent))
	{
		CFrameInputComp->RemoveCommandInputByHandle(MoveTriggeredHandle);
		CFrameInputComp->RemoveCommandInputByHandle(MoveCompletedHandle);
	}
	else if (UEnhancedInputComponent* InputComp = Cast<UEnhancedInputComponent>(InputComponent))
	{
		InputComp->RemoveBindingByHandle(MoveTriggeredHandle);
		InputComp->RemoveBindingByHandle(MoveCompletedHandle);
	}
}

constexpr float VERTICAL_SLOPE_NORMAL_Z = 0.001f; // Slope is vertical if Abs(Normal.Z) <= this threshold. Accounts for precision problems that sometimes angle normals slightly off horizontal for vertical surface.


void UCFrameFallingMode::GenerateMove(FCFrameMovementContext& Context, FCFrameProposedMove& OutProposedMove)
{
	UCFrameMoverComponent* MoverComp = Context.MoverComp;
	UCFrameMoveStateAdapter* MoveStateAdapter = Context.MoveStateAdapter;
	UPrimitiveComponent* MovementBase = MoveStateAdapter->GetMovementBase();
	FName MovementBaseBoneName = MoveStateAdapter->GetMovementBaseBoneName();

	// We don't want velocity limits to take the falling velocity component into account, since it is handled 
	//   separately by the terminal velocity of the environment.
	const FVector StartVelocity = MoveStateAdapter->GetVelocity_WorldSpace();
	const FVector StartHorizontalVelocity = FVector(StartVelocity.X, StartVelocity.Y, 0.f);


	//////////////////////////////////////////////////////////////////////////
	FVector FrameInputVector = ConsumeControlInputVector();
	FVector OrientationIntent = FVector::ZeroVector;
	FRotator IntendedOrientation_WorldSpace = FRotator::ZeroRotator;


	CFrameMovementModeUtils::CalculateControlInput(OUT FrameInputVector, OUT OrientationIntent, Context, MoveInputType, LastAffirmativeMoveInput,
		bOrientRotationToMovement, bMaintainLastInputOrientation, bShouldRemainVertical, bUseBaseRelativeMovement);


	// 如果输入没有intent改变orientation，使用当前的Orientation
	if (ControlInputVector.IsNearlyZero() || OrientationIntent.IsNearlyZero())
	{
		IntendedOrientation_WorldSpace = MoveStateAdapter->GetOrientation_WorldSpace();
	}
	else if (bUseBaseRelativeMovement && MovementBase)
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
	
	FCFrameFreeMoveParams Params;

	Params.MoveInputType = MoveInputType;
	Params.MoveInput = FrameInputVector;

	Params.OrientationIntent = IntendedOrientation_WorldSpace;
	Params.PriorVelocity = StartHorizontalVelocity;
	Params.PriorOrientation = MoveStateAdapter->GetOrientation_WorldSpace();
	Params.DeltaSeconds = Context.DeltaTime;
	Params.TurningRate = TurningRate;
	Params.TurningBoost = TurningBoost;
	Params.MaxSpeed = MaxSpeed;
	Params.Acceleration = Acceleration;
	Params.Deceleration = FallingDeceleration;

	Params.MoveInput *= AirControlPercentage;
	// Don't care about Z axis input since falling - if Z input matters that should probably be a different movement mode
	Params.MoveInput.Z = 0;

	// Check if any current velocity values are over our terminal velocity - if so limit the move input in that direction and apply OverTerminalVelocityFallingDeceleration
	if (Params.MoveInput.Dot(StartVelocity) > 0 && StartVelocity.Size2D() >= TerminalMovementPlaneSpeed)
	{
		const FPlane MovementNormalPlane(StartVelocity, StartVelocity.GetSafeNormal());
		Params.MoveInput = Params.MoveInput.ProjectOnTo(MovementNormalPlane);

		Params.Deceleration = OverTerminalSpeedFallingDeceleration;
	}


	FFloorCheckResult LastFloorResult;
	// limit our moveinput based on the floor we're on
	if (Context.GetPersistentData(CFrameContextDataKey::LastFloorResult, LastFloorResult))
	{
		if (LastFloorResult.HitResult.IsValidBlockingHit() && LastFloorResult.HitResult.Normal.Z > VERTICAL_SLOPE_NORMAL_Z && !LastFloorResult.IsWalkableFloor())
		{
			// If acceleration is into the wall, limit contribution.
			if (FVector::DotProduct(Params.MoveInput, LastFloorResult.HitResult.Normal) < 0.f)
			{
				// Allow movement parallel to the wall, but not into it because that may push us up.
				const FVector Normal2D = LastFloorResult.HitResult.Normal.GetSafeNormal2D();
				Params.MoveInput = FVector::VectorPlaneProject(Params.MoveInput, Normal2D);
			}
		}
	}

	FCFrameAirMoveUtils::ComputeControlledFreeMove(Params, OutProposedMove);
	const FVector VelocityWithGravity = StartVelocity + FCFrameMovementUtils::ComputeVelocityFromGravity(MoverComp->GetGravityAcceleration(), Context.DeltaTime);

	//  If we are going faster than TerminalVerticalVelocity apply VerticalFallingDeceleration otherwise reset Z velocity to before we applied deceleration 
	if (VelocityWithGravity.GetAbs().Z > TerminalVerticalSpeed)
	{
		if (bShouldClampTerminalVerticalSpeed)
		{
			OutProposedMove.LinearVelocity.Z = FMath::Sign(VelocityWithGravity.Z) * TerminalVerticalSpeed;
		}
		else
		{
			float DesiredDeceleration = FMath::Abs(TerminalVerticalSpeed - FMath::Abs(VelocityWithGravity.Z)) / Context.DeltaTime;
			float DecelerationToApply = FMath::Min(DesiredDeceleration, VerticalFallingDeceleration);
			DecelerationToApply = FMath::Sign(VelocityWithGravity.Z) * DecelerationToApply * Context.DeltaTime;
			OutProposedMove.LinearVelocity.Z = VelocityWithGravity.Z - DecelerationToApply;
		}
	}
	else
	{
		OutProposedMove.LinearVelocity.Z = VelocityWithGravity.Z;
	}
}

void UCFrameFallingMode::Execute(FCFrameMovementContext& Context)
{
	UCFrameMoverComponent* MoverComp = Context.MoverComp;
	USceneComponent* UpdatedComponent = Context.UpdatedComponent;
	UPrimitiveComponent* UpdatedPrimitive = Context.UpdatedPrimitive;
	UCFrameMoveStateAdapter* MoveStateAdapter = Context.MoveStateAdapter;
	FCFrameProposedMove& ProposedMove = Context.CombinedMove;

	bIsOnWalkableFloor = false;

	//const FVector GravityAccel = FVector(0.0f, 0.0f, -9800.0f);	// -9.8 m / sec^2

	Context.InvalidPersistentData(CFrameContextDataKey::LastFloorResult);	// falling = no valid floor
	Context.InvalidPersistentData(CFrameContextDataKey::LastFoundDynamicMovementBase);	// falling = no valid floor

	// Use the orientation intent directly. If no intent is provided, use last frame's orientation. Note that we are assuming rotation changes can't fail. 
	FRotator TargetOrient = MoveStateAdapter->GetOrientation_WorldSpace();

	// Apply orientation changes (if any)
	if (!ProposedMove.AngularVelocity.IsZero())
	{
		TargetOrient += (ProposedMove.AngularVelocity * Context.DeltaTime);
	}

	const FVector PriorFallingVelocity = MoveStateAdapter->GetVelocity_WorldSpace();

	//FVector OrigMoveDelta = 0.5f * (PriorFallingVelocity + ProposedMove.LinearVelocity) * Context.DeltaTime; 	// TODO: revive midpoint integration
	FVector OrigMoveDelta = ProposedMove.LinearVelocity * Context.DeltaTime;

	FHitResult MoveHitResult(1.f);
	const FQuat TargetOrientQuat = TargetOrient.Quaternion();

	FCFrameMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, OrigMoveDelta, TargetOrientQuat, true, MoveHitResult, ETeleportType::None);

	// Compute final velocity based on how long we actually go until we get a hit.
	FVector NewFallingVelocity = MoveStateAdapter->GetVelocity_WorldSpace();
	//NewFallingVelocity.Z = 0.5f * (PriorFallingVelocity.Z + (ProposedMove.LinearVelocity.Z * Hit.Time));	// TODO: revive midpoint integration
	NewFallingVelocity.Z = ProposedMove.LinearVelocity.Z * MoveHitResult.Time;

	FFloorCheckResult LandingFloor;

	// Handle impact, whether it's a landing surface or something to slide on
	if (MoveHitResult.IsValidBlockingHit() && UpdatedPrimitive)
	{
		float LastMoveTimeSlice = Context.DeltaTime;
		float SubTimeTickRemaining = LastMoveTimeSlice * (1.f - MoveHitResult.Time);

		// Check for hitting a landing surface
		if (FCFrameAirMoveUtils::IsValidLandingSpot(UpdatedComponent, UpdatedPrimitive, UpdatedPrimitive->GetComponentLocation(),
			MoveHitResult, FloorSweepDistance, MaxWalkSlopeCosine, OUT LandingFloor))
		{
			CaptureFinalState(Context, LandingFloor);
			return;
		}

		LandingFloor.HitResult = MoveHitResult;
		Context.SetPersistentData(CFrameContextDataKey::LastFloorResult, LandingFloor);

		MoverComp->HandleImpact(MoveHitResult, GetModeName(), OrigMoveDelta);

		// We didn't land on a walkable surface, so let's try to slide along it
		FCFrameAirMoveUtils::TryMoveToFallAlongSurface(UpdatedComponent, UpdatedPrimitive, MoverComp, OrigMoveDelta,
			(1.f - MoveHitResult.Time), TargetOrientQuat, MoveHitResult.Normal, MoveHitResult, true,
			FloorSweepDistance, MaxWalkSlopeCosine, LandingFloor);

		if (LandingFloor.IsWalkableFloor())
		{
			CaptureFinalState(Context, LandingFloor);
			return;
		}
	}

	CaptureFinalState(Context, LandingFloor);
}


void UCFrameFallingMode::OnClientRewind()
{
	// 回滚时，需要清理上一帧的缓存数据
	ControlInputVector = FVector::ZeroVector;
}

void UCFrameFallingMode::OnMoveTriggered(const FInputActionValue& Value)
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

void UCFrameFallingMode::OnMoveCompleted(const FInputActionValue& Value)
{
	ControlInputVector = FVector::ZeroVector;
}

void UCFrameFallingMode::CaptureFinalState(FCFrameMovementContext& Context, const FFloorCheckResult& FloorResult) const
{
	Context.MoveStateAdapter->UpdateMoveFrame();

	FVector EffectiveVelocity = Context.MoveStateAdapter->GetVelocity_WorldSpace();

	FCFrameRelativeBaseInfo CurrentBaseInfo;
	{
		if (FloorResult.IsWalkableFloor())
		{
			// Transfer to LandingMovementMode (usually walking), and cache any floor / movement base info
			EffectiveVelocity.Z = 0.0;

			bIsOnWalkableFloor = true;

			Context.SetPersistentData(CFrameContextDataKey::LastFloorResult, FloorResult);

			if (FCFrameBasedMoveUtils::IsADynamicBase(FloorResult.HitResult.GetComponent()))
			{
				CurrentBaseInfo.SetFromFloorResult(FloorResult);
			}
		}
		else
		{
			bIsOnWalkableFloor = false;
		}
		// we could check for other surfaces here (i.e. when swimming is implemented we can check the floor hit here and see if we need to go into swimming)
		// This would also be a good spot for implementing some falling physics interactions (i.e. falling into a movable object and pushing it based off of this actors velocity)
		// if a new mode was set go ahead and switch to it after this tick and broadcast we landed
	}

	Context.MoveStateAdapter->SetLastFrameVelocity(EffectiveVelocity, Context.DeltaTime);

	if (CurrentBaseInfo.HasRelativeInfo())
	{
		Context.SetPersistentData(CFrameContextDataKey::LastFoundDynamicMovementBase, CurrentBaseInfo);

		Context.MoveStateAdapter->SetMovementBase(CurrentBaseInfo.MovementBase.Get(), CurrentBaseInfo.BoneName);
	}
	else
	{
		// no movement base
		Context.MoveStateAdapter->SetMovementBase(nullptr, NAME_None);
	}

	Context.UpdatedComponent->ComponentVelocity = Context.MoveStateAdapter->GetVelocity_WorldSpace();
}

FVector UCFrameFallingMode::ConsumeControlInputVector()
{
	FVector Result = ControlInputVector;
	ControlInputVector = FVector::ZeroVector;
	return Result;
}