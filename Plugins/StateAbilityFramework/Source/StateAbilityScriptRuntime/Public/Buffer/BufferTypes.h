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

	uint32 CommandFrame;
	TArray<FCommandFrameInputAtom> InputQueue;
	TMap<FUniqueNetIdRepl, uint8> OrderCounter;

	FBitWriter SharedSerialization;

	void AddItem(const FUniqueNetIdRepl& Key, const TArray<FCommandFrameInputAtom>& ItemData);
	uint8* AllocateItem(const FUniqueNetIdRepl& Key, uint32 DataSize);
	void Reset(uint32 InCommandFrame);
	bool Verify(uint32 InCommandFrame);
	bool HasOwnerShip(const FUniqueNetIdRepl& Key);
};

struct FCommandFrameAttributeSnapshot
{
	uint32 CommandFrame;
	TMap<UScriptStruct*, uint8*> MemoryBuffer;

	void AddItem(const UScriptStruct* Key, const uint8*& ItemData) {}
	uint8* AllocateItem(const UScriptStruct* Key, uint32 DataSize) { return nullptr; }
	void Reset(uint32 InCommandFrame) {}
	bool Verify(uint32 InCommandFrame) { return false; }
	bool HasOwnerShip(const UScriptStruct* Key) { return false; }
};