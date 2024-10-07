#include "Attribute/Reactive/AttributeBinding.h"

PRIVATE_DEFINE_NAMESPACE(FAttributeBindEffect, FAttributeBindEffect::DependencyBaseType, Pairs)

uint64 FBindEntry::GlobalSerialNumber = 0;
const FBindEntryHandle FBindEntry::InValidHandle;

TSet<const FAttributeBindTracker*> FAttributeBindEffect::GlobalActiveTracker;
TArray<FAttributeBindEffect::FEffectInfo> FAttributeBindEffect::GlobalActiveEffectStack;

FBindEntryHandle FBindEntry::GenerateHandle(int32 LayerID, int32 Index)
{
	uint64 NewSerialNumber = ++GlobalSerialNumber;
	if (!ensureMsgf(NewSerialNumber != FBindEntryHandle::MaxSerialNumber, TEXT("BindEntry serial number has wrapped around!")))
	{
		NewSerialNumber = (uint64)1;
	}

	FBindEntryHandle Result;
	Result.SetEntryID(LayerID, Index, NewSerialNumber);
	return Result;
}

void FBindEntryContainer::Allocate(int32 EntryCount)
{
	int32 OldNum = DataBindings.Num();
	DataBindings.AddDefaulted(EntryCount);
	for (int32 i = OldNum; i < OldNum + EntryCount; ++i)
	{
		DataBindings[i].LayerID = i;
	}
}

FAttributeBindEffect::FAttributeBindEffect(FAttributeBindEffect&& Other)
	: _Owner(Other._Owner)
	, _Handle(Other._Handle)
	, _IndexMask(Other._IndexMask)
	, _Dependencies(Other._Dependencies)
{
	Other._Owner = nullptr;
	Other._Handle.Reset();
	Other._Dependencies.Empty();
}

FAttributeBindEffect::FAttributeBindEffect(const FAttributeBindEffect& Other)
	: _Owner(Other._Owner)
	, _IndexMask(Other._IndexMask)
	, _Dependencies(Other._Dependencies)
{
	_Owner->CopyBindEntryItem(_Owner->GetTrackerBindEntry(0).GetDelegate(Other._Handle), 0, _Handle);
}

FAttributeBindEffect& FAttributeBindEffect::operator=(FAttributeBindEffect&& Other)
{
	_Owner = Other._Owner;
	_Handle = Other._Handle;
	_IndexMask = Other._IndexMask;
	_Dependencies = Other._Dependencies;

	Other._Owner = nullptr;
	Other._Handle.Reset();
	Other._Dependencies.Empty();

	return *this;
}

FAttributeBindEffect& FAttributeBindEffect::operator=(const FAttributeBindEffect& Other)
{
	_Owner = Other._Owner;
	_IndexMask = Other._IndexMask;
	_Dependencies = Other._Dependencies;

	_Owner->CopyBindEntryItem(_Owner->GetTrackerBindEntry(0).GetDelegate(Other._Handle), 0, _Handle);

	return *this;
}

void FAttributeBindEffect::Run()
{
	if (!GlobalActiveTracker.Contains(_Owner))
	{
		return;
	}

	UpdateDependencyBegin();
	_Owner->Invoke(_Handle);
	UpdateDependencyEnd();
}

void FAttributeBindEffect::UpdateDependencyBegin()
{
	if (bIsUpdatingDependency || !IsValid())
	{
		return;
	}

	bIsUpdatingDependency = true;

	// Algo: Uncaptured dependency cleanup.

	_IndexMask.Clear();

	// Allocate sufficient IndexMax.
	if (!_Dependencies.IsEmpty())
	{
		if (_IndexMask.GetLen() <= _Dependencies.GetMaxIndex())
		{
			_IndexMask = FNetBitArray(_Dependencies.GetMaxIndex() + 1);
		}

		// By default, all elements are uncaptured.
		_IndexMask.AddRange(0, _Dependencies.GetMaxIndex());
	}

	// Invoke...
	GlobalActiveEffectStack.Emplace(this, _Owner);
}

void FAttributeBindEffect::UpdateDependencyEnd()
{
	if (!bIsUpdatingDependency || !IsValid())
	{
		return;
	}

	GlobalActiveEffectStack.RemoveAt(GlobalActiveEffectStack.Num() - 1, EAllowShrinking::No);

	// Clean up uncaptured trackers
	for (auto It = FNetBitArray::FIterator(_IndexMask); It; ++It)
	{
		FSetElementId ElementId = FSetElementId::FromInteger(*It);
		if (_Dependencies.GetMaxIndex() < *It)
		{
			break;
		}

		if (!_Dependencies.IsValidId(ElementId))
		{
			continue;
		}

		DependencyType::ElementType& Dependency = _Dependencies.Get(ElementId);
		if (GlobalActiveTracker.Contains(Dependency.Key))
		{
			for (FBindEntryHandle& Handle : Dependency.Value)
			{
				Dependency.Key->RemoveBinding(Handle);
			}
		}

		DependencyInnerSetType& InnerSet = PRIVATE_GET_NAMESPACE(FAttributeBindEffect, &_Dependencies, Pairs);
		InnerSet.Remove(ElementId);
	}

	bIsUpdatingDependency = false;
}

void FAttributeBindEffect::Clear()
{
	if (!_Owner)
	{
		return;
	}
	_Owner->RemoveBinding(_Handle);
	_Owner = nullptr;
	_Handle.Reset();

	for (auto& Dependency : _Dependencies)
	{
		if (!GlobalActiveTracker.Contains(Dependency.Key))
		{
			continue;
		}

		for (FBindEntryHandle& Handle : Dependency.Value)
		{
			Dependency.Key->RemoveBinding(Handle);
		}
	}

	_Dependencies.Empty(0);
}

bool FAttributeBindEffect::IsValid()
{
	if (!_Owner || !_Handle.IsValid())
	{
		return false;
	}

	if (GlobalActiveTracker.Contains(_Owner))
	{
		return true;
	}

	return false;
}

void FAttributeBindEffect::AddDependency(const FAttributeBindTracker* Tracker, int32 LayerID)
{
	if (GlobalActiveEffectStack.IsEmpty())
	{
		return;
	}

	if (!GlobalActiveTracker.Contains(Tracker))
	{
		Tracker->bResgiter = true;
		GlobalActiveTracker.Add(Tracker);
	}

	FEffectInfo& TopEffectInfo = GlobalActiveEffectStack.Last();
	FAttributeBindEffect& TopEffect = *TopEffectInfo.Key;

	DependencyInnerSetType& InnerSet = PRIVATE_GET_NAMESPACE(FAttributeBindEffect, &TopEffect._Dependencies, Pairs);
	FSetElementId ElementId = InnerSet.FindId(Tracker);
	if (ElementId.IsValidId())
	{
		TopEffect._IndexMask.Remove(ElementId.AsInteger());
	}
	else
	{
		const FBindEntry& SrcBindEntry = TopEffectInfo.Value->GetTrackerBindEntry(TopEffect._Handle.GetLayerID());
		FBindEntryHandle OutHandle;
		Tracker->CopyBindEntryItem(SrcBindEntry.GetDelegate(TopEffect._Handle), LayerID, OutHandle);
		ensureMsgf(OutHandle.IsValid(), TEXT("Update dependency failed!!"));

		TSet<FBindEntryHandle>& DependencyBindings = TopEffect._Dependencies.Add(Tracker);
		DependencyBindings.Add(OutHandle);
	}
}