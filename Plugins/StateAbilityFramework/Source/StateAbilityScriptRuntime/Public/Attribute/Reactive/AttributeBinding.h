#pragma once

#include "Templates/IsInvocable.h"
#include "Delegates/DelegateSignatureImpl.inl"

#include "Attribute/NetBitArray.h"
#include "PrivateAccessor.h"

struct STATEABILITYSCRIPTRUNTIME_API FBindEntryHandle
{
	friend struct FBindEntryContainer;
	friend struct FBindEntry;
	FBindEntryHandle() {}

	FBindEntryHandle(int32 InLayerID, uint64 InEntryID)
		: LayerID(InLayerID)
		, EntryID(InEntryID)
	{
	}

	FORCEINLINE bool IsValid() const
	{
		return LayerID != INDEX_NONE && EntryID != 0;
	}

	FORCEINLINE void Reset()
	{
		LayerID = INDEX_NONE;
		EntryID = 0;
	}

	void SetIndexAndSerialNumber(int32 Index, uint64 SerialNumber)
	{
		check(Index >= 0 && Index < MaxIndex);
		check(SerialNumber < MaxSerialNumber);
		EntryID = (SerialNumber << IndexBits) | (uint64)(uint32)Index;
	}

	FORCEINLINE int32 GetLayerID() const
	{
		return LayerID;
	}

	FORCEINLINE int32 GetEntryItemIndex() const
	{
		return (int32)(EntryID & (uint64)(MaxIndex - 1));
	}

	FORCEINLINE uint64 GetSerialNumber() const
	{
		return EntryID >> IndexBits;
	}

	bool operator==(const FBindEntryHandle& Rhs) const
	{
		return LayerID == Rhs.LayerID && EntryID == Rhs.EntryID;
	}

	bool operator!=(const FBindEntryHandle& Rhs) const
	{
		return LayerID != Rhs.LayerID || EntryID != Rhs.EntryID;
	}

	friend FORCEINLINE uint32 GetTypeHash(const FBindEntryHandle& Key)
	{
		return HashCombine(GetTypeHash(Key.LayerID), GetTypeHash(Key.EntryID));
	}
protected:
	static constexpr uint32 IndexBits = 24;
	static constexpr uint32 SerialNumberBits = 40;

	static_assert(IndexBits + SerialNumberBits == 64, "The space for the timer index and serial number should total 64 bits");

	static constexpr int32  MaxIndex = (int32)1 << IndexBits;
	static constexpr uint64 MaxSerialNumber = (uint64)1 << SerialNumberBits;

	int32 LayerID = INDEX_NONE;
	uint64 EntryID = 0;
};

/**
 * Not Thread Safe!
 */
struct STATEABILITYSCRIPTRUNTIME_API FBindEntry
{
	// individual bindings are not checked for races as it's done for the parent delegate
	using DelegateType = TDelegate<void()>;

	struct FBindEntryItem
	{
		FBindEntryItem(const DelegateType& InDelegate)
			: Delegate(InDelegate)
		{
		}
		FBindEntryItem& operator=(const DelegateType& Other)
		{
			Delegate = Other;
			return *this;
		}
		FBindEntryItem(DelegateType&& InDelegate)
			: Delegate(InDelegate)
		{
		}
		FBindEntryItem& operator=(DelegateType&& Other)
		{
			Delegate = Other;
			return *this;
		}

		DelegateType Delegate;
		uint64 EntrySerialNumber = 0;
	};

	void Broadcast() const
	{
		for (const FBindEntryItem& Item : EntryItems)
		{
			Item.Delegate.ExecuteIfBound();
		}
	}

	bool Execute(const FBindEntryHandle& Handle) const
	{
		if (!Handle.IsValid())
		{
			return false;
		}

		int32 Index = Handle.GetEntryItemIndex();
		uint64 SerialNumber = Handle.GetSerialNumber();
		if (!EntryItems.IsValidIndex(Index) || EntryItems[Index].EntrySerialNumber != SerialNumber)
		{
			return false;
		}

		return EntryItems[Index].Delegate.ExecuteIfBound();
	}

	const DelegateType* FindDelegate(const FBindEntryHandle& Handle) const
	{
		if (!Handle.IsValid())
		{
			return nullptr;
		}

		int32 Index = Handle.GetEntryItemIndex();
		uint64 SerialNumber = Handle.GetSerialNumber();
		if (!EntryItems.IsValidIndex(Index) || EntryItems[Index].EntrySerialNumber != SerialNumber)
		{
			return nullptr;
		}

		return &EntryItems[Index].Delegate;
	}

	const DelegateType& GetDelegate(const FBindEntryHandle& Handle) const
	{
		int32 Index = Handle.GetEntryItemIndex();
		uint64 SerialNumber = Handle.GetSerialNumber();

		ensureMsgf(Handle.IsValid() && EntryItems.IsValidIndex(Index) && EntryItems[Index].EntrySerialNumber == SerialNumber, TEXT("BindEntry cannot use this handle properly!"));

		return EntryItems[Index].Delegate;
	}

	template <typename NewDelegateType>
	FBindEntryHandle AddDelegate(NewDelegateType&& NewDelegateBaseRef)
	{
		if (NewDelegateBaseRef.IsBound())
		{
			// @TODO: There is an extra uint64 ID inside DelegateHandle. I will replace the TDelegate later
			// FDelegateHandle Handle = NewDelegateBaseRef.GetHandle();
			int32 Index = EntryItems.Emplace(Forward<NewDelegateType>(NewDelegateBaseRef));
			FBindEntryHandle Handle = GenerateHandle(Index);
			EntryItems[Index].EntrySerialNumber = Handle.GetSerialNumber();

			return Handle;
		}

		return InValidHandle;
	}

	FBindEntryHandle AddDelegateCopyFrom(const DelegateType& OtherDelegateBaseRef)
	{
		int32 Index = EntryItems.Emplace(OtherDelegateBaseRef);

		FBindEntryHandle Handle = GenerateHandle(Index);
		EntryItems[Index].EntrySerialNumber = Handle.GetSerialNumber();

		return Handle;
	}

	bool RemoveDelegate(const FBindEntryHandle& Handle)
	{
		if (!Handle.IsValid())
		{
			return false;
		}

		int32 Index = Handle.GetEntryItemIndex();
		uint64 SerialNumber = Handle.GetSerialNumber();
		if (EntryItems.IsValidIndex(Index) && EntryItems[Index].EntrySerialNumber == SerialNumber)
		{
			EntryItems[Index].Delegate.Unbind();
			EntryItems.RemoveAtUninitialized(Index);
			return true;
		}
		return false;
	}

private:
	static uint64 GlobalSerialNumber;
	static const FBindEntryHandle InValidHandle;

	static FBindEntryHandle GenerateHandle(int32 Index);

	// If don't open the entrust memory allocation inline, FMulticastInvocationListAllocatorType == FDefaultSparseArrayAllocator::ElementAllocator
	TSparseArray<FBindEntryItem> EntryItems;
};

struct STATEABILITYSCRIPTRUNTIME_API FBindEntryContainer
{
	// Allocate some Entries of BindEntry
	void Allocate(int32 EntryCount);

	// Retrieve the BindEntry for the specified layer
	FBindEntry& GetBindEntry(int32 LayerID);

	const FBindEntry& GetBindEntry(int32 LayerID) const;

	void RemoveBinding(const FBindEntryHandle& Handle);
private:
	TArray<FBindEntry> DataBindings;
};

struct STATEABILITYSCRIPTRUNTIME_API IAttributeBindTracker
{
	friend struct FAttributeBindEffect;

	virtual ~IAttributeBindTracker();
	virtual void Invoke(const FBindEntryHandle& Handle) const = 0;
	virtual void RemoveDependency(const FBindEntryHandle& Handle) = 0;
	virtual void CopyBindEntryItem(const FBindEntry::DelegateType& SrcEntryItem, int32 LayerID, FBindEntryHandle& OutHandle) = 0;
	virtual const FBindEntry& GetTrackerBindEntry(int32 LayerID) const = 0;

private:
	bool bResgiter = false;
};

/**
 * Not recommended for use in code with performance requirements.
 * Not Thread Safe!
 */
struct STATEABILITYSCRIPTRUNTIME_API FAttributeBindEffect
{
	friend struct IAttributeBindTracker;
	typedef TPair<FAttributeBindEffect*, IAttributeBindTracker*> FEffectInfo;

	FAttributeBindEffect() {}
	FAttributeBindEffect(IAttributeBindTracker* Owner, const FBindEntryHandle& Handle)
		: _Owner(Owner)
		, _Handle(Handle)
	{
		GlobalActiveTracker.Add(Owner);
	}

	void Run();
	void Clear();
	bool IsValid();
	static void UpdateDependency(IAttributeBindTracker* Tracker, int32 LayerID);

	// MakeEffect需要放在Reactive头文件中实现
private:
	using DependencyType = TMap<IAttributeBindTracker*, TSet<FBindEntryHandle>>;
	using DependencyBaseType = TMapBase<IAttributeBindTracker*, TSet<FBindEntryHandle>, FDefaultSetAllocator, TDefaultMapHashableKeyFuncs<IAttributeBindTracker*, TSet<FBindEntryHandle>, false>>;
	using DependencyInnerSetType = TSet<DependencyType::ElementType, TDefaultMapHashableKeyFuncs<IAttributeBindTracker*, TSet<FBindEntryHandle>, false>, FDefaultSetAllocator>;
	PRIVATE_DECLARE_NAMESPACE(DependencyBaseType, DependencyInnerSetType, Pairs)

	IAttributeBindTracker* _Owner = nullptr;
	FBindEntryHandle _Handle;
	FNetBitArray IndexMask;
	DependencyType _Dependencies; // 依赖的可靠性由GlobalActiveTracker来保证。依赖仅被用于RemoveBinding，所以即使UObject被标记为回收，也无妨，因为我们不会执行内部逻辑，仅仅是移除一些数据。

	static TSet<IAttributeBindTracker*> GlobalActiveTracker;
	static TArray<FEffectInfo> GlobalActiveEffectStack;
};