#pragma once

#include "CoreMinimal.h"

template<typename InElementType>
class TCircularQueueView
{
public:
	using ElementType = InElementType;

	TCircularQueueView()
		: DataPtr(nullptr)
		, BeginIndex(0)
		, MaxItemNum(0)
		, ItemNum(0)
	{}

	TCircularQueueView(ElementType* InDataPtr, uint32 InBeginIndex, uint32 InMaxItemNum, uint32 InItemNum)
		: DataPtr(InDataPtr)
		, BeginIndex(InBeginIndex)
		, MaxItemNum(InMaxItemNum)
		, ItemNum(InItemNum)
	{}

	FORCEINLINE ElementType& operator[](uint32 Index) const
	{
		RangeCheck(Index);
		return DataPtr[(Index + BeginIndex) & MaxItemNum];
	}

	FORCEINLINE uint32 Num() const
	{
		return ItemNum;
	}

	bool IsEmpty() const
	{
		return ItemNum == 0;
	}

	FORCEINLINE void CheckInvariants() const
	{
		checkSlow(ItemNum >= 0);
	}

	FORCEINLINE void RangeCheck(uint32 Index) const
	{
		CheckInvariants();

		checkf((Index >= 0) & (Index < ItemNum), TEXT("Queue index out of bounds: %lld from an queue of size %lld"), (long long)Index, (long long)ItemNum); // & for one branch
	}

private:
	ElementType* DataPtr;
	uint32 BeginIndex;
	uint32 MaxItemNum;
	uint32 ItemNum;
};

//////////////////////////////////////////////////////////////////////////
// 每个Owner的CommandFrame只允许被写入一次
//////////////////////////////////////////////////////////////////////////
/** Templated datas history holding a datas buffer */
template<typename DataType, typename OwnerShipDataType, typename ItemDataType>
struct TJOwnerShipCircularQueue
{
	FORCEINLINE TJOwnerShipCircularQueue()
		: EndFrame(0)
		, HeadFrame(0)
		, EndIndex(0)
		, HeadIndex(0)
		, MaxFramesNum(0)
	{}
	FORCEINLINE TJOwnerShipCircularQueue(const uint32 FrameCount)
		: EndFrame(0)
		, HeadFrame(0)
		, EndIndex(0)
		, HeadIndex(0)
	{
		checkSlow(Capacity > 0);
		checkSlow(Capacity <= 0xffffffffU);

		Buffer.SetNum(FMath::RoundUpToPowerOfTwo(FrameCount + 1));
		MaxFramesNum = Buffer.Num() - 1;
		
		// 额外的一个空间用来判断队尾
	}
	FORCEINLINE virtual ~TJOwnerShipCircularQueue() {}

public:
	FORCEINLINE uint32 Count() const
	{
		return EndFrame - HeadFrame - 1;
	}

	FORCEINLINE uint32 Count(const OwnerShipDataType& Owner) const
	{
		if (auto CountPtr = Counter.Find(Owner))
		{
			return *CountPtr;
		}

		return 0;
	}

	FORCEINLINE bool IsFull() const
	{
		return ((HeadIndex + 1) & MaxFramesNum) == EndIndex;
	}

	FORCEINLINE bool IsEmpty() const
	{
		return HeadIndex == EndIndex;
	}

	FORCEINLINE bool IsEmpty(const OwnerShipDataType& Owner) const
	{
		if (uint32 CountPtr = Counter.Find(Owner))
		{
			return *CountPtr == 0;
		}

		return true;
	}

	FORCEINLINE void CheckInvariants() const
	{
		checkSlow(MaxFramesNum >= 0);
	}

	FORCEINLINE void AckData(const uint32 AckFrame)
	{
		if (AckFrame < HeadFrame)
		{
			// 目标帧过旧，已经被放弃
			return;
		}

		if (AckFrame < EndFrame)
		{

			// 遍历所有目标并更新计数
			for (uint32 Frame = HeadFrame; Frame <= AckFrame; ++Frame)
			{
				for (auto& CountPair : Counter)
				{
					if (Buffer[Frame & MaxFramesNum].HasOwnerShip(CountPair.Key))
					{
						if (CountPair.Value)
						{
							--(CountPair.Value);
						}
					}
				}
			}

			HeadFrame = AckFrame + 1;
			HeadIndex = HeadFrame & MaxFramesNum;
			// 此时可能变成空哦~。
		}
		else
		{
			// 目标帧还未加入，队列置空
			Empty();
			HeadFrame = AckFrame;
			EndFrame = HeadFrame;
		}
	}

	// 相对AckData开销更小
	FORCEINLINE void AckNextData()
	{
		++HeadFrame;

		if (HeadFrame < EndFrame)
		{
			HeadIndex = HeadFrame & MaxFramesNum;
			// 此时可能变成空哦~。
			for (auto& CountPair : Counter)
			{
				if (CountPair.Value)
				{
					--(CountPair.Value);
				}
			}

		}
		else
		{
			// 目标帧还未加入，队列置空
			Empty();
		}
	}

	// 基于最新的Frame进行置空
	FORCEINLINE void Empty()
	{
		// 置空，但不会修改其他已有的内容
		HeadFrame = EndFrame ? (EndFrame - 1) : 0;
		EndFrame = HeadFrame;

		HeadIndex = 0;
		EndIndex = 0;
		
		Counter.Empty();
	}

	//////////////////////////////////////////////////////////////////////////
	// READ
	FORCEINLINE bool ReadData(DataType& Data, const uint32 ReadFrame)
	{
		CheckInvariants();

		if (ReadFrame >= HeadFrame && ReadFrame < EndFrame)
		{
			Data = Buffer[ReadFrame & MaxFramesNum];
			return true;
		}
		return false;
	}

	FORCEINLINE DataType* ReadData(const uint32 ReadFrame)
	{
		CheckInvariants();

		if (ReadFrame >= HeadFrame && ReadFrame < EndFrame)
		{
			return &Buffer[ReadFrame & MaxFramesNum];
		}
		return nullptr;
	}

	FORCEINLINE TCircularQueueView<DataType> ReadRangeData(uint32 ReadFrame, uint32 FrameCount)
	{
		CheckInvariants();

		if (ReadFrame + 1 >= FrameCount && ReadFrame + 1 - FrameCount >= HeadFrame && ReadFrame < EndFrame)
		{
			TCircularQueueView<DataType> Result(Buffer.GetData(), ReadFrame, MaxFramesNum, FrameCount);
			return Result;
		}
		return TCircularQueueView<DataType>();
	}

	// 自动检查有效范围，并收缩返回的数据范围
	FORCEINLINE TCircularQueueView<DataType> ReadRangeData_Shrink(uint32 ReadFrame, uint32 FrameCount)
	{
		CheckInvariants();

		FrameCount = FMath::Min(ReadFrame + 1, FrameCount);

		uint32 LastFrame = FMath::Min(ReadFrame, EndFrame - 1);
		uint32 FirstFrame = FMath::Max(ReadFrame + 1 - FrameCount, HeadFrame);

		if (FirstFrame <= LastFrame)
		{
			TCircularQueueView<DataType> Result(Buffer.GetData(), FirstFrame, MaxFramesNum, LastFrame - FirstFrame + 1);
			return Result;
		}
		return TCircularQueueView<DataType>();
	}

	//////////////////////////////////////////////////////////////////////////
	// WRITE
	bool RecordItemData(const OwnerShipDataType& Owner, const ItemDataType& ItemData, const uint32 InsertFrame)
	{
		CheckInvariants();

		uint32 Index = InsertFrame & MaxFramesNum;

		// 无效数据
		if (InsertFrame < HeadFrame)
		{
			return false;
		}

		if (InsertFrame < EndFrame)
		{
			// 是否为已有效数据
			if (!Buffer[Index].Verify(InsertFrame))
			{
				Buffer[Index].AddItem(Owner, ItemData);
				++(Counter.FindOrAdd(Owner));
			}
			else
			{
				// 不能覆盖存在的有效数据
				return false;
			}
		}
		else
		{
			// 未分配数据
			Buffer[Index].Reset(InsertFrame);
			Buffer[Index].AddItem(Owner, ItemData);
			++(Counter.FindOrAdd(Owner));

			if (InsertFrame - HeadFrame + 1 > MaxFramesNum)
			{
				// 没有空间了
				EndIndex = (Index + 1) & MaxFramesNum;
				HeadIndex = (EndIndex + 1) & MaxFramesNum;

				EndFrame = InsertFrame + 1;
				HeadFrame = InsertFrame - MaxFramesNum + 1;

				// @TODO: 目前临时靠这种方法来判断是否有异常
				check(HeadFrame < MAX_uint32 / 2);
			}
			else
			{
				EndIndex = (Index + 1) & MaxFramesNum;

				EndFrame = InsertFrame + 1;
			}
		}

		return true;

	}

	// 申请未初始化的空间并返回
	uint8* AllocateItemData(const OwnerShipDataType& Owner, uint32 DataSize, const uint32 InsertFrame)
	{
		CheckInvariants();

		uint32 Index = InsertFrame & MaxFramesNum;

		// 无效数据
		if (InsertFrame < HeadFrame)
		{
			return nullptr;
		}

		if (InsertFrame < EndFrame)
		{
			// 是否为已有效数据
			if (!Buffer[Index].Verify(InsertFrame))
			{
				++(Counter.FindOrAdd(Owner));
				return Buffer[Index].AllocateItem(Owner, DataSize);
			}
			else
			{
				return nullptr;
			}
		}
		else
		{
			if (InsertFrame - HeadFrame + 1 > MaxFramesNum)
			{
				// 没有空间了
				EndIndex = (Index + 1) & MaxFramesNum;
				HeadIndex = (EndIndex + 1) & MaxFramesNum;

				EndFrame = InsertFrame + 1;
				HeadFrame = InsertFrame - MaxFramesNum + 1;

				// @TODO: 目前临时靠这种方法来判断是否有异常
				check(HeadFrame < MAX_uint32 / 2);
			}
			else
			{
				EndIndex = (Index + 1) & MaxFramesNum;

				EndFrame = InsertFrame + 1;
			}

			++(Counter.FindOrAdd(Owner));

			// 未分配数据
			Buffer[Index].Reset(InsertFrame);
			return Buffer[Index].AllocateItem(Owner, DataSize);
		}
	}

protected:

	/** 数据缓冲区 */
	TArray<DataType> Buffer;

	TMap<OwnerShipDataType, uint32> Counter;

	/** 一直指向队尾无效数据的占位帧 */
	uint32 EndFrame;
	uint32 HeadFrame;

	/** 一直指向队尾无效数据的占位索引 */
	uint32 EndIndex;
	uint32 HeadIndex;

	uint32 MaxFramesNum;
};