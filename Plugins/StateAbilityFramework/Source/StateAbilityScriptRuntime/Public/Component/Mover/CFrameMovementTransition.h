// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Component/Mover/CFrameMovementTypes.h"
#include "Component/Mover/CFrameMovementContext.h"

#include "CFrameMovementTransition.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCFrameMoveModeTransition, Log, Verbose);

class UCFrameMovementMode;
struct FCFrameMovementContext;

UCLASS(Abstract, BlueprintType)
class UCFrameMoveModeTransition : public UObject
{
	GENERATED_BODY()
public:
	UCFrameMoveModeTransition();
	UCFrameMoveModeTransition(UClass* InFromModeClass, UClass* InToModeClass);

	virtual bool OnEvaluate(FCFrameMovementContext& Context) const;

protected:
	UPROPERTY()
	TSubclassOf<UCFrameMovementMode> FromModeClass;
	UPROPERTY()
	TSubclassOf<UCFrameMovementMode> ToModeClass;
};

