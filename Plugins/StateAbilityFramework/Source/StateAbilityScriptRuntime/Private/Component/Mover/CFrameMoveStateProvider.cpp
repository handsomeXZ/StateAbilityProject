#include "Component/Mover/CFrameMoveStateProvider.h"

#include "Component/CFrameMoverComponent.h"
#include "Component/Mover/MoveLibrary/CFrameBasedMoveUtils.h"

UCFrameMoveStateProvider::UCFrameMoveStateProvider()
	: MoverComponent(nullptr)
	, MovementBase(nullptr)
	, MovementBaseBoneName(NAME_None)
	, MovementBasePos(FVector::ZeroVector)
	, MovementBaseQuat(FQuat::Identity)
	, LastFrameLocation(FVector::ZeroVector)
	, LastFrameMoveDelta(FVector::ZeroVector)
	, LastFrameDeltaTime(0.0f)
{

}

void UCFrameMoveStateProvider::Init(UCFrameMoverComponent* InMoverComp)
{
	MoverComponent = InMoverComp;
}

void UCFrameMoveStateProvider::SetMovementBase(UPrimitiveComponent* Base, FName BaseBone)
{
	MovementBase = Base;
	MovementBaseBoneName = BaseBone;

	bool bDidGetBaseTransform = false;

	if (MovementBase)
	{
		bDidGetBaseTransform = FCFrameBasedMoveUtils::GetMovementBaseTransform(MovementBase, MovementBaseBoneName, OUT MovementBasePos, OUT MovementBaseQuat);
	}

	if (!bDidGetBaseTransform)
	{
		MovementBase = nullptr;
		MovementBaseBoneName = NAME_None;
		MovementBasePos = FVector::ZeroVector;
		MovementBaseQuat = FQuat::Identity;
	}
}

void UCFrameMoveStateProvider::BeginMoveFrame(float DeltaTime, uint32 RCF, uint32 ICF)
{
	LastFrameLocation = GetLocation_WorldSpace();

	// 这里不应该更新 LastFrameDeltaTime
}

void UCFrameMoveStateProvider::EndMoveFrame(float DeltaTime, uint32 RCF, uint32 ICF)
{
	LastFrameMoveDelta = GetLocation_WorldSpace() - LastFrameLocation;
	LastFrameLocation = GetLocation_WorldSpace();
	LastFrameDeltaTime = DeltaTime;
}

FVector UCFrameMoveStateProvider::GetLocation_WorldSpace() const
{
	if (MovementBase)
	{
		return FTransform(MovementBaseQuat, MovementBasePos).TransformPositionNoScale(GetLocation_BaseSpace());
	}

	return GetLocation_BaseSpace(); // if no base, assumed to be in world space
}

FVector UCFrameMoveStateProvider::GetVelocity_WorldSpace() const
{
	if (MovementBase)
	{
		return MovementBaseQuat.RotateVector(GetVelocity_BaseSpace());
	}

	return GetVelocity_BaseSpace(); // if no base, assumed to be in world space
}

FVector UCFrameMoveStateProvider::GetVelocity_BaseSpace() const
{
	return LastFrameDeltaTime > FLOAT_NORMAL_THRESH ? (LastFrameMoveDelta / LastFrameDeltaTime) : FVector::ZeroVector;
}

FRotator UCFrameMoveStateProvider::GetOrientation_WorldSpace() const
{
	if (MovementBase)
	{
		return (MovementBaseQuat * FQuat(GetOrientation_BaseSpace())).Rotator();
	}

	return GetOrientation_BaseSpace(); // if no base, assumed to be in world space
}

//////////////////////////////////////////////////////////////////////////
// UCFrameMoveStateProvider_Pawn
void UCFrameMoveStateProvider_Pawn::Init(UCFrameMoverComponent* InMoverComp)
{
	Super::Init(InMoverComp);

	StateOwner = CastChecked<APawn>(InMoverComp->GetOuter());
}

FRotator UCFrameMoveStateProvider_Pawn::GetControlRotation() const
{
	return StateOwner->GetControlRotation();
}

FVector UCFrameMoveStateProvider_Pawn::GetLocation_BaseSpace() const
{
	return StateOwner->GetActorLocation();
}

FRotator UCFrameMoveStateProvider_Pawn::GetOrientation_BaseSpace() const
{
	return StateOwner->GetActorRotation();
}