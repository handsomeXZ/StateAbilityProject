// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "CFrameMovementMixer.generated.h"

UCLASS(BlueprintType)
class UCFrameMovementMixer : public UObject
{
	GENERATED_BODY()

public:
	UCFrameMovementMixer() {}

	/** In charge of mixing proposed moves together. Is similar to MixLayeredMove but is only responsible for mixing proposed moves instead of layered moves. */
	virtual void MixProposedMoves(const FCFrameMovementContext& Context, const FCFrameProposedMove& MoveToMix, FCFrameProposedMove& OutCumulativeMove);

	/** In charge of mixing Layered Move proposed moves into a cumulative proposed move based on mix mode and priority.*/
	virtual void MixLayeredMove(const FCFrameMovementContext& Context, const UCFrameLayeredMove* ActiveLayerMove, const FCFrameProposedMove& MoveStep, FCFrameProposedMove& OutCumulativeMove) {}

	/** Resets all state used for mixing. Should be called before or after finished mixing moves. */
	virtual void ResetMixerState() {}

};