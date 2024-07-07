// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "CFrameMovementTypes.generated.h"

class UCFrameMoveModeTransition;
class UCFrameMoveStateAdapter;
class UCFrameMovementMixer;
class UCFrameMovementMode;
class UCFrameLayeredMove;

/**
 * @TODO: 最好改为可视化编辑
 */
USTRUCT(BlueprintType)
struct FCFrameModeTransitionLink
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UCFrameMoveModeTransition> ModeTransition;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName FromMode;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ToMode;
};

USTRUCT(BlueprintType)
struct FCFrameMovementConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = Config)
	TObjectPtr<UCFrameMoveStateAdapter> MoveStateAdapter;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = Config)
	TObjectPtr<UCFrameMovementMixer> MovementMixer;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Config, meta = (FullyExpand = true))
	TMap<FName, TSubclassOf<UCFrameMovementMode>> MovementModes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = Config)
	TArray<TObjectPtr<UCFrameLayeredMove>> LayeredMoves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Config)
	FName DefaultModeName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Config)
	TArray<FCFrameModeTransitionLink> ModeTransitionLinks;
};

USTRUCT()
struct FCFrameMovementSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location;
	UPROPERTY()
	FVector Velocity;
	UPROPERTY()
	FRotator Orientation;

	UPROPERTY()
	TWeakObjectPtr<UPrimitiveComponent> MovementBase;

	// Optional: for movement bases that are skeletal meshes, this is the bone we're based on. Only valid if MovementBase is set.
	UPROPERTY()
	FName MovementBaseBoneName;

	UPROPERTY()
	FVector MovementBasePos;

	UPROPERTY()
	FQuat MovementBaseQuat;
};