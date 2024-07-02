// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "MoveLibrary/FloorQueryUtils.h"

#include "Component/Mover/CFrameProposedMove.h"

#include "CFrameMovementContext.generated.h"

class UCFrameMoveModeTransition;
class UCFrameMoveStateProvider;
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
	TObjectPtr<UCFrameMoveStateProvider> MoveStateProvider;
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
struct FCFrameMovementContext
{
	GENERATED_BODY()
public:
	void Init(float InDeltaTime, uint32 InRCF, uint32 InICF);
	void ResetFrameData();	// 仅清除每帧的临时数据
	void ResetAllData();	// 清除所有数据（包含跨帧数据）

	// 临时数据
	float DeltaTime;
	uint32 RCF;
	uint32 ICF;

	FCFrameProposedMove CombinedMove;

	// 跨帧数据
	FFloorCheckResult LastFloorResult;

};