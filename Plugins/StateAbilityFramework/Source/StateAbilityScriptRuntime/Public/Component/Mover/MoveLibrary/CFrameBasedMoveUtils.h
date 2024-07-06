// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "MoveLibrary/FloorQueryUtils.h"

#include "CFrameBasedMoveUtils.generated.h"

class UPrimitiveComponent;

/** Data about the object a Mover actor is basing its movement on, such as when standing on a moving platform */
USTRUCT()
struct FCFrameRelativeBaseInfo
{
	GENERATED_USTRUCT_BODY()
	/** Component we are moving relative to */
	UPROPERTY()
	TWeakObjectPtr<UPrimitiveComponent> MovementBase = nullptr;

	/** Bone name on component, for skeletal meshes. NAME_None if not a skeletal mesh or if bone is invalid. */
	UPROPERTY()
	FName BoneName = NAME_None;

	/** Last captured worldspace location of MovementBase / Bone */
	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	/** Last captured worldspace orientation of MovementBase / Bone */
	UPROPERTY()
	FQuat Rotation = FQuat::Identity;

	/** Last captured location of the tethering point where the Mover actor is "attached", relative to the base. */
	UPROPERTY()
	FVector ContactLocalPosition = FVector::ZeroVector;

public:

	void Clear();

	bool HasRelativeInfo() const;
	bool UsesSameBase(const FCFrameRelativeBaseInfo& Other) const;
	bool UsesSameBase(const UPrimitiveComponent* OtherComp, FName OtherBoneName = NAME_None) const;

	void SetFromFloorResult(const FFloorCheckResult& FloorTestResult);
	void SetFromComponent(UPrimitiveComponent* InRelativeComp, FName InBoneName = NAME_None);

	FString ToString() const;
};

/**
 * MovementBaseUtils: a collection of stateless static BP-accessible functions for based movement
 */
struct FCFrameBasedMoveUtils
{
	/** Determine whether MovementBase can possibly move. */
	static bool IsADynamicBase(const UPrimitiveComponent* MovementBase);
	/** Get the transform (local-to-world) for the given MovementBase, optionally at the location of a bone. Returns false if MovementBase is nullptr, or if BoneName is not a valid bone. */
	static bool GetMovementBaseTransform(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector& OutLocation, FQuat& OutQuat);

	/** Convert a local direction to a world direction for a given MovementBase. Returns false if MovementBase is nullptr, or if BoneName is not a valid bone. Scaling is ignored. */
	static bool TransformBasedDirectionToWorld(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector LocalDirection, FVector& OutDirectionWorldSpace);
	/** Convert a world direction to a local direction for a given MovementBase, optionally relative to the orientation of a bone. Returns false if MovementBase is nullptr, or if BoneName is not a valid bone. Scaling is ignored. */
	static bool TransformWorldDirectionToBased(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector WorldSpaceDirection, FVector& OutLocalDirection);

	/** Convert a local direction to a world direction for a given MovementBase. Returns false if MovementBase is nullptr, or if BoneName is not a valid bone. Scaling is ignored. */
	static void TransformDirectionToWorld(FQuat BaseQuat, FVector LocalDirection, FVector& OutDirectionWorldSpace);
	/** Convert a world direction to a local direction for a given MovementBase, optionally relative to the orientation of a bone. Returns false if MovementBase is nullptr, or if BoneName is not a valid bone. Scaling is ignored. */
	static void TransformDirectionToLocal(FQuat BaseQuat, FVector WorldSpaceDirection, FVector& OutLocalDirection);

	/** Convert a local location to a world location for a given MovementBase. Returns false if MovementBase is nullptr, or if BoneName is not a valid bone. Scaling is ignored. */
	static bool TransformBasedLocationToWorld(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector LocalLocation, FVector& OutLocationWorldSpace);
	/** Convert a world location to a local location for a given MovementBase, optionally at the location of a bone. Returns false if MovementBase is nullptr, or if BoneName is not a valid bone. Scaling is ignored. */
	static bool TransformWorldLocationToBased(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector WorldSpaceLocation, FVector& OutLocalLocation);

	/** Convert a local location to a world location for a given MovementBase. Returns false if MovementBase is nullptr, or if BoneName is not a valid bone. Scaling is ignored. */
	static void TransformLocationToWorld(FVector BasePos, FQuat BaseQuat, FVector LocalLocation, FVector& OutLocationWorldSpace);
	/** Convert a world location to a local location for a given MovementBase, optionally at the location of a bone. Returns false if MovementBase is nullptr, or if BoneName is not a valid bone. Scaling is ignored. */
	static void TransformLocationToLocal(FVector BasePos, FQuat BaseQuat, FVector WorldSpaceLocation, FVector& OutLocalLocation);
};