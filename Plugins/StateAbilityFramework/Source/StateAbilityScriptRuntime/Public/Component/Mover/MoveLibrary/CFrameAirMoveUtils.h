// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Component/Mover/MoveLibrary/CFrameMovementUtils.h"

struct FFloorCheckResult;
struct FCFrameProposedMove;

struct STATEABILITYSCRIPTRUNTIME_API FCFrameFreeMoveParams
{
	ECFrameMoveInputType MoveInputType = ECFrameMoveInputType::DirectionalIntent;

	FVector MoveInput = FVector::ZeroVector;

	FRotator OrientationIntent = FRotator::ZeroRotator;

	FVector PriorVelocity = FVector::ZeroVector;

	FRotator PriorOrientation = FRotator::ZeroRotator;

	float MaxSpeed = 800.f;

	float Acceleration = 4000.f;

	float Deceleration = 8000.f;

	float TurningBoost = 8.f;

	float TurningRate = 500.f;

	float DeltaSeconds = 0.f;
};

/**
 * CFrameAirMoveUtils: a collection of stateless static BP-accessible functions for a variety of air movement-related operations
 */
struct STATEABILITYSCRIPTRUNTIME_API FCFrameAirMoveUtils
{
public:
	/** Generate a new movement based on move/orientation intents and the prior state, unconstrained like when flying */
	static void ComputeControlledFreeMove(const FCFrameFreeMoveParams& InParams, OUT FCFrameProposedMove& OutProposedMove);
	
	// Checks if a hit result represents a walkable location that an actor can land on
	static bool IsValidLandingSpot(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, const FVector& Location, const FHitResult& Hit, float FloorSweepDistance, float MaxWalkSlopeCosine, FFloorCheckResult& OutFloorResult);

	/** Attempts to move a component along a surface, while checking for landing on a walkable surface. Intended for use while falling. Returns the percent of time applied, with 0.0 meaning no movement occurred. */
	static float TryMoveToFallAlongSurface(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, class UCFrameMoverComponent* MoverComponent, const FVector& Delta, float PctOfDeltaToMove, const FQuat Rotation, const FVector& Normal, FHitResult& Hit, bool bHandleImpact, float FloorSweepDistance, float MaxWalkSlopeCosine, FFloorCheckResult& OutFloorResult);


};