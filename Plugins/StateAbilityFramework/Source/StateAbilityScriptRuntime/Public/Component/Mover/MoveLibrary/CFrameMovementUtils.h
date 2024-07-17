// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"

#include "Components/PrimitiveComponent.h"

#include "CFrameMovementUtils.generated.h"

class UCFrameMoverComponent;

// Used to identify how to interpret a movement input vector's values
UENUM(BlueprintType)
enum class ECFrameMoveInputType : uint8
{
	Invalid,

	/** Move with intent, as a per-axis magnitude [-1,1] (E.g., "move straight forward as fast as possible" would be (1, 0, 0) and "move straight left at half speed" would be (0, -0.5, 0) regardless of frame time). Zero vector indicates intent to stop. */
	DirectionalIntent,

	/** Move with a given velocity (units per second) */
	Velocity,
};

// Input parameters for compute velocity function
struct STATEABILITYSCRIPTRUNTIME_API FCFrameComputeVelocityParams
{

	float DeltaSeconds = 0.f;
	FVector InitialVelocity = FVector::ZeroVector;
	FVector MoveDirectionIntent = FVector::ZeroVector;

	// AuxState variables
	float MaxSpeed = 0.f;
	float TurningBoost = 0.f;
	float Friction = 0.f;
	float Deceleration = 0.f;
	float Acceleration = 0.f;
};

// Input parameters for ComputeCombinedVelocity()
struct STATEABILITYSCRIPTRUNTIME_API FCFrameComputeCombinedVelocityParams
{
	float DeltaSeconds = 0.f;
	FVector InitialVelocity = FVector::ZeroVector;
	FVector MoveDirectionIntent = FVector::ZeroVector;

	// AuxState variables
	float MaxSpeed = 0.f;
	float TurningBoost = 0.f;
	float Friction = 0.f;
	float Deceleration = 0.f;
	float Acceleration = 0.f;

	FVector ExternalAcceleration = FVector::ZeroVector;
	float OverallMaxSpeed = 0.f;
};

/**
 * MovementUtils: a collection of stateless static BP-accessible functions for a variety of movement-related operations
 */
struct STATEABILITYSCRIPTRUNTIME_API FCFrameMovementUtils
{
	static const double SMALL_MOVE_DISTANCE;

	/** Checks whether a given velocity is exceeding a maximum speed, with some leeway to account for numeric imprecision */
	static bool IsExceedingMaxSpeed(const FVector& Velocity, float InMaxSpeed);

	/** Returns new ground-based velocity (worldspace) based on previous state, movement intent (worldspace), and movement settings */
	static FVector ComputeVelocity(const FCFrameComputeVelocityParams& InParams);

	/** Returns new velocity based on previous state, movement intent, movement mode's influence and movement settings */
	static FVector ComputeCombinedVelocity(const FCFrameComputeCombinedVelocityParams& InParams);

	/** Returns velocity (units per second) contributed by gravitational acceleration over a given time */
	static FVector ComputeVelocityFromGravity(const FVector& GravityAccel, float DeltaSeconds) { return GravityAccel * DeltaSeconds; }

	/** Ensures input Vector (typically a velocity, acceleration, or move delta) is limited to a movement plane.
	* @param bMaintainMagnitude - if true, vector will be scaled after projection in an attempt to keep magnitude the same
	*/
	static FVector ConstrainToPlane(const FVector& Vector, const FPlane& MovementPlane, bool bMaintainMagnitude = true);

	/** Computes the angular velocity needed to change from one orientation to another within a time frame. Use the optional TurningRateLimit to clamp to a maximum step (negative=unlimited). */
	static FRotator ComputeAngularVelocity(const FRotator& From, const FRotator& To, float DeltaSeconds, float TurningRateLimit = -1.f);

	/** Computes the directional movement intent based on input vector and associated type */
	static FVector ComputeDirectionIntent(const FVector& MoveInput, ECFrameMoveInputType MoveInputType);

	/** Returns a movement step that should get the subject of the hit result out of an initial penetration condition */
	static FVector ComputePenetrationAdjustment(const FHitResult& Hit);


	//////////////////////////////////////////////////////////////////////////
	// Surface sliding

	/** Returns an alternative move delta to slide along a surface, based on parameters describing a blocked attempted move */
	static FVector ComputeSlideDelta(const FVector& Delta, const float PctOfDeltaToMove, const FVector& Normal, const FHitResult& Hit);

	/** Returns an alternative move delta when we are in contact with 2 surfaces */
	static FVector ComputeTwoWallAdjustedDelta(const FVector& MoveDelta, const FHitResult& Hit, const FVector& OldHitNormal);

	/** Attempts to move a component along a surface. Returns the percent of time applied, with 0.0 meaning no movement occurred. */
	static float TryMoveToSlideAlongSurface(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UCFrameMoverComponent* MoverComponent, const FVector& Delta, float PctOfDeltaToMove, const FQuat Rotation, const FVector& Normal, FHitResult& Hit, bool bHandleImpact);
	
	//////////////////////////////////////////////////////////////////////////
	
	/** Attempts to move out of a situation where the component is stuck in geometry, using a suggested adjustment to start. */
	static bool TryMoveToResolvePenetration(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, EMoveComponentFlags MoveComponentFlags, const FVector& ProposedAdjustment, const FHitResult& Hit, const FQuat& NewRotationQuat);
	static void InitCollisionParams(const UPrimitiveComponent* UpdatedPrimitive, FCollisionQueryParams& OutParams, FCollisionResponseParams& OutResponseParam);
	static bool OverlapTest(const USceneComponent* UpdatedComponent, const UPrimitiveComponent* UpdatedPrimitive, const FVector& Location, const FQuat& RotationQuat, const ECollisionChannel CollisionChannel, const FCollisionShape& CollisionShape, const AActor* IgnoreActor);



	/** Attempts to move a component and resolve any penetration issues with the proposed move Delta */
	static bool TrySafeMoveUpdatedComponent(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult& OutHit, ETeleportType Teleport);
	
	// Internal functions - not meant to be called outside of this library

	/** Internal function that other move functions use to perform all actual component movement and retrieve results
	 *  Note: This function moves the character directly and should only be used if needed. Consider using something like TrySafeMoveUpdatedComponent.
	 */
	static bool TryMoveUpdatedComponent_Internal(USceneComponent* UpdatedComponent, const FVector& Delta, const FQuat& NewRotation, bool bSweep, EMoveComponentFlags MoveComponentFlags, FHitResult* OutHit, ETeleportType Teleport);
};