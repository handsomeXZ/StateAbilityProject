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

	FBindEntryHandle(uint64 InEntryID)
		: EntryID(InEntryID)
	{
	}

	FORCEINLINE bool IsValid() const
	{
		return EntryID != 0;
	}

	FORCEINLINE void Reset()
	{
		EntryID = 0;
	}

	FORCEINLINE void SetEntryID(int16 LayerID, int16 Index, uint64 SerialNumber)
	{
		check(LayerID >= 0 && LayerID < MaxLayerID);
		check(Index >= 0 && Index < MaxIndex);
		check(SerialNumber < MaxSerialNumber);
		EntryID = (SerialNumber << LayerIDAndIndexBits) | ((uint64)(uint16)Index << LayerIDBits) | (uint64)(uint16)LayerID;
	}

	FORCEINLINE int32 GetLayerID() const
	{
		return (int32)(EntryID & (uint64)(MaxLayerID - 1));
	}

	FORCEINLINE int32 GetEntryItemIndex() const
	{
		return (int32)((uint64)(EntryID >> LayerIDBits) & (uint64)(MaxIndex - 1));
	}

	FORCEINLINE uint64 GetSerialNumber() const
	{
		return EntryID >> LayerIDAndIndexBits;
	}

	bool operator==(const FBindEntryHandle& Rhs) const
	{
		return EntryID == Rhs.EntryID;
	}

	bool operator!=(const FBindEntryHandle& Rhs) const
	{
		return EntryID != Rhs.EntryID;
	}

	friend FORCEINLINE uint32 GetTypeHash(const FBindEntryHandle& Key)
	{
		return GetTypeHash(Key.EntryID);
	}
protected:
	static constexpr uint32 LayerIDBits = 8;
	static constexpr uint32 IndexBits = 16;
	static constexpr uint32 LayerIDAndIndexBits = LayerIDBits + IndexBits;
	static constexpr uint32 SerialNumberBits = 40;

	static_assert(LayerIDBits + IndexBits + SerialNumberBits == 64, "The space for the Entry index and serial number should total 64 bits");

	static constexpr int32  MaxLayerID = (int32)1 << LayerIDBits;
	static constexpr int32  MaxIndex = (int32)1 << IndexBits;
	static constexpr uint64 MaxSerialNumber = (uint64)1 << SerialNumberBits;

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
		int32 MaxIndex = EntryItems.GetMaxIndex();
		for (int32 Index = 0; Index <= MaxIndex; ++Index)
		{
			if (EntryItems.IsValidIndex(Index))
			{
				const FBindEntryItem& Item = EntryItems[Index];
				Item.Delegate.ExecuteIfBound();
			}
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
			FBindEntryHandle Handle = GenerateHandle(LayerID, Index);
			EntryItems[Index].EntrySerialNumber = Handle.GetSerialNumber();

			return Handle;
		}

		return InValidHandle;
	}

	FBindEntryHandle AddDelegateCopyFrom(const DelegateType& OtherDelegateBaseRef)
	{
		int32 Index = EntryItems.Emplace(OtherDelegateBaseRef);

		FBindEntryHandle Handle = GenerateHandle(LayerID, Index);
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

	FORCEINLINE void ClearEntryItems()
	{
		EntryItems.Empty();
	}

	FORCEINLINE int32 GetEntryItemsNum() const
	{
		return EntryItems.Num();
	}
private:
	friend struct FBindEntryContainer;
	static uint64 GlobalSerialNumber;
	static const FBindEntryHandle InValidHandle;

	static FBindEntryHandle GenerateHandle(int32 LayerID, int32 Index);

	int16 LayerID = 0;
	// If don't open the entrust memory allocation inline, FMulticastInvocationListAllocatorType == FDefaultSparseArrayAllocator::ElementAllocator
	TSparseArray<FBindEntryItem> EntryItems;
};

struct STATEABILITYSCRIPTRUNTIME_API FBindEntryContainer
{
	// Allocate some Entries of BindEntry
	void Allocate(int32 EntryCount);

	// Retrieve the BindEntry for the specified layer
	FORCEINLINE FBindEntry& GetBindEntry(int32 LayerID)
	{
		ensureMsgf(DataBindings.IsValidIndex(LayerID), TEXT("GetBindEntry DataBindings access overflow!!"));
		return DataBindings[LayerID];
	}

	FORCEINLINE const FBindEntry& GetBindEntry(int32 LayerID) const
	{
		ensureMsgf(DataBindings.IsValidIndex(LayerID), TEXT("GetBindEntry DataBindings access overflow!!"));
		return DataBindings[LayerID];
	}

	FORCEINLINE int32 GetBindEntriesNum() const
	{
		return DataBindings.Num();
	}
	FORCEINLINE int32 GetEntryItemsNum(int32 LayerID) const
	{
		return GetBindEntry(LayerID).GetEntryItemsNum();
	}

	FORCEINLINE void RemoveBinding(const FBindEntryHandle& Handle)
	{
		ensureMsgf(DataBindings.IsValidIndex(Handle.GetLayerID()), TEXT("UnBind DataBindings access overflow!!"));
		DataBindings[Handle.GetLayerID()].RemoveDelegate(Handle);
	}

	FORCEINLINE void ClearBindEntry(int32 LayerID)
	{
		ensureMsgf(DataBindings.IsValidIndex(LayerID), TEXT("UnBind DataBindings access overflow!!"));
		DataBindings[LayerID].ClearEntryItems();
	}
	FORCEINLINE void ClearAllBindEntry()
	{
		for (auto& BindEntry : DataBindings)
		{
			BindEntry.ClearEntryItems();
		}
	}
private:
	TArray<FBindEntry> DataBindings;
};

/**
 * Not recommended for use in code with performance requirements.
 * Not Thread Safe!
 */
struct STATEABILITYSCRIPTRUNTIME_API FAttributeBindEffect
{
	friend struct FAttributeBindTracker;
	typedef TPair<FAttributeBindEffect*, const FAttributeBindTracker*> FEffectInfo;

	FAttributeBindEffect() {}
	FAttributeBindEffect(const FAttributeBindTracker* Owner, const FBindEntryHandle& Handle)
		: _Owner(Owner)
		, _Handle(Handle)
	{
		GlobalActiveTracker.Add(Owner);
	}
	virtual ~FAttributeBindEffect()
	{
		Clear();
	}

	FAttributeBindEffect(FAttributeBindEffect&& Other);
	FAttributeBindEffect(const FAttributeBindEffect& Other);
	FAttributeBindEffect& operator=(FAttributeBindEffect&& Other);
	FAttributeBindEffect& operator=(const FAttributeBindEffect& Other);

	// Only update dependency only when running effect.
	void Run();
	void Clear();
	bool IsValid();
	static void AddDependency(const FAttributeBindTracker* Tracker, int32 LayerID);

	void UpdateDependencyBegin();
	void UpdateDependencyEnd();

private:
	using DependencyType = TMap<const FAttributeBindTracker*, TSet<FBindEntryHandle>>;
	using DependencyBaseType = TMapBase<const FAttributeBindTracker*, TSet<FBindEntryHandle>, FDefaultSetAllocator, TDefaultMapHashableKeyFuncs<const FAttributeBindTracker*, TSet<FBindEntryHandle>, false>>;
	using DependencyInnerSetType = TSet<DependencyType::ElementType, TDefaultMapHashableKeyFuncs<const FAttributeBindTracker*, TSet<FBindEntryHandle>, false>, FDefaultSetAllocator>;
	PRIVATE_DECLARE_NAMESPACE(DependencyBaseType, DependencyInnerSetType, Pairs)

	const FAttributeBindTracker* _Owner = nullptr;
	FBindEntryHandle _Handle;
	FNetBitArray _IndexMask;
	uint64 bIsUpdatingDependency : 1 = false;
	DependencyType _Dependencies; // 依赖的可靠性由GlobalActiveTracker来保证。依赖仅被用于RemoveBinding，所以即使UObject被标记为回收，也无妨，因为我们不会执行内部逻辑，仅仅是移除一些数据。

	static TSet<const FAttributeBindTracker*> GlobalActiveTracker;
	static TArray<FEffectInfo> GlobalActiveEffectStack;
};

struct STATEABILITYSCRIPTRUNTIME_API FAttributeBindSharedEffect
{
	FAttributeBindSharedEffect() {}
	FAttributeBindSharedEffect(const FAttributeBindTracker* Owner, const FBindEntryHandle& Handle)
		: SharedEffectPtr(MakeShared<FAttributeBindEffect>(Owner, Handle))
	{
		
	}

	FORCEINLINE void Run()
	{
		if (SharedEffectPtr.IsValid())
		{
			SharedEffectPtr->Run();
		}
	}
	FORCEINLINE void Clear()
	{
		if (SharedEffectPtr.IsValid())
		{
			SharedEffectPtr->Clear();
		}
	}
	FORCEINLINE bool IsValid()
	{
		return SharedEffectPtr.IsValid() && SharedEffectPtr->IsValid();
	}

	FAttributeBindSharedEffect(FAttributeBindSharedEffect&& Other) = default;
	FAttributeBindSharedEffect(const FAttributeBindSharedEffect& Other) = default;
	FAttributeBindSharedEffect& operator=(FAttributeBindSharedEffect&& Other) = default;
	FAttributeBindSharedEffect& operator=(const FAttributeBindSharedEffect& Other) = default;
private:
	TSharedPtr<FAttributeBindEffect> SharedEffectPtr;
};

struct STATEABILITYSCRIPTRUNTIME_API FAttributeBindTracker
{
	friend struct FAttributeBindEffect;

	~FAttributeBindTracker()
	{
		CloseTracker();
	}

	FORCEINLINE void Invoke(const FBindEntryHandle& Handle) const
	{
		const FBindEntry& BindEntry = GetBindEntry(Handle.GetLayerID());
		BindEntry.Execute(Handle);
	}

	FORCEINLINE void CopyBindEntryItem(const FBindEntry::DelegateType& SrcEntryItem, int32 LayerID, FBindEntryHandle& OutHandle) const
	{
		FBindEntry& BindEntry = GetBindEntry(LayerID);

		OutHandle = BindEntry.AddDelegateCopyFrom(SrcEntryItem);
	}

	FORCEINLINE const FBindEntry& GetTrackerBindEntry(int32 LayerID) const
	{
		return GetBindEntry(LayerID);
	}

	FORCEINLINE FBindEntry& GetBindEntry(int32 LayerID) const
	{
		return BindEntryContainer.GetBindEntry(LayerID);
	}

	FORCEINLINE int32 GetBindEntriesNum() const
	{
		return BindEntryContainer.GetBindEntriesNum();
	}

	FORCEINLINE void AllocateBindEntry(int32 Count)
	{
		BindEntryContainer.Allocate(Count);
	}

	FORCEINLINE void RemoveBinding(const FBindEntryHandle& Handle) const
	{
		BindEntryContainer.RemoveBinding(Handle);
	}

	FORCEINLINE void ClearBindEntry(int32 LayerID) const
	{
		BindEntryContainer.ClearBindEntry(LayerID);
	}

	FORCEINLINE void ClearAllBindEntry() const
	{
		BindEntryContainer.ClearAllBindEntry();
	}

	FORCEINLINE void CloseTracker()
	{
		if (bResgiter)
		{
			FAttributeBindEffect::GlobalActiveTracker.Remove(this);
		}
	}

protected:
	mutable FBindEntryContainer BindEntryContainer;
private:
	mutable bool bResgiter = false;
};