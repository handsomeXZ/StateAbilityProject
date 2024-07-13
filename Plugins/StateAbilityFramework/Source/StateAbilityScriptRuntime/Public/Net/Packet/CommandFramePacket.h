// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Containers/BitArray.h"

#include "PrivateAccessor.h"
#include "Buffer/CircularQueueCore.h"

#include "CommandFramePacket.generated.h"

#ifndef CHARACTER_SERIALIZATION_PACKEDBITS_RESERVED_SIZE
#define CHARACTER_SERIALIZATION_PACKEDBITS_RESERVED_SIZE 1024
#endif

class ACommandFrameNetChannelBase;
class UNetConnection;
struct FCommandFrameInputFrame;
struct FCommandFrameInputAtom;

enum EDeltaNetPacketType : uint32
{
	None						= 0x00000000,
	Movement					= 1,
	StateAbilityScript			= 1 << 1,


	// 0x10000000 即以上都为Fault
	Fault_FrameExpiry			= 0x10000000,
};

/**
 * DeltaNetPacket:
 * 
 * - bool bLocal(Autonomous/Simulated)
 *
 * 
 * - [RAW DATA]
 * Autonomous:
 * -- bool bFault
 * -- uint32 ServerInputBufferNum
 *
 * Simulated:
 * --
 * 
 */

 /**
 * 
 * [RAW DATA StateAbilityScript]
 *
 * uint32 ChunkDataSize_0
 * uint32 ChunkDataSize_1
 * uint32 ChunkDataSize_2
 * ......
 *
 */

namespace DeltaNetPacketUtils
{
	// 必须严格按照PacketType顺序执行

	using FPrefixCallBackFunc = TFunction<void(int32 PrefixDataSize, uint32 ServerCommandBufferNum, bool bFault)>;

	void BuildPacket_Fault_FrameExpiry(FCommandFrameDeltaNetPacket& Packet);

	void SerializeDeltaPrefix(const FCommandFrameDeltaNetPacket& NetPacket, FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess, FPrefixCallBackFunc CallBack = FPrefixCallBackFunc());
	void SerializeDeltaPackaged(const FCommandFrameDeltaNetPacket& NetPacket, FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	template<EDeltaNetPacketType Type>
	void NetSync(const FCommandFrameDeltaNetPacket& NetPacket, FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	template<>
	void NetSync<EDeltaNetPacketType::Fault_FrameExpiry>(const FCommandFrameDeltaNetPacket& NetPacket, FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
}

USTRUCT()
struct FCommandFrameDeltaNetPacket
{
	GENERATED_BODY()
public:
	FCommandFrameDeltaNetPacket();
	FCommandFrameDeltaNetPacket(uint32 InServerCommandFrame, uint32 InPrevServerCommandFrame, EDeltaNetPacketType InPacketType, ACommandFrameNetChannelBase* InNetChannel);

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	uint32 ServerCommandFrame;
	uint32 PrevServerCommandFrame;
	ACommandFrameNetChannelBase* NetChannel;
	EDeltaNetPacketType PacketType;

	bool bLocal;

	UPROPERTY()
	class UPackageMap* PackageMap;

	TBitArray<TInlineAllocator<CHARACTER_SERIALIZATION_PACKEDBITS_RESERVED_SIZE / NumBitsPerDWORD>> RawData;
};

template<>
struct TStructOpsTypeTraits<FCommandFrameDeltaNetPacket> : public TStructOpsTypeTraitsBase2<FCommandFrameDeltaNetPacket>
{
	enum
	{
		WithNetSerializer = true,
		WithNetSharedSerialization = false,
	};
};

/**
 * @TODO：考虑允许启用BulkSerialize来优化
 */
USTRUCT()
struct FCommandFrameInputNetPacket
{
	GENERATED_BODY()

	PRIVATE_DECLARE_NAMESPACE(FBitReader, int64, Pos);

public:
	FCommandFrameInputNetPacket() {}
	FCommandFrameInputNetPacket(UNetConnection* Connection, TCircularQueueView<FCommandFrameInputFrame>& InputFrames);

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
	
	// 冗余读写
	void WriteRedundantData(UNetConnection* Connection, TCircularQueueView<FCommandFrameInputFrame>& InputFrames);
	void ReadRedundantData(UNetConnection* Connection, TFunction<uint8*(uint32 CommandFrame, int32 DataCount)> AllocateData) const;

	// 客户端的最新帧号
	uint32 ClientCommandFrame;

	TBitArray<TInlineAllocator<CHARACTER_SERIALIZATION_PACKEDBITS_RESERVED_SIZE / NumBitsPerDWORD>> RawData;
};

template<>
struct TStructOpsTypeTraits<FCommandFrameInputNetPacket> : public TStructOpsTypeTraitsBase2<FCommandFrameInputNetPacket>
{
	enum
	{
		WithNetSerializer = true,
	};
};