// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Component/Mover/CFrameMovementMode.h"
#include "Component/Mover/MoveLibrary/CFrameMovementUtils.h"
#include "Component/Mover/MoveLibrary/CFrameBasedMoveUtils.h"

#include "CFrameFallingMode.generated.h"

struct FInputActionValue;
class UInputAction;

UCLASS(Abstract, Blueprintable, BlueprintType)
class UCFrameFallingMode : public UCFrameMovementMode
{
	GENERATED_UCLASS_BODY()
public:
	mutable bool bIsOnWalkableFloor = false;

	virtual void OnActivated() override;
	virtual void OnDeactivated() override;
	virtual void GenerateMove(FCFrameMovementContext& Context, FCFrameProposedMove& OutProposedMove) override;
	virtual void Execute(FCFrameMovementContext& Context) override;
	virtual void OnClientRewind() override;

protected:
	void OnMoveTriggered(const FInputActionValue& Value);
	void OnMoveCompleted(const FInputActionValue& Value);

	void CaptureFinalState(FCFrameMovementContext& Context, const FFloorCheckResult& FloorResult) const;

	FVector ConsumeControlInputVector();
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





	/**
	 * When falling, amount of movement control available to the actor.
	 * 0 = no control, 1 = full control
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingMode", meta = (ClampMin = "0", ClampMax = "1.0"))
	float AirControlPercentage = 0.4f;
	/**
	 * Deceleration to apply to air movement when falling slower than terminal velocity.
	 * Note: This is NOT applied to vertical velocity, only movement plane velocity
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingMode", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float FallingDeceleration = 200.0f;
	/**
	 * Deceleration to apply to air movement when falling faster than terminal velocity
	 * Note: This is NOT applied to vertical velocity, only movement plane velocity
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingMode", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float OverTerminalSpeedFallingDeceleration = 800.0f;
	/**
	 * If the actor's movement plane velocity is greater than this speed falling will start applying OverTerminalSpeedFallingDeceleration instead of FallingDeceleration
	 * The expected behavior is to set OverTerminalSpeedFallingDeceleration higher than FallingDeceleration so the actor will slow down faster
	 * when going over TerminalMovementPlaneSpeed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingMode", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float TerminalMovementPlaneSpeed = 1500.0f;
	/** When exceeding maximum vertical speed, should it be enforced via a hard clamp? If false, VerticalFallingDeceleration will be used for a smoother transition to the terminal speed limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingMode")
	bool bShouldClampTerminalVerticalSpeed = true;
	/** Deceleration to apply to vertical velocity when it's greater than TerminalVerticalSpeed. Only used if bShouldClampTerminalVerticalSpeed is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingMode", meta = (EditCondition = "!bShouldClampTerminalVerticalSpeed", ClampMin = "0", ForceUnits = "cm/s^2"))
	float VerticalFallingDeceleration = 2000.0f;
	/**
	 * If the actors vertical velocity is greater than this speed VerticalFallingDeceleration will be applied to vertical velocity
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingMode", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float TerminalVerticalSpeed = 2000.0f;




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




	/** Walkable slope angle, represented as cosine(max slope angle) for performance reasons. Ex: for max slope angle of 30 degrees, value is cosine(30 deg) = 0.866 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ground Movement")
	float MaxWalkSlopeCosine = 0.71f;
	/** Max distance to scan for floor surfaces under a Mover actor */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ground Movement", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm"))
	float FloorSweepDistance = 40.0f;

private:
	FVector ControlInputVector = FVector::ZeroVector;

	uint32 MoveTriggeredHandle;
	uint32 MoveCompletedHandle;

	FVector LastAffirmativeMoveInput = FVector::ZeroVector;	// Movement input (intent or velocity) the last time we had one that wasn't zero
};