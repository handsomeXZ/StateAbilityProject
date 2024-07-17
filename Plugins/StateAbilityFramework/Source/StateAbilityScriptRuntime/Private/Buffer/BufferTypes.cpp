#include "Buffer/BufferTypes.h"

//////////////////////////////////////////////////////////////////////////
// FCommandFrameInputFrame
FCommandFrameInputFrame::FCommandFrameInputFrame()
	: CommandFrame(0)
{
	SharedSerialization.SetAllowResize(true);
}

void FCommandFrameInputFrame::AddItem(const FUniqueNetIdRepl& Key, const TArray<FCommandFrameInputAtom>& ItemData)
{
	// 每个Key只允许被加入一次
	// 如果ItemData数量为0，则不加入
	if (!OrderCounter.Contains(Key) && ItemData.Num())
	{
		InputQueue.Append(ItemData);

		OrderCounter.Add(Key, ItemData.Num());
	}
}

uint8* FCommandFrameInputFrame::AllocateItem(const FUniqueNetIdRepl& Key, uint32 DataSize)
{
	// 每个Key只允许被加入一次
	// 如果DataSize数量为0，则不加入
	if (!OrderCounter.Contains(Key) && DataSize)
	{
		OrderCounter.Add(Key, DataSize);

		int64 Pos = InputQueue.AddUninitialized(DataSize);

		return (uint8*)(InputQueue.GetData() + Pos);
	}
	return nullptr;
}

void FCommandFrameInputFrame::Reset(uint32 InCommandFrame)
{
	CommandFrame = InCommandFrame;
	InputQueue.Reset();

	// 通过查阅稀疏数组代码可以发现，在执行Reset后，NumFreeIndices会被归零，所以也就不会继续使用链表了，可以保持后续新加入的数据有序。
	// Empty 会释放空间
	OrderCounter.Reset();

	SharedSerialization.Reset();
}

void FCommandFrameInputFrame::Release(uint32 InCommandFrame)
{
	CommandFrame = InCommandFrame;
	InputQueue.Empty();

	OrderCounter.Empty();

	SharedSerialization.Reset();
}

bool FCommandFrameInputFrame::Verify(uint32 InCommandFrame)
{
	if (CommandFrame != InCommandFrame)
	{
		Reset(InCommandFrame);
		return false;
	}

	return true;
}

bool FCommandFrameInputFrame::HasOwnerShip(const FUniqueNetIdRepl& Key)
{
	return OrderCounter.Contains(Key);
}

//////////////////////////////////////////////////////////////////////////
// FCommandFrameAttributeSnapshot
FCommandFrameAttributeSnapshot::FCommandFrameAttributeSnapshot()
	: CommandFrame(0)
{
	
}

void FCommandFrameAttributeSnapshot::AddItem(const UScriptStruct* Key, const uint8* const & ItemData)
{
	// 每个Key只允许被加入一次
	// 如果ItemData数量为0，则不加入
	if (Key != nullptr && ItemData != nullptr && !UsedKey.Contains(Key))
	{
		uint8* Memory = AllocateItem(Key, 0);

		Key->CopyScriptStruct(Memory, ItemData);
	}
}

uint8* FCommandFrameAttributeSnapshot::AllocateItem(const UScriptStruct* Key, uint32 DataSize)
{
	// DataSize 是无效参数

	if (!UsedKey.Contains(Key))
	{
		uint8* Memory = nullptr;
		if (uint8** MemoryPtr = MemoryBuffer.Find(Key))
		{
			Memory = *MemoryPtr;
			Key->DestroyStruct(Memory);
		}
		else
		{
			const int32 MinAlignment = Key->GetMinAlignment();
			const int32 RequiredSize = Key->GetStructureSize();
			Memory = ((uint8*)FMemory::Malloc(FMath::Max(1, RequiredSize), MinAlignment));
		}

		Key->InitializeStruct(Memory);

		MemoryBuffer.Add(Key, Memory);
		UsedKey.Add(Key);

		return Memory;
	}
	return nullptr;
}

void FCommandFrameAttributeSnapshot::Reset(uint32 InCommandFrame)
{
	CommandFrame = InCommandFrame;
	
	// Empty 会释放空间
	// MemoryBuffer.Reset();
	UsedKey.Reset();
}

void FCommandFrameAttributeSnapshot::Release(uint32 InCommandFrame)
{
	CommandFrame = InCommandFrame;

	for (auto& Pair : MemoryBuffer)
	{
		FMemory::Free(Pair.Value);
	}

	MemoryBuffer.Empty();
	UsedKey.Empty();
}

bool FCommandFrameAttributeSnapshot::Verify(uint32 InCommandFrame)
{
	if (CommandFrame != InCommandFrame)
	{
		Reset(InCommandFrame);
		return false;
	}

	return true;
}

bool FCommandFrameAttributeSnapshot::HasOwnerShip(const UScriptStruct* Key)
{
	return UsedKey.Contains(Key);
}

uint8* FCommandFrameAttributeSnapshot::ReadItemData(const UScriptStruct* Key)
{
	if (UsedKey.Contains(Key))
	{
		if (uint8** MemoryPtr = MemoryBuffer.Find(Key))
		{
			return *MemoryPtr;
		}
	}

	return nullptr;
}