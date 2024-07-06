#include "Component/Mover/MoveLibrary/CFrameBasedMoveUtils.h"

#include "Components/PrimitiveComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogCFrameBasedMove, Log, All)

void FCFrameRelativeBaseInfo::Clear()
{
	MovementBase = nullptr;
	BoneName = NAME_None;
	Location = FVector::ZeroVector;
	Rotation = FQuat::Identity;
	ContactLocalPosition = FVector::ZeroVector;
}

bool FCFrameRelativeBaseInfo::HasRelativeInfo() const
{
	return MovementBase != nullptr;
}

bool FCFrameRelativeBaseInfo::UsesSameBase(const FCFrameRelativeBaseInfo& Other) const
{
	return UsesSameBase(Other.MovementBase.Get(), Other.BoneName);
}

bool FCFrameRelativeBaseInfo::UsesSameBase(const UPrimitiveComponent* OtherComp, FName OtherBoneName) const
{
	return HasRelativeInfo()
		&& (MovementBase == OtherComp)
		&& (BoneName == OtherBoneName);
}

void FCFrameRelativeBaseInfo::SetFromFloorResult(const FFloorCheckResult& FloorTestResult)
{
	bool bDidSucceed = false;

	if (FloorTestResult.bWalkableFloor)
	{
		MovementBase = FloorTestResult.HitResult.GetComponent();

		if (MovementBase.IsValid())
		{
			BoneName = FloorTestResult.HitResult.BoneName;

			if (FCFrameBasedMoveUtils::GetMovementBaseTransform(MovementBase.Get(), BoneName, OUT Location, OUT Rotation) &&
				FCFrameBasedMoveUtils::TransformWorldLocationToBased(MovementBase.Get(), BoneName, FloorTestResult.HitResult.ImpactPoint, OUT ContactLocalPosition))
			{
				bDidSucceed = true;
			}
		}
	}

	if (!bDidSucceed)
	{
		Clear();
	}
}

void FCFrameRelativeBaseInfo::SetFromComponent(UPrimitiveComponent* InRelativeComp, FName InBoneName)
{
	bool bDidSucceed = false;

	MovementBase = InRelativeComp;

	if (MovementBase.IsValid())
	{
		BoneName = InBoneName;
		bDidSucceed = FCFrameBasedMoveUtils::GetMovementBaseTransform(MovementBase.Get(), BoneName, /*out*/Location, /*out*/Rotation);
	}

	if (!bDidSucceed)
	{
		Clear();
	}
}


FString FCFrameRelativeBaseInfo::ToString() const
{
	if (MovementBase.IsValid())
	{
		return FString::Printf(TEXT("Base: %s, Loc: %s, Rot: %s, LocalContact: %s"),
			*GetNameSafe(MovementBase->GetOwner()),
			*Location.ToCompactString(),
			*Rotation.Rotator().ToCompactString(),
			*ContactLocalPosition.ToCompactString());
	}

	return FString(TEXT("Base: NULL"));
}

//////////////////////////////////////////////////////////////////////////

bool FCFrameBasedMoveUtils::IsADynamicBase(const UPrimitiveComponent* MovementBase)
{
	return (MovementBase && MovementBase->Mobility == EComponentMobility::Movable);
}

bool FCFrameBasedMoveUtils::GetMovementBaseTransform(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector& OutLocation, FQuat& OutQuat)
{
	if (MovementBase)
	{
		bool bBoneNameIsInvalid = false;

		if (BoneName != NAME_None)
		{
			// Check if this socket or bone exists (DoesSocketExist checks for either, as does requesting the transform).
			if (MovementBase->DoesSocketExist(BoneName))
			{
				MovementBase->GetSocketWorldLocationAndRotation(BoneName, OutLocation, OutQuat);
				return true;
			}

			bBoneNameIsInvalid = true;
			UE_LOG(LogCFrameBasedMove, Warning, TEXT("GetMovementBaseTransform(): Invalid bone or socket '%s' for PrimitiveComponent base %s. Using component's root transform instead."), *BoneName.ToString(), *GetPathNameSafe(MovementBase));
		}

		OutLocation = MovementBase->GetComponentLocation();
		OutQuat = MovementBase->GetComponentQuat();
		return !bBoneNameIsInvalid;
	}

	// nullptr MovementBase
	OutLocation = FVector::ZeroVector;
	OutQuat = FQuat::Identity;
	return false;
}

bool FCFrameBasedMoveUtils::TransformBasedDirectionToWorld(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector LocalDirection, FVector& OutDirectionWorldSpace)
{
	FVector IgnoredLocation;
	FQuat BaseQuat;
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ IgnoredLocation, /*out*/ BaseQuat))
	{
		TransformDirectionToWorld(BaseQuat, LocalDirection, OutDirectionWorldSpace);
		return true;
	}

	return false;
}

bool FCFrameBasedMoveUtils::TransformWorldDirectionToBased(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector WorldSpaceDirection, FVector& OutLocalDirection)
{
	FVector IgnoredLocation;
	FQuat BaseQuat;
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ IgnoredLocation, /*out*/ BaseQuat))
	{
		TransformDirectionToLocal(BaseQuat, WorldSpaceDirection, OutLocalDirection);
		return true;
	}

	return false;
}

void FCFrameBasedMoveUtils::TransformDirectionToWorld(FQuat BaseQuat, FVector LocalDirection, FVector& OutDirectionWorldSpace)
{
	OutDirectionWorldSpace = BaseQuat.RotateVector(LocalDirection);
}

void FCFrameBasedMoveUtils::TransformDirectionToLocal(FQuat BaseQuat, FVector WorldSpaceDirection, FVector& OutLocalDirection)
{
	OutLocalDirection = BaseQuat.UnrotateVector(WorldSpaceDirection);
}

bool FCFrameBasedMoveUtils::TransformBasedLocationToWorld(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector LocalLocation, FVector& OutLocationWorldSpace)
{
	FVector BaseLocation;
	FQuat BaseQuat;

	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ BaseLocation, /*out*/ BaseQuat))
	{
		TransformLocationToWorld(BaseLocation, BaseQuat, LocalLocation, OutLocationWorldSpace);
		return true;
	}

	return false;
}


bool FCFrameBasedMoveUtils::TransformWorldLocationToBased(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector WorldSpaceLocation, FVector& OutLocalLocation)
{
	FVector BaseLocation;
	FQuat BaseQuat;
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ BaseLocation, /*out*/ BaseQuat))
	{
		TransformLocationToLocal(BaseLocation, BaseQuat, WorldSpaceLocation, OutLocalLocation);
		return true;
	}

	return false;
}

void FCFrameBasedMoveUtils::TransformLocationToWorld(FVector BasePos, FQuat BaseQuat, FVector LocalLocation, FVector& OutLocationWorldSpace)
{
	OutLocationWorldSpace = FTransform(BaseQuat, BasePos).TransformPositionNoScale(LocalLocation);
}

void FCFrameBasedMoveUtils::TransformLocationToLocal(FVector BasePos, FQuat BaseQuat, FVector WorldSpaceLocation, FVector& OutLocalLocation)
{
	OutLocalLocation = FTransform(BaseQuat, BasePos).InverseTransformPositionNoScale(WorldSpaceLocation);
}