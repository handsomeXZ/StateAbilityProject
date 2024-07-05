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

struct FCommandFrameInputFrame;
struct FCommandFrameInputAtom;
class ACommandFrameNetChannel;


enum EDeltaNetPacketType
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
 * uint32 ObjectPoolRange 用于映射 ScriptArchetype
 * uint32 ChunkDataSize_0
 * uint32 ChunkDataSize_1
 * uint32 ChunkDataSize_2
 * ......
 *
 */

namespace DeltaNetPacketUtils
{
	// 必须严格按照PacketType顺序执行

	void BuildPacket_Fault_FrameExpiry(FCommandFrameDeltaNetPacket& Packet);

	template<EDeltaNetPacketType Type>
	bool NetSerialize(FCommandFrameDeltaNetPacket& Packet, bool bLocal) { return true; }
}

USTRUCT()
struct FCommandFrameDeltaNetPacket
{
	GENERATED_BODY()
public:
	FCommandFrameDeltaNetPacket();
	FCommandFrameDeltaNetPacket(uint32 InServerCommandFrame, uint32 InPrevServerCommandFrame, EDeltaNetPacketType InPacketType, ACommandFrameNetChannel* InChannel);

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	uint32 ServerCommandFrame;
	uint32 PrevServerCommandFrame;
	ACommandFrameNetChannel* Channel;
	EDeltaNetPacketType PacketType;

	bool bLocal;

	TArray<UObject*> ObjectPool;

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
	FCommandFrameInputNetPacket(TCircularQueueView<FCommandFrameInputFrame>& InputFrames);

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
	
	// 冗余读写
	void WriteRedundantData(TCircularQueueView<FCommandFrameInputFrame>& InputFrames);
	void ReadRedundantData(TFunction<FCommandFrameInputAtom*(uint32 CommandFrame, int32 DataCount)> AllocateData) const;

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