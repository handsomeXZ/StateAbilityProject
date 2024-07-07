// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "StructView.h"
#include "InstancedStruct.h"
#include "MoveLibrary/FloorQueryUtils.h"

#include "Component/Mover/CFrameProposedMove.h"

#include "CFrameMovementContext.generated.h"

class UCFrameMoveStateAdapter;
class UCFrameMoverComponent;
class UCommandFrameManager;

namespace CFrameContextDataKey
{
	const FName LastFloorResult = TEXT("LastFloor");
	const FName LastFoundDynamicMovementBase = TEXT("LastFoundDynamicMovementBase");
}

USTRUCT()
struct FCFrameMovementContextPersistentData
{
	GENERATED_BODY()
	UPROPERTY()
	bool bIsHidden = true;
	UPROPERTY()
	FInstancedStruct Data;
};

USTRUCT()
struct FCFrameMovementContext
{
	GENERATED_BODY()
	void Init(UCFrameMoverComponent* InMoverComp, float InDeltaTime, uint32 InRCF, uint32 InICF);
	void ResetFrameData();	// 仅清除每帧的临时数据
	void ResetAllData();	// 清除所有数据（包含跨帧数据）
	bool IsValid();

	//////////////////////////////////////////////////////////////////////////
	// 临时数据
	float DeltaTime;
	uint32 RCF;
	uint32 ICF;

	UCFrameMoverComponent* MoverComp;
	USceneComponent* UpdatedComponent;
	UPrimitiveComponent* UpdatedPrimitive;
	UCFrameMoveStateAdapter* MoveStateAdapter;

	UCommandFrameManager* CFrameManager;

	FCFrameProposedMove CombinedMove;

	//////////////////////////////////////////////////////////////////////////
	// 跨帧数据 
	template<typename T>
	void SetPersistentData(FName Key, T Data);
	template<typename T>
	bool GetPersistentData(FName Key, T& Data);
	FStructView GetPersistentData(FName Key);
	void RemovePersistentData(FName Key);
	void InvalidPersistentData(FName Key);
	bool HasValidPersistentData(FName Key);
	UPROPERTY()
	TMap<FName, FCFrameMovementContextPersistentData> PersistentDataBuffer;
	//////////////////////////////////////////////////////////////////////////

};


template<typename T>
void FCFrameMovementContext::SetPersistentData(FName Key, T Data)
{
	FCFrameMovementContextPersistentData& PersistentData = PersistentDataBuffer.FindOrAdd(Key);
	PersistentData.bIsHidden = false;

	if (PersistentData.Data.IsValid())
	{
		PersistentData.Data.GetMutable<T>() = Data;
	}
	else
	{
		PersistentData.Data.InitializeAs(T::StaticStruct(), (uint8*)&Data);
	}
}

template<typename T>
bool FCFrameMovementContext::GetPersistentData(FName Key, T& Data)
{
	FCFrameMovementContextPersistentData& PersistentData = PersistentDataBuffer.FindOrAdd(Key);
	if (PersistentData.bIsHidden || !PersistentData.Data.IsValid())
	{
		return false;
	}

	Data = PersistentData.Data.GetMutable<T>();
	return true;
}