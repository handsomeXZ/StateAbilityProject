#include "Attribute/Reactive/AttributeBinding.h"

PRIVATE_DEFINE_NAMESPACE(FAttributeBindEffect, FAttributeBindEffect::DependencyBaseType, Pairs)

uint64 FBindEntry::GlobalSerialNumber = 0;
const FBindEntryHandle FBindEntry::InValidHandle;

TSet<IAttributeBindTracker*> FAttributeBindEffect::GlobalActiveTracker;
TArray<FAttributeBindEffect::FEffectInfo> FAttributeBindEffect::GlobalActiveEffectStack;

FBindEntryHandle FBindEntry::GenerateHandle(int32 Index)
{
	uint64 NewSerialNumber = ++GlobalSerialNumber;
	if (!ensureMsgf(NewSerialNumber != FBindEntryHandle::MaxSerialNumber, TEXT("BindEntry serial number has wrapped around!")))
	{
		NewSerialNumber = (uint64)1;
	}

	FBindEntryHandle Result;
	Result.SetIndexAndSerialNumber(Index, NewSerialNumber);
	return Result;
}

void FBindEntryContainer::Allocate(int32 EntryCount)
{
	DataBindings.AddDefaulted(EntryCount);
}

FBindEntry& FBindEntryContainer::GetBindEntry(int32 LayerID)
{
	ensureMsgf(DataBindings.IsValidIndex(LayerID), TEXT("GetBindEntry DataBindings access overflow!!"));
	return DataBindings[LayerID];
}

const FBindEntry& FBindEntryContainer::GetBindEntry(int32 LayerID) const
{
	ensureMsgf(DataBindings.IsValidIndex(LayerID), TEXT("GetBindEntry DataBindings access overflow!!"));
	return DataBindings[LayerID];
}

void FBindEntryContainer::RemoveBinding(const FBindEntryHandle& Handle)
{
	ensureMsgf(DataBindings.IsValidIndex(Handle.LayerID), TEXT("UnBind DataBindings access overflow!!"));
	DataBindings[Handle.LayerID].RemoveDelegate(Handle);
}

IAttributeBindTracker::~IAttributeBindTracker()
{
	if (bResgiter)
	{
		FAttributeBindEffect::GlobalActiveTracker.Remove(this);
	}
}

void FAttributeBindEffect::Run()
{
	if (!GlobalActiveTracker.Contains(_Owner))
	{
		return;
	}

	// Algo: Uncaptured dependency cleanup.
	// Allocate sufficient IndexMax.
	if (IndexMask.GetLen() <= _Dependencies.GetMaxIndex())
	{
		IndexMask = FNetBitArray(_Dependencies.GetMaxIndex() + 1);
	}

	// By default, all elements are uncaptured.
	IndexMask.MarkAll();

	// Invoke...
	GlobalActiveEffectStack.Emplace(this, _Owner);
	_Owner->Invoke(_Handle);
	GlobalActiveEffectStack.RemoveAt(GlobalActiveEffectStack.Num() - 1, EAllowShrinking::No);

	// Clean up uncaptured trackers
	for (auto It = FNetBitArray::FIterator(IndexMask); It; ++It)
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

		TPair<IAttributeBindTracker*, TSet<FBindEntryHandle>>& Dependency = _Dependencies.Get(ElementId);
		if (GlobalActiveTracker.Contains(Dependency.Key))
		{
			for (FBindEntryHandle& Handle : Dependency.Value)
			{
				Dependency.Key->RemoveDependency(Handle);
			}
		}

		DependencyInnerSetType& InnerSet = PRIVATE_GET_NAMESPACE(FAttributeBindEffect, &_Dependencies, Pairs);
		InnerSet.Remove(ElementId);
	}
}

void FAttributeBindEffect::Clear()
{
	_Owner->RemoveDependency(_Handle);
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
			Dependency.Key->RemoveDependency(Handle);
		}
	}

	_Dependencies.Empty(0);
}

bool FAttributeBindEffect::IsValid()
{
	if (_Owner || !_Handle.IsValid())
	{
		return false;
	}

	if (GlobalActiveTracker.Contains(_Owner))
	{
		return true;
	}

	return false;
}

void FAttributeBindEffect::UpdateDependency(IAttributeBindTracker* Tracker, int32 LayerID)
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
		TopEffect.IndexMask.Remove(ElementId.AsInteger());
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