// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "CFrameProposedMove.generated.h"

UENUM()
enum class ECFrameMoveMixMode : uint8
{
	/** Velocity (linear and angular) is intended to be added with other sources */
	AdditiveVelocity = 0,
	/** Velocity (linear and angular) should override others */
	OverrideVelocity = 1,
	/** All move parameters should override others */
	OverrideAll = 2,
};

/** Encapsulates info about an intended move that hasn't happened yet */
USTRUCT(BlueprintType)
struct FCFrameProposedMove
{
	GENERATED_USTRUCT_BODY()

	FCFrameProposedMove() :
		bHasTargetLocation(false)
	{}

	void Clear();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	ECFrameMoveMixMode MixMode = ECFrameMoveMixMode::AdditiveVelocity;		// Determines how this move should resolve with other moves

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	uint8	 bHasTargetLocation : 1;					// Signals whether the proposed move should move to a target location, regardless of other fields

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector  LinearVelocity = FVector::ZeroVector;		// Units per second, world space, possibly mapped onto walking surface
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FRotator AngularVelocity = FRotator::ZeroRotator;	// Degrees per second, local space

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector  MovePlaneVelocity = FVector::ZeroVector;	// Units per second, world space, always along the movement plane

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector  TargetLocation = FVector::ZeroVector;		// World space go-to position. Only valid if bHasTargetLocation is set. Used for movement like teleportation.
};