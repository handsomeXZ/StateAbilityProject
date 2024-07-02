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
	OrderCounter.Reset();

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