// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "CFrameMoveStateAdapter.generated.h"

class UCFrameMoverComponent;

/**
 * 确保 MovementMode 可以不用关心Actor的具体类型（Pawn、Character、Actor）。
 * 封装了获取各种移动状态信息的方法。
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew)
class STATEABILITYSCRIPTRUNTIME_API UCFrameMoveStateAdapter : public UObject
{
	GENERATED_BODY()
public:
	UCFrameMoveStateAdapter();

	virtual void Init(UCFrameMoverComponent* InMoverComp);
	virtual void SetMovementBase(UPrimitiveComponent* Base, FName BaseBone);
	virtual void BeginMoveFrame(float DeltaTime, uint32 RCF, uint32 ICF);
	virtual void UpdateMoveFrame();
	virtual void EndMoveFrame(float DeltaTime, uint32 RCF, uint32 ICF);

	// 相机朝向？
	virtual FRotator GetControlRotation() const { return FRotator(); }

	virtual FVector GetLocation_WorldSpace() const;
	// If there is no movement base set, these will be the same as world space
	virtual FVector GetLocation_BaseSpace() const { return FVector::ZeroVector; }

	virtual FVector GetVelocity_WorldSpace() const;
	virtual FVector GetVelocity_BaseSpace() const;

	virtual FRotator GetOrientation_WorldSpace() const;
	virtual FRotator GetOrientation_BaseSpace() const { return FRotator::ZeroRotator; }

	virtual FVector GetUp() const { return FVector::ZeroVector; }
	virtual FVector GetRight() const { return FVector::ZeroVector; }
	virtual FVector GetForward() const { return FVector::ZeroVector; }

	virtual UPrimitiveComponent* GetMovementBase() const { return MovementBase; }
	virtual FName GetMovementBaseBoneName() const { return MovementBaseBoneName; }
	virtual FVector GetMovementBasePos() const { return MovementBasePos; }
	virtual FQuat GetMovementBaseQuat() const { return MovementBaseQuat; }

protected:
	UPROPERTY(Transient)
	TObjectPtr<UCFrameMoverComponent> MoverComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> MovementBase;
	// Optional: for movement bases that are skeletal meshes, this is the bone we're based on. Only valid if MovementBase is set.
	UPROPERTY(Transient)
	FName MovementBaseBoneName;
	UPROPERTY(Transient)
	FVector MovementBasePos;
	UPROPERTY(Transient)
	FQuat MovementBaseQuat;

	// Velocity
	FVector LastFrameLocation;
	FVector LastFrameMoveDelta;
	float LastFrameDeltaTime;
};

UCLASS(Blueprintable, BlueprintType)
class STATEABILITYSCRIPTRUNTIME_API UCFrameMoveStateAdapter_Pawn : public UCFrameMoveStateAdapter
{
	GENERATED_BODY()
public:
	virtual void Init(UCFrameMoverComponent* InMoverComp) override;

	virtual FRotator GetControlRotation() const override;
	virtual FVector GetLocation_BaseSpace() const override;
	virtual FRotator GetOrientation_BaseSpace() const override;

	virtual FVector GetUp() const override;
	virtual FVector GetRight() const override;
	virtual FVector GetForward() const override;
private:
	UPROPERTY(Transient)
	TObjectPtr<APawn> StateOwner;
};