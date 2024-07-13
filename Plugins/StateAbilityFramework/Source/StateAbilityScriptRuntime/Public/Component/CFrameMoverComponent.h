// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Net/CommandFrameNetTypes.h"
#include "Component/Mover/CFrameMovementTypes.h"
#include "Component/Mover/CFrameMovementContext.h"

#include "CFrameMoverComponent.generated.h"

class UCFrameMovementMode;
class UCFrameLayeredMoveBase;
class UCommandFrameManager;
class UCFrameMoveModeStateMachine;

UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class STATEABILITYSCRIPTRUNTIME_API UCFrameMoverComponent : public UActorComponent, public ICommandFrameNetProcedure
{
	GENERATED_BODY()

public:
	UCFrameMoverComponent(const FObjectInitializer& ObjectInitializer);

	static const FVector DefaultGravityAccel;		// Fallback gravity if not determined by the component or world (cm/s^2)
	static const FVector DefaultUpDir;				// Fallback up direction if not determined by the component or world (normalized)

	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void OnRegister() override;
	virtual void BeginPlay() override;

	virtual void FixedTick(float DeltaTime, uint32 RCF, uint32 ICF);
	virtual void OnHandleImpact(const FHitResult& Hit, const FName ModeName, const FVector& MoveDelta);

	// ICommandFrameNetProcedure
	virtual void OnServerNetSync(FNetProcedureSyncParam& SyncParam) override;
	virtual void OnClientNetSync(FNetProcedureSyncParam& SyncParam, bool& bNeedRewind) override;
	virtual void OnClientRewind() override;
	// ~ICommandFrameNetProcedure

	void HandleImpact(const FHitResult& Hit, const FName ModeName = NAME_None, const FVector& MoveDelta = FVector::ZeroVector);
public:
	// Get the current acceleration due to gravity (cm/s^2) in worldspace
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = Mover)
	FVector GetGravityAcceleration() const;

	// Get the normalized direction considered "up" in worldspace. Typically aligned with gravity, and typically determines the plane an actor tries to move along.
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = Mover)
	FVector GetUpDirection() const;

	USceneComponent* GetUpdatedComponent() const { return UpdatedComponent; }
	UPrimitiveComponent* GetPrimitiveComponent() const { return UpdatedCompAsPrimitive; }
	FCFrameMovementConfig& GetMovementConfig() { return MovementConfig; }
	UCommandFrameManager* GetCommandFrameManager();
protected:
	// Basic "Update Component/Ticking"
	void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);
	void UpdateTickRegistration();

	// 检查客户端是否超过了允许的位置误差
	bool CheckClientExceedsAllowablePositionError(const FVector& ServerWorldLocation);
	void AdjustClientPosition(FVector WorldLocation, FVector WorldVelocity, FRotator WorldOrientation, UPrimitiveComponent* MovementBase, FName MovementBaseBoneName);
protected:
	//////////////////////////////////////////////////////////////////////////
	// CallBack
	UFUNCTION()
	virtual void OnPhysicsVolumeChanged(class APhysicsVolume* NewVolume) {}
	UFUNCTION()
	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {}

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Move)
	FCFrameMovementConfig MovementConfig;

	// Whether or not gravity is overridden on this actor. Otherwise, fall back on world settings. See @SetGravityOverride
	UPROPERTY(EditDefaultsOnly, Category = "Mover|Gravity")
	bool bHasGravityOverride = false;

	// cm/s^2, only meaningful if @bHasGravityOverride is enabled. Set @SetGravityOverride
	UPROPERTY(EditDefaultsOnly, Category = "Mover|Gravity", meta = (ForceUnits = "cm/s^2"))
	FVector GravityAccelOverride;

private:
	bool bInInitializeComponent;
	bool bInOnRegister;

	/** This is the component that's actually being moved. Typically it is the Actor's root component and often a collidable primitive. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> UpdatedComponent;

	/** UpdatedComponent, cast as a UPrimitiveComponent. May be invalid if UpdatedComponent was null or not a UPrimitiveComponent. */
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> UpdatedCompAsPrimitive;

	/** The main visual component associated with this Mover actor, typically a mesh and typically parented to the UpdatedComponent. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> PrimaryVisualComponent;

	UPROPERTY(Transient)
	UCommandFrameManager* CFrameManager;

	UPROPERTY(Transient)
	TObjectPtr<UCFrameMoveModeStateMachine> ModeFSM;
};