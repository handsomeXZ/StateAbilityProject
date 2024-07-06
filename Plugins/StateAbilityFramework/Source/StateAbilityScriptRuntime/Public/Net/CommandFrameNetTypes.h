// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "UObject/Interface.h"
#include "GameFramework/Info.h"

#include "CommandFrameNetTypes.generated.h"

struct FCommandFrameInputNetPacket;
struct FCommandFrameDeltaNetPacket;
class UCommandFrameManager;
enum EDeltaNetPacketType : uint32;

struct FNetProcedureSyncParam
{
	FNetProcedureSyncParam(FCommandFrameDeltaNetPacket& InNetPacket, FArchive& InAr, UPackageMap* InMap, bool& InOutSuccess)
		: NetPacket(InNetPacket)
		, Ar(InAr)
		, Map(InMap)
		, bOutSuccess(InOutSuccess)
	{}

	FNetProcedureSyncParam() = delete;
	FNetProcedureSyncParam(const FNetProcedureSyncParam& Param) = delete;
	FNetProcedureSyncParam& operator=(const FNetProcedureSyncParam& Param) = delete;

	FCommandFrameDeltaNetPacket& NetPacket;
	FArchive& Ar;
	UPackageMap* Map;
	bool& bOutSuccess;
};

/**
 * 负责为命令帧网络同步的通道提供特化的同步内容处理程序
 */
UINTERFACE(BlueprintType)
class UCommandFrameNetProcedure : public UInterface
{
	GENERATED_BODY()
};
class STATEABILITYSCRIPTRUNTIME_API ICommandFrameNetProcedure
{
	GENERATED_BODY()
public:
	virtual void OnNetSync(FNetProcedureSyncParam& SyncParam) {}
};

/**
 * 负责每个玩家的命令帧网络同步的通道。
 * 之所以要单独实现这么一个类，主要是为了避免SubObject网络同步的开销。
 */
UCLASS()
class STATEABILITYSCRIPTRUNTIME_API ACommandFrameNetChannelBase : public AInfo
{
	GENERATED_BODY()
public:
	virtual void FixedTick(float DeltaTime, uint32 RCF, uint32 ICF) {}
	virtual void RegisterCFrameManager(UCommandFrameManager* CommandFrameManager) {}

	virtual void ClientSend_CommandFrameInputNetPacket(FCommandFrameInputNetPacket& InputNetPacket) {}
	virtual void ServerSend_CommandFrameDeltaNetPacket(FCommandFrameDeltaNetPacket& DeltaNetPacket) {}

	virtual uint32 GetLastServerCommandFrame() { return 0; }
	virtual ICommandFrameNetProcedure* GetNetPacketProcedure(EDeltaNetPacketType NetPacketType) { return nullptr; }
	virtual void RegisterNetPacketProcedure(EDeltaNetPacketType NetPacketType, UObject* Procedure) {}
};