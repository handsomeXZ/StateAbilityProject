#include "Component/Mover/CFrameMoveStateAdapter.h"

#include "Component/CFrameMoverComponent.h"
#include "Component/Mover/MoveLibrary/CFrameBasedMoveUtils.h"

#if WITH_EDITOR
#include "Debug/DebugUtils.h"
#endif

UCFrameMoveStateAdapter::UCFrameMoveStateAdapter()
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

void UCFrameMoveStateAdapter::Init(UCFrameMoverComponent* InMoverComp)
{
	MoverComponent = InMoverComp;
}

void UCFrameMoveStateAdapter::SetMovementBase(UPrimitiveComponent* Base, FName BaseBone)
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

void UCFrameMoveStateAdapter::BeginMoveFrame(float DeltaTime, uint32 RCF, uint32 ICF)
{
	LastFrameLocation = GetLocation_WorldSpace();

	LastFrameDeltaTime = DeltaTime;
}

void UCFrameMoveStateAdapter::UpdateMoveFrame()
{
	LastFrameMoveDelta = GetLocation_WorldSpace() - LastFrameLocation;
	LastFrameLocation = GetLocation_WorldSpace();
}

void UCFrameMoveStateAdapter::EndMoveFrame(float DeltaTime, uint32 RCF, uint32 ICF)
{
	

#if WITH_EDITOR
	FColor NetColor;
	if (GetWorld()->GetNetMode() == NM_Client)
	{
		NetColor = FColor::Green;
	}
	else if (GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		NetColor = FColor::White;
	}
	else
	{
		NetColor = FColor::Blue;
	}
	FCFDebugHelper::Get().DrawDebugCircle_3D(GetLocation_WorldSpace(), 50.0f, 32, NetColor, true, 3.0f, 1, 1.0f, GetRight(), GetForward(), true);
#endif
}

FVector UCFrameMoveStateAdapter::GetLocation_WorldSpace() const
{
	if (MovementBase)
	{
		return FTransform(MovementBaseQuat, MovementBasePos).TransformPositionNoScale(GetLocation_BaseSpace());
	}

	return GetLocation_BaseSpace(); // if no base, assumed to be in world space
}

FVector UCFrameMoveStateAdapter::GetVelocity_WorldSpace() const
{
	if (MovementBase)
	{
		return MovementBaseQuat.RotateVector(GetVelocity_BaseSpace());
	}

	return GetVelocity_BaseSpace(); // if no base, assumed to be in world space
}

FVector UCFrameMoveStateAdapter::GetVelocity_BaseSpace() const
{
	return LastFrameDeltaTime > FLOAT_NORMAL_THRESH ? (LastFrameMoveDelta / LastFrameDeltaTime) : FVector::ZeroVector;
}

FRotator UCFrameMoveStateAdapter::GetOrientation_WorldSpace() const
{
	if (MovementBase)
	{
		return (MovementBaseQuat * FQuat(GetOrientation_BaseSpace())).Rotator();
	}

	return GetOrientation_BaseSpace(); // if no base, assumed to be in world space
}

//////////////////////////////////////////////////////////////////////////
// UCFrameMoveStateAdapter_Pawn
void UCFrameMoveStateAdapter_Pawn::Init(UCFrameMoverComponent* InMoverComp)
{
	Super::Init(InMoverComp);

	StateOwner = CastChecked<APawn>(InMoverComp->GetOuter());
}

FRotator UCFrameMoveStateAdapter_Pawn::GetControlRotation() const
{
	return StateOwner->GetControlRotation();
}

FVector UCFrameMoveStateAdapter_Pawn::GetLocation_BaseSpace() const
{
	return StateOwner->GetActorLocation();
}

FRotator UCFrameMoveStateAdapter_Pawn::GetOrientation_BaseSpace() const
{
	return StateOwner->GetActorRotation();
}

FVector UCFrameMoveStateAdapter_Pawn::GetUp() const
{
	return StateOwner->GetActorUpVector();
}

FVector UCFrameMoveStateAdapter_Pawn::GetRight() const
{
	return StateOwner->GetActorRightVector();
}

FVector UCFrameMoveStateAdapter_Pawn::GetForward() const
{
	return StateOwner->GetActorForwardVector();
}