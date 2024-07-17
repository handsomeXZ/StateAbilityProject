// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Component/Mover/CFrameMovementTransition.h"

#include "CFrameFallingTransition.generated.h"


UCLASS(meta = (DisplayName = "FallToWalk"))
class UCFrameMoveModeTransition_FallToWalk : public UCFrameMoveModeTransition
{
	GENERATED_BODY()

public:
	UCFrameMoveModeTransition_FallToWalk();

	virtual bool OnEvaluate(FCFrameMovementContext& Context) const override;
};
