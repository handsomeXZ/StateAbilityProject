// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Component/Mover/CFrameMovementMode.h"
#include "Component/Mover/MoveLibrary/CFrameMovementUtils.h"
#include "Component/Mover/MoveLibrary/CFrameBasedMoveUtils.h"

#include "CFrameWalkingMode.generated.h"

struct FInputActionValue;
class UInputAction;

UCLASS(Abstract, Blueprintable, BlueprintType)
class UCFrameWalkingMode : public UCFrameMovementMode
{
	GENERATED_UCLASS_BODY()
public:
	bool bIsOnWalkableFloor = true;

	virtual void OnActivated() override;
	virtual void OnDeactivated() override;
	virtual void GenerateMove(FCFrameMovementContext& Context, FCFrameProposedMove& OutProposedMove) override;
	virtual void Execute(FCFrameMovementContext& Context) override;
	virtual void OnClientRewind() override;

protected:
	void OnMoveTriggered(const FInputActionValue& Value);
	void OnMoveCompleted(const FInputActionValue& Value);

	void CaptureFinalState(FCFrameMovementContext& Context, bool bDidAttemptMovement, const FFloorCheckResult& FloorResult) const;

	FVector ConsumeControlInputVector();
	FCFrameRelativeBaseInfo UpdateFloorAndBaseInfo(FCFrameMovementContext& Context, const FFloorCheckResult& FloorResult) const;
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	ECFrameMoveInputType MoveInputType = ECFrameMoveInputType::Velocity;
	/**
	 * If true, rotate the Character toward the direction the actor is moving
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bOrientRotationToMovement = true;
	/**
	 * If true, the actor will continue orienting towards the last intended orientation (from input) even after movement intent input has ceased.
	 * This makes the character finish orienting after a quick stick flick from the player.  If false, character will not turn without input.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bMaintainLastInputOrientation = false;
	// Whether or not we author our movement inputs relative to whatever base we're standing on, or leave them in world space
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bUseBaseRelativeMovement = true;
	// If true, the actor will remain vertical despite any rotation applied to the actor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bShouldRemainVertical = true;





	/** Default max linear rate of deceleration when there is no controlled input */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
	float Deceleration = 4000.f;
	/** Default max linear rate of acceleration for controlled input. May be scaled based on magnitude of input. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
	float Acceleration = 4000.f;
	/** Maximum rate of turning rotation (degrees per second). Negative numbers indicate instant rotation and should cause rotation to snap instantly to desired direction. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General", meta = (ClampMin = "-1", UIMin = "0", ForceUnits = "degrees/s"))
	float TurningRate = 500.f;
	/** Speeds velocity direction changes while turning, to reduce sliding */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "Multiplier"))
	float TurningBoost = 8.f;
	/** Maximum speed in the movement plane */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float MaxSpeed = 800.f;





	/**
	 * Setting that affects movement control. Higher values allow faster changes in direction. This can be used to simulate slippery
	 * surfaces such as ice or oil by lowering the value (possibly based on the material the actor is standing on).
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General|Friction", meta = (ClampMin = "0", UIMin = "0"))
	float GroundFriction = 8.0f;
	/**
	  * If true, BrakingFriction will be used to slow the character to a stop (when there is no Acceleration).
	  * If false, braking uses the same friction passed to CalcVelocity() (ie GroundFriction when walking), multiplied by BrakingFrictionFactor.
	  * This setting applies to all movement modes; if only desired in certain modes, consider toggling it when movement modes change.
	  * @see BrakingFriction
	  */
	UPROPERTY(Category = "General|Friction", EditDefaultsOnly, BlueprintReadWrite)
	uint8 bUseSeparateBrakingFriction : 1;
	/**
	 * Friction (drag) coefficient applied when braking (whenever Acceleration = 0, or if character is exceeding max speed); actual value used is this multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * Braking is composed of friction (velocity-dependent drag) and constant deceleration.
	 * This is the current value, used in all movement modes; if this is not desired, override it or bUseSeparateBrakingFriction when movement mode changes.
	 * @note Only used if bUseSeparateBrakingFriction setting is true, otherwise current friction such as GroundFriction is used.
	 * @see bUseSeparateBrakingFriction, BrakingFrictionFactor, GroundFriction, BrakingDecelerationWalking
	 */
	UPROPERTY(Category = "General|Friction", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0", EditCondition = "bUseSeparateBrakingFriction"))
	float BrakingFriction = 8.0f;
	/**
	 * Factor used to multiply actual value of friction used when braking.
	 * This applies to any friction value that is currently used, which may depend on bUseSeparateBrakingFriction.
	 * @note This is 2 by default for historical reasons, a value of 1 gives the true drag equation.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General|Friction", meta = (ClampMin = "0", UIMin = "0"))
	float BrakingFrictionFactor = 2.0f;





	/** Walkable slope angle, represented as cosine(max slope angle) for performance reasons. Ex: for max slope angle of 30 degrees, value is cosine(30 deg) = 0.866 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ground Movement")
	float MaxWalkSlopeCosine = 0.71f;
	/** Max distance to scan for floor surfaces under a Mover actor */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ground Movement", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm"))
	float FloorSweepDistance = 40.0f;
	/** Mover actors will be able to step up onto or over obstacles shorter than this */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ground Movement", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm"))
	float MaxStepHeight = 40.0f;

private:
	FVector ControlInputVector = FVector::ZeroVector;

	uint32 MoveTriggeredHandle;
	uint32 MoveCompletedHandle;

	FVector LastAffirmativeMoveInput = FVector::ZeroVector;	// Movement input (intent or velocity) the last time we had one that wasn't zero
};