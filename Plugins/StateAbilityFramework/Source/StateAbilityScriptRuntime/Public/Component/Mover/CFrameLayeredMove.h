// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "CFrameLayeredMove.generated.h"

struct FCFrameMovementContext;
struct FCFrameProposedMove;

UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew)
class STATEABILITYSCRIPTRUNTIME_API UCFrameLayeredMove : public UObject
{
	GENERATED_BODY()
public:
	virtual void GenerateMove(FCFrameMovementContext& Context, FCFrameProposedMove& OutProposedMove) {}
};