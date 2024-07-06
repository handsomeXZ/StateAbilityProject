// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Net/CommandFrameNetTypes.h"

#include "Net/Packet/CommandFramePacket.h"
#include "PrivateAccessor.h"

#include "CommandFrameNetChannel.generated.h"

class UCommandFrameManager;

UENUM()
enum class ECommandFrameNetChannelState : uint8
{
	Unkown,
	Normal,		// 正常
	WaitCatch,	// Server等待Client追赶
};

/**
 * 通用CFrameNetChannel，支持的同步：Movement、StateAbilityScript。
 * 
 * @TODO：需要考虑AOI优化
 */
UCLASS()
class ADefaultCommandFrameNetChannel : public ACommandFrameNetChannelBase
{
	GENERATED_UCLASS_BODY()

	PRIVATE_DECLARE_NAMESPACE(FBitReader, int64, Pos)
	friend struct FCommandFrameDeltaNetPacket;
public:
	//virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;
	virtual void BeginPlay() override;

	virtual void FixedTick(float DeltaTime, uint32 RCF, uint32 ICF) override;
	virtual void RegisterCFrameManager(UCommandFrameManager* CommandFrameManager) override { CFrameManager = CommandFrameManager; }
	
	virtual void ClientSend_CommandFrameInputNetPacket(FCommandFrameInputNetPacket& InputNetPacket) override;
	virtual void ServerSend_CommandFrameDeltaNetPacket(FCommandFrameDeltaNetPacket& DeltaNetPacket) override;

	virtual uint32 GetLastServerCommandFrame() override { return LastServerCommandFrame; }
	virtual ICommandFrameNetProcedure* GetNetPacketProcedure(EDeltaNetPacketType NetPacketType) override;
	virtual void RegisterNetPacketProcedure(EDeltaNetPacketType NetPacketType, UObject* Procedure) override;
private:

	UFUNCTION(Server, Unreliable)
	void ServerReceive_CommandFrameInputNetPacket(const FCommandFrameInputNetPacket& InputNetPacket);

	UFUNCTION(NetMulticast, reliable)
	void ClientReceive_CommandFrameDeltaNetPacket(const FCommandFrameDeltaNetPacket& DeltaNetPacket);

	UCommandFrameManager* GetCommandFrameManager();

	//////////////////////////////////////////////////////////////////////////
	// 处理数据包前缀（只要收到数据就处理，不保证有序）
	void ProcessDeltaPrefix(const FCommandFrameDeltaNetPacket& DeltaNetPacket, FBitReader& BitReader);
	void ResetCommandFrame(uint32 ServerCommandFrame, uint32 PrevServerCommandFrame);

	//////////////////////////////////////////////////////////////////////////
	// 处理数据包主体（仅处理有序的数据包）
	void ProcessDeltaPackaged(const FCommandFrameDeltaNetPacket& DeltaNetPacket, FBitReader& BitReader);

	//////////////////////////////////////////////////////////////////////////
	// 处理乱序包
	void VerifyUnorderedPackets();

	void ShrinkUnorderedPackets();
	void RemoveUnorderedPacket(uint32 CommandFrame);
	
private:
	uint32 LastServerCommandFrame;									// Client收到的最新的来自服务器的CF，用于确保Delta按序处理
	uint32 LastClientCommandFrame;									// Server收到的最新的来自客户端的CF
	TMap<uint32, FCommandFrameDeltaNetPacket> UnorderedPackets;		// 乱序而缓存的包

	ECommandFrameNetChannelState NetChannelState;

	UPROPERTY(Transient)
	UCommandFrameManager* CFrameManager;

	//////////////////////////////////////////////////////////////////////////
	TMap<EDeltaNetPacketType, TWeakObjectPtr<UObject>> NetPacketProcedures;


	//////////////////////////////////////////////////////////////////////////
};

namespace DeltaNetPacketUtils
{
	template<>
	void NetSerialize<EDeltaNetPacketType::Movement>(FCommandFrameDeltaNetPacket& NetPacket, FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

	template<>
	void NetSerialize<EDeltaNetPacketType::StateAbilityScript>(FCommandFrameDeltaNetPacket& NetPacket, FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
}