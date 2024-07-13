// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "GameFramework/OnlineReplStructs.h"
#include "Serialization/BitWriter.h"
#include "UObject/Class.h"

#include "Net/Packet/CommandFrameInput.h"

struct FCommandFrameInputFrame
{
	FCommandFrameInputFrame();

	uint32 CommandFrame = 0;
	TArray<FCommandFrameInputAtom> InputQueue;
	TMap<FUniqueNetIdRepl, uint8> OrderCounter;

	FNetBitWriter SharedSerialization;

	void AddItem(const FUniqueNetIdRepl& Key, const TArray<FCommandFrameInputAtom>& ItemData);
	uint8* AllocateItem(const FUniqueNetIdRepl& Key, uint32 DataSize);
	void Reset(uint32 InCommandFrame);
	void Release(uint32 InCommandFrame);
	bool Verify(uint32 InCommandFrame);
	bool HasOwnerShip(const FUniqueNetIdRepl& Key);
};

/**
 * 不能存储UObject，因为其内存空间在Reset后并不会被实际释放，并且也不会有GC的引用保护。
 * 同样不建议使用有自定义内存管理的结构体。
 */
struct FCommandFrameAttributeSnapshot
{
	uint32 CommandFrame = 0;

	// 为什么不用FInstancedStruct？
	// 因为FInstancedStruct占用额外的空间，且在扩容等操作时会产生不必要的内存拷贝。
	TMap<const UScriptStruct*, uint8*> MemoryBuffer;
	TSet<const UScriptStruct*> UsedKey;

	FCommandFrameAttributeSnapshot();
	~FCommandFrameAttributeSnapshot() { Release(0); }
	FCommandFrameAttributeSnapshot(const FCommandFrameAttributeSnapshot& Param) = delete;
	FCommandFrameAttributeSnapshot& operator=(const FCommandFrameAttributeSnapshot& Param) = delete;

	void AddItem(const UScriptStruct* Key, const uint8* const& ItemData);
	uint8* AllocateItem(const UScriptStruct* Key, uint32 DataSize);
	void Reset(uint32 InCommandFrame);
	void Release(uint32 InCommandFrame);
	bool Verify(uint32 InCommandFrame);
	bool HasOwnerShip(const UScriptStruct* Key);
};