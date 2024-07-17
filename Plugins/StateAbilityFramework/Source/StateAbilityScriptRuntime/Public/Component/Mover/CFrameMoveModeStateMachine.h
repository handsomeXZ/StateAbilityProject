// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Component/Mover/CFrameMovementTypes.h"
#include "Component/Mover/CFrameMovementContext.h"

#include "CFrameMoveModeStateMachine.generated.h"

struct FCFrameMovementConfig;
class UCFrameMovementMixer;
class UCFrameMovementMode;
class UCFrameMoveModeTransition;

USTRUCT()
struct FCFrameMoveModeInfo
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UCFrameMovementMode> ModeInstance;
	UPROPERTY()
	TArray<FCFrameModeTransitionLink> TransitionLinks;
};

UCLASS()
class UCFrameMoveModeStateMachine : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	void Init(FCFrameMovementConfig& Config);
	void FixedTick(float DeltaTime, uint32 RCF, uint32 ICF);
	void UpdateTransition();
	void OnClientRewind();

	const FCFrameMovementContext& GetMovementContext() { return Context; }
	UCFrameMovementMode* GetCurrentMode() { return CurrentMode; }
	FName GetCurrentModeName() { return CurrentModeName; }

protected:
	void RecordMovementSnapshot();

protected:
	UPROPERTY(Transient)
	TMap<FName, FCFrameMoveModeInfo> ModeInfos;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCFrameLayeredMove>> LayeredMoves;

	UPROPERTY(Transient)
	TObjectPtr<UCFrameMovementMode> CurrentMode;
	UPROPERTY(Transient)
	FName CurrentModeName;
	UPROPERTY(Transient)
	TObjectPtr<UCFrameMovementMixer> CurrentMixer;
	UPROPERTY(Transient)
	FCFrameMovementContext Context;
private:
	
};
