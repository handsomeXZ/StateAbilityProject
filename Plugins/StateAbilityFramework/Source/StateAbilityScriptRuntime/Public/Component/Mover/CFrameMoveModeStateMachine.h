// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

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

	UCFrameMovementMode* GetCurrentMode() { return CurrentMode; }
	FName GetCurrentModeName() { return CurrentModeName; }
protected:
	UPROPERTY(Transient)
	TMap<FName, FCFrameMoveModeInfo> ModeInfos;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCFrameLayeredMove>> LayeredMoves;

	UPROPERTY(Transient)
	TObjectPtr<UCFrameMovementMode> CurrentMode;
	FName CurrentModeName;
	UPROPERTY(Transient)
	TObjectPtr<UCFrameMovementMixer> CurrentMixer;
	UPROPERTY(Transient)
	FCFrameMovementContext Context;
private:
	
};

UCLASS(Abstract, BlueprintType)
class UCFrameMoveModeTransition : public UObject
{
	GENERATED_BODY()
	friend class UCFrameMoveModeStateMachine;
public:
	UCFrameMoveModeTransition() {}
	UCFrameMoveModeTransition(UClass* InFromModeClass, UClass* InToModeClass) {}

	//virtual FTransitionEvalResult OnEvaluate(const FCFrameMovementContext& Params) const {}

protected:
	UPROPERTY()
	TSubclassOf<UCFrameMovementMode> FromModeClass;
	UPROPERTY()
	TSubclassOf<UCFrameMovementMode> ToModeClass;
};

UCLASS(BlueprintType, meta = (DisplayName = "WalkToFall（Simple）"))
class UCFrameMoveModeTransition_WalkToFall : public UCFrameMoveModeTransition
{
	GENERATED_BODY()

public:
	//virtual FTransitionEvalResult OnEvaluate(const FCFrameMovementContext& Params) const override {}
};
