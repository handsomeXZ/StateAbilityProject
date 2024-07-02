// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "CommandFrameSetting.generated.h"


UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Command Frame"))
class UCommandFrameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public: 
	UPROPERTY(config, EditAnywhere, Category = Default)
	bool bEnableCommandFrameNetReplication;
};
