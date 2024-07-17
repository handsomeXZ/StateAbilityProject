// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Component/Mover/MoveLibrary/CFrameMovementUtils.h"

class UCFrameMoverComponent;
struct FCFrameProposedMove;
struct FOptionalFloorCheckResult;
struct FFloorCheckResult;

// Input parameters for controlled ground movement function
struct STATEABILITYSCRIPTRUNTIME_API FCFrameGroundMoveParams
{
	ECFrameMoveInputType MoveInputType = ECFrameMoveInputType::DirectionalIntent;
	FVector MoveInput = FVector::ZeroVector;
	FRotator OrientationIntent = FRotator::ZeroRotator;
	FVector PriorVelocity = FVector::ZeroVector;
	FRotator PriorOrientation = FRotator::ZeroRotator;
	float MaxSpeed = 800.f;
	float Acceleration = 4000.f;
	float Deceleration = 8000.f;
	float Friction = 0.f;
	float TurningRate = 500.f;
	float TurningBoost = 8.f;
	FVector GroundNormal = FVector::UpVector;
	float DeltaSeconds = 0.f;
};


struct STATEABILITYSCRIPTRUNTIME_API FCFrameGroundMoveUtils
{
	/** Generate a new movement based on move/orientation intents and the prior state, constrained to the ground movement plane. Also applies deceleration friction as necessary. */
	static void ComputeControlledGroundMove(const FCFrameGroundMoveParams& InParams, OUT FCFrameProposedMove& OutProposedMove);

	/** Used to change a movement to be along a ramp's surface, typically to prevent slow movement when running up/down a ramp */
	static FVector ComputeDeflectedMoveOntoRamp(const FVector& OrigMoveDelta, const FHitResult& RampHitResult, float MaxWalkSlopeCosine, const bool bHitFromLineTrace);

	static bool CanStepUpOnHitSurface(const FHitResult& Hit);


	//////////////////////////////////////////////////////////////////////////

	// TODO: Refactor this API for fewer parameters
	/** Move up steps or slope. Does nothing and returns false if hit surface is invalid for step-up use */
	static bool TryMoveToStepUp(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UCFrameMoverComponent* MoverComponent, const FVector& GravDir, float MaxStepHeight, float MaxWalkSlopeCosine, float FloorSweepDistance, const FVector& MoveDelta, const FHitResult& MoveHitResult, const FFloorCheckResult& CurrentFloor, bool bIsFalling, FOptionalFloorCheckResult* OutStepDownResult);

	/** Moves vertically to stay within range of the walkable floor. Does nothing and returns false if floor is unwalkable or if already in range. */
	static bool TryMoveToAdjustHeightAboveFloor(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, FFloorCheckResult& CurrentFloor, float MaxWalkSlopeCosine);

	/** Attempts to move a component along a surface in the walking mode. Returns the percent of time applied, with 0.0 meaning no movement occurred.
	 *  Note: This modifies the normal and calls FCFrameMovementUtils::TryMoveToSlideAlongSurface
	 */
	static float TryWalkToSlideAlongSurface(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UCFrameMoverComponent* MoverComponent, const FVector& Delta, float PctOfDeltaToMove, const FQuat Rotation, const FVector& Normal, FHitResult& Hit, bool bHandleImpact, float MaxWalkSlopeCosine, float MaxStepHeight);

};
