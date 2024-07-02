// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "CommandFrameDelta.generated.h"

USTRUCT()
struct FCommandFrameDelta
{
	GENERATED_BODY()
public:

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) { return true; }

	
};


template<> struct TStructOpsTypeTraits<FCommandFrameDelta> : public TStructOpsTypeTraitsBase2<FCommandFrameDelta>
{
	enum
	{
		WithNetSerializer = true,
	};
};