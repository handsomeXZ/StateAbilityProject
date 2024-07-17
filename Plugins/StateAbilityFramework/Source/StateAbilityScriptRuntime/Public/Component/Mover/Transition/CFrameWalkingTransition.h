// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Component/Mover/CFrameMovementTransition.h"

#include "CFrameWalkingTransition.generated.h"


UCLASS(meta = (DisplayName = "WalkToFall"))
class UCFrameMoveModeTransition_WalkToFall : public UCFrameMoveModeTransition
{
	GENERATED_BODY()

public:
	UCFrameMoveModeTransition_WalkToFall();

	virtual bool OnEvaluate(FCFrameMovementContext& Context) const override;
};
