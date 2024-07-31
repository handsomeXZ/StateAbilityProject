#include "Attribute/AttributeBag.h"

#include "Engine/ActorChannel.h"
#include "Net/RepLayout.h"
#include "Engine/NetConnection.h"
#include "Engine/PackageMapClient.h"
#include "Engine/UserDefinedStruct.h"


DEFINE_LOG_CATEGORY_STATIC(LogAttributeBag, Log, All);


void IAttributeListenerInterface::Initialize(UWorld* World)
{
	check(World);

	EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
}

void IAttributeListenerInterface::Release(UObject* Owner)
{
	if (EntitySubsystem.IsValid())
	{
		FAttributeProviderFragment* ProviderFragment = EntitySubsystem->GetEntityManager().GetFragmentDataPtr<FAttributeProviderFragment>(EntityHandle);

		if (ProviderFragment)
		{
			ProviderFragment->Listeners.Remove(Owner);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FAttributeBag
FAttributeBag::FAttributeBag()
{
}

FAttributeBag::FAttributeBag(const FAttributeBag& InOther)
{
}

FAttributeBag& FAttributeBag::operator=(const FAttributeBag& InOther)
{
	return *this;
}
void FAttributeBag::MarkDirty(const int32 Index)
{
	RawDirtyMark.Add(Index);
}

void FAttributeBag::MarkAllDirty()
{
	RawDirtyMark.MarkAll();
}

void FAttributeBag::ClearDirty()
{
	RawDirtyMark.Clear();
	DirtyMark.Clear();
}

void FAttributeBag::UpdatePropertiesCompare(uint32 ReplicationFrame)
{
	if (LastReplicationFrame == ReplicationFrame)
	{
		return;
	}

	LastReplicationFrame = ReplicationFrame;

	if (RawDirtyMark.IsEmpty())
	{
		return;
	}

	DirtyMark = RawDirtyMark;
	RawDirtyMark.Clear();

}


//////////////////////////////////////////////////////////////////////////
// FAttributeEntityBag
FAttributeEntityBag::FAttributeEntityBag()
	: Super()
	, DataStruct(nullptr)
{
}

FAttributeEntityBag::FAttributeEntityBag(const UScriptStruct* InDataStruct)
	: Super()
	, DataStruct(const_cast<UScriptStruct*>(InDataStruct))
{
}

FAttributeEntityBag::FAttributeEntityBag(const FAttributeEntityBag& InOther)
	: Super(InOther)
	, DataStruct(const_cast<UScriptStruct*>(InOther.DataStruct))
	, AttributeEntity(InOther.AttributeEntity)
{

}

FAttributeEntityBag& FAttributeEntityBag::operator=(const FAttributeEntityBag& InOther)
{
	DataStruct = const_cast<UScriptStruct*>(InOther.DataStruct);
	AttributeEntity = InOther.AttributeEntity;
	return *this;
}

FAttributeEntityBag::~FAttributeEntityBag()
{

}

bool FAttributeEntityBag::IsDataValid() const
{
	return DataStruct != nullptr && AttributeEntity.IsDataValid();
}

void FAttributeEntityBag::Initialize(FAttributeEntityBuildParam& BuildParam)
{
	UMassEntitySubsystem* EntitySubsystem = BuildParam.World->GetSubsystem<UMassEntitySubsystem>();
	check(EntitySubsystem);


	// 缓存RepProps信息
	FAttributeNetSharedFragment TemplateSharedFragment;
	const uint32 SubsystemHash = UE::StructUtils::GetStructCrc32(FConstStructView::Make(TemplateSharedFragment));
	const FSharedStruct& SharedFragmentView = EntitySubsystem->GetMutableEntityManager().GetOrCreateSharedFragmentByHash<FAttributeNetSharedFragment>(SubsystemHash, TemplateSharedFragment);
	if (SharedFragmentView.IsValid())
	{
		FAttributeNetSharedFragment& SharedFragment = SharedFragmentView.Get<FAttributeNetSharedFragment>();
		if (!SharedFragment.ReplicatedProps.Contains(DataStruct))
		{
			TArray<FProperty*>& PropArrayRef = SharedFragment.ReplicatedProps.FindOrAdd(DataStruct);
			for (TFieldIterator<FProperty> It(DataStruct); It; ++It)
			{
				PropArrayRef.Add(*It);
			}
		}
	}

	BuildParam.ArchetypeFragment.Add(DataStruct);
	BuildParam.ArchetypeFragment.Add(FAttributeProviderFragment::StaticStruct());
	BuildParam.ArchetypeFragment.Add(FAttributeNetFragment::StaticStruct());
	BuildParam.ArchetypeFragment.Add(FAttributeTagFragment::StaticStruct());

	BuildParam.SharedFragmentValue.AddSharedFragment(SharedFragmentView);

	//BuildParam.ArchetypeFragment.Add(FAttributeNetRoleTagFragment::StaticStruct());

	AttributeEntity.Initialize(BuildParam);
}

bool FAttributeEntityBag::Serialize(FArchive& Ar)
{
	Ar << UID;
	Ar << DataStruct;

	return true;
}

bool FAttributeEntityBag::SerializeRead(FNetDeltaSerializeInfo& deltaParms)
{
	FBitReader& Ar = *(deltaParms.Reader);

	bool bChangesIsEmpty;
	Ar << bChangesIsEmpty;

	if (!bChangesIsEmpty)
	{
		uint8* data = GetMutableMemory();

		int32 PropNum = GetPropertyNum();

		FNetBitArray changes(PropNum);
		Ar << changes;

		if (!(this->NetSerializeDirtyItem(Ar, deltaParms.Map, changes)))
		{
			return false;
		}

		DirtyMark |= changes;
		RawDirtyMark |= changes;
	}

	return true;
}

bool FAttributeEntityBag::SerializeWrite(FNetDeltaSerializeInfo& deltaParms)
{
	FBitWriter& Ar = *(deltaParms.Writer);

	UPackageMapClient* PackageMapClient = Cast<UPackageMapClient>(deltaParms.Map);
	UNetConnection* NetConnection = PackageMapClient->GetConnection();
	uint32 ReplicationFrame = NetConnection->Driver->ReplicationFrame;

	UpdatePropertiesCompare(ReplicationFrame);
	{
		FNetBitArray changes(GetPropertyNum());

		changes |= DirtyMark;


		bool bChangesIsEmpty = changes.IsEmpty();

		Ar << bChangesIsEmpty;

		if (!bChangesIsEmpty)
		{
			Ar << changes;

			uint8* data = GetMutableMemory();

			if (!(this->NetSerializeDirtyItem(Ar, PackageMapClient, changes)))
			{
				return false;
			}

		}
	}

	return true;
}

bool FAttributeEntityBag::NetDeltaSerialize(FNetDeltaSerializeInfo& deltaParms)
{
	if (deltaParms.GatherGuidReferences)
	{
		// 当前处于 GatherGuidReferences 阶段

		FAttributeNetFragment& NetFragment = GetNetFragment();

		for (const auto& GuidReferencePair : NetFragment.GuidReferencesMap)
		{
			const auto& GuidReference = GuidReferencePair.Value;

			deltaParms.GatherGuidReferences->Append(GuidReference.UnmappedGUIDs);
			deltaParms.GatherGuidReferences->Append(GuidReference.MappedDynamicGUIDs);
		}
		return true;
	}

	if (deltaParms.MoveGuidToUnmapped)
	{
		// 当前处于 MoveGuidToUnmapped 阶段

		FAttributeNetFragment& NetFragment = GetNetFragment();

		bool bFound = false;

		const FNetworkGUID GUID = *deltaParms.MoveGuidToUnmapped;

		// 如果这个GUID是此处关心的，则确保它现在被移交到未映射列表中
		for (auto& GuidReferencePair : NetFragment.GuidReferencesMap)
		{
			auto& GuidReference = GuidReferencePair.Value;

			if (GuidReference.MappedDynamicGUIDs.Contains(GUID))
			{
				GuidReference.MappedDynamicGUIDs.Remove(GUID);
				GuidReference.UnmappedGUIDs.Add(GUID);
				bFound = true;
			}
		}

		return bFound;
	}

	if (deltaParms.bUpdateUnmappedObjects)
	{
		// 当前处于 UpdateUnmappedObjects 阶段

		FAttributeNetFragment& NetFragment = GetNetFragment();
		FAttributeNetSharedFragment& NetSharedFragment = GetNetSharedFragment();

		const TArray<FProperty*>& PropArray = NetSharedFragment.ReplicatedProps.FindOrAdd(DataStruct);
		TArray<FAttributeNetFragment::FRepPropIndex> changes;

		// 循环遍历每个 GuidReference，检查是否有unmapped objects
		for (auto It = NetFragment.GuidReferencesMap.CreateIterator(); It; ++It)
		{
			// Get the element id
			const auto index = It.Key();

			if (index < 0)
			{
				continue;
			}

			// Get a reference to the unmapped item itself
			auto& GuidReference = It.Value();

			if ((GuidReference.UnmappedGUIDs.Num() == 0 && GuidReference.MappedDynamicGUIDs.Num() == 0))
			{
				// 如果由于某种原因导致当前的属性的GuidReference完成了(或者所有跟踪的guids都被移除了)，我们不再需要跟踪这个属性的guids了
				It.RemoveCurrent();
				continue;
			}

			// 循环遍历所有的guids，并检查它们是否已经加载
			bool bMappedSomeGUIDs = false;

			for (auto unmappedIt = GuidReference.UnmappedGUIDs.CreateIterator(); unmappedIt; ++unmappedIt)
			{
				const FNetworkGUID& GUID = *unmappedIt;

				if (deltaParms.Map->IsGUIDBroken(GUID, false))
				{
					// 停止加载损坏的guids
					UE_LOG(LogAttributeBag, Warning, TEXT("AttributeBagNetSerialization: Broken GUID. NetGuid: %s"), *GUID.ToString());
					unmappedIt.RemoveCurrent();
					continue;
				}

				{
					UObject* mapObject = deltaParms.Map->GetObjectFromNetGUID(GUID, false);

					if (mapObject != nullptr)
					{
						// This guid loaded!
						if (GUID.IsDynamic())
						{
							// Move back to mapped list
							GuidReference.MappedDynamicGUIDs.Add(GUID);
						}
						unmappedIt.RemoveCurrent();
						bMappedSomeGUIDs = true;
					}
				}
			}

			// 检查是否映射了任何guids。如果这样做了，可以再次序列化该元素，这一次将加载它
			if (bMappedSomeGUIDs)
			{
				deltaParms.bOutSomeObjectsWereMapped = true;

				if (!deltaParms.bCalledPreNetReceive)
				{
					// Call PreNetReceive if we are going to change a value (some game code will need to think this is an actual replicated value)
					deltaParms.Object->PreNetReceive();
					deltaParms.bCalledPreNetReceive = true;
				}
				if (PropArray.IsValidIndex(index))
				{
					// Initialize the reader with the stored buffer that we need to read from
					FNetBitReader reader(deltaParms.Map, GuidReference.Buffer.GetData(), GuidReference.NumBufferBits);
					const FProperty* prop = PropArray[index];
					int32 propOffset = prop->GetOffset_ForInternal();
					NetSerializeItem(prop, reader, deltaParms.Map, GetMutableMemory() + propOffset);

					changes.Add(index);
				}
			}

			// 如果没有其他guids需要追踪就可以正常的移除了
			if (GuidReference.UnmappedGUIDs.Num() == 0 && GuidReference.MappedDynamicGUIDs.Num() == 0)
			{
				It.RemoveCurrent();
			}
		}

		//如果我们仍然有未映射的属性，将其传递给外部
		if (NetFragment.GuidReferencesMap.Num() > 0)
		{
			deltaParms.bOutHasMoreUnmapped = true;
		}

		for (auto index : changes)
		{
			// @TODO：通知外部
		}

		return true;
	}

	if (deltaParms.Reader)
	{
		return SerializeRead(deltaParms);
	}
	else if (deltaParms.Writer)
	{
		return SerializeWrite(deltaParms);
	}
	return false;
}

bool FAttributeEntityBag::NetSerializeDirtyItem(FArchive& Ar, UPackageMap* Map, const FNetBitArray& Changes)
{
	uint8* data = GetMutableMemory();

	TFieldIterator<FProperty> PropertyIter(GetScriptStruct());
	FNetBitArray::FFullIterator It(Changes);

	for (; It; ++It, ++PropertyIter)
	{
		FProperty* Prop = *PropertyIter;

		if (It.IsMarked())
		{
			NetSerializeItem(Prop, Ar, Map, data);
		}

		if (Ar.IsError())
		{
			return false;
		}
	}

	return true;
}

bool FAttributeEntityBag::NetSerializeItem(const FProperty* Prop, FArchive& Ar, UPackageMap* Map, void* Data)
{
	if (auto StructProp = CastField<FStructProperty>(Prop))
	{
		if (StructProp->Struct->StructFlags & STRUCT_NetSerializeNative)
		{
			return StructProp->NetSerializeItem(Ar, Map, Data);
		}
		else
		{
			// 这里之所以可以用SerializeItem，是因为Ar本身就不是普通的FArchive，而是支持Rep的NetWriter/NetReader.
			StructProp->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(), Data, nullptr);
			return true;
		}
	}
	else if (auto MapProp = CastField<FMapProperty>(Prop))
	{
		MapProp->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(), Data, nullptr);
		return true;
	}
	else if (auto ArrayProp = CastField<FArrayProperty>(Prop))
	{
		// @TODO: FastArray？
		ArrayProp->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(), Data, nullptr);
		return true;
	}

	return Prop->NetSerializeItem(Ar, Map, Data);
}

int32 FAttributeEntityBag::GetPropertyNum() const
{
	int32 Num = 0;

	for (TFieldIterator<FProperty> It(DataStruct); It; ++It)
	{
		++Num;
	}

	return Num;
}

const uint8* FAttributeEntityBag::GetMemory() const
{
	return AttributeEntity.Get(DataStruct).GetMemory();
}

uint8* FAttributeEntityBag::GetMutableMemory()
{
	return AttributeEntity.Get(DataStruct).GetMemory();
}

const UScriptStruct* FAttributeEntityBag::GetScriptStruct() const
{
	return DataStruct;
}

FAttributeNetFragment& FAttributeEntityBag::GetNetFragment()
{
	FStructView NetFragmentView = AttributeEntity.Get(FAttributeNetFragment::StaticStruct());
	if (NetFragmentView.IsValid())
	{
		return NetFragmentView.Get<FAttributeNetFragment>();
	}
	else
	{
		ensureMsgf(0, TEXT("AttributeBag's NetFragment is empty. Maybe you forgot to initialize it?"));
		static FAttributeNetFragment Empty_NetFragment;
		return Empty_NetFragment;
	}
}

FAttributeNetSharedFragment& FAttributeEntityBag::GetNetSharedFragment()
{
	return AttributeEntity.GetShared<FAttributeNetSharedFragment>();
}


//////////////////////////////////////////////////////////////////////////
// FAttributeDynamicBag
FAttributeDynamicBag::FAttributeDynamicBag()
	: Super()
{

}

FAttributeDynamicBag::~FAttributeDynamicBag()
{

}

const FAttributeBagPropertyDesc* FAttributeDynamicBag::FindPropertyDescByIndex(int32 PropertyIndex) const
{
	if (const UAttributeBagStruct* BagStruct = GetAttributeBagStruct())
	{
		return BagStruct->FindPropertyDescByIndex(PropertyIndex);
	}
	return nullptr;
}

const FAttributeBagPropertyDesc* FAttributeDynamicBag::FindPropertyDescByName(const FName Name) const
{
	if (const UAttributeBagStruct* BagStruct = GetAttributeBagStruct())
	{
		return BagStruct->FindPropertyDescByName(Name);
	}
	return nullptr;
}

int32 FAttributeDynamicBag::GetPropertyNum() const
{
	if (const UAttributeBagStruct* BagStruct = GetAttributeBagStruct())
	{
		return BagStruct->GetPropertyDescsNum();
	}
	return 0;
}

void FAttributeDynamicBag::ResetDataStruct(const UAttributeBagStruct* NewBagStruct)
{
	if (!UID.IsValid())
	{
		UID = FGuid::NewGuid();
	}
	DataStruct = const_cast<UAttributeBagStruct*>(NewBagStruct);
}

const UAttributeBagStruct* FAttributeDynamicBag::GetAttributeBagStruct() const
{
	return Cast<UAttributeBagStruct>(GetScriptStruct());
}

static FArchive& operator<<(FArchive& Ar, FAttributeBagPropertyDesc& Bag)
{
	Ar << Bag.ValueTypeObject;
	Ar << Bag.Index;
	Ar << Bag.Name;
	Ar << Bag.ValueType;

	if (Ar.IsLoading())
	{
		EAttributeBagContainerType TmpContainerType = EAttributeBagContainerType::None;
		Ar << TmpContainerType;

		if (TmpContainerType != EAttributeBagContainerType::None)
		{
			Bag.ContainerTypes.Add(TmpContainerType);
		}
	}
	else
	{
		Ar << Bag.ContainerTypes;
	}

	bool bHasMetaData = false;
#if WITH_EDITORONLY_DATA
	if (Ar.IsSaving())
	{
		bHasMetaData = !Ar.IsCooking() && Bag.MetaData.Num() > 0;
	}
#endif
	Ar << bHasMetaData;

	if (bHasMetaData)
	{
#if WITH_EDITORONLY_DATA
		Ar << Bag.MetaData;
#else
		TArray<FAttributeBagPropertyDescMetaData> TempMetaData;
		Ar << TempMetaData;
#endif
	}

	return Ar;
}

bool FAttributeDynamicBag::Serialize(FArchive& Ar)
{
	const UAttributeBagStruct* BagStruct = GetAttributeBagStruct();
	bool bHasData = (BagStruct != nullptr);

	Ar << UID;
	Ar << bHasData;

	if (bHasData)
	{
		if (Ar.IsSaving())
		{
			check(BagStruct);

			TArray<FAttributeBagPropertyDesc> PropertyDescs = BagStruct->PropertyDescs;
#if WITH_ENGINE && WITH_EDITOR
			// Save primary struct for user defined struct properties.
			// This is used as part of the user defined struct reinstancing logic.
			for (FAttributeBagPropertyDesc& Desc : PropertyDescs)
			{
				if (Desc.ValueType == EAttributeBagPropertyType::Struct)
				{
					const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(Desc.ValueTypeObject);
					if (UserDefinedStruct
						&& UserDefinedStruct->Status == EUserDefinedStructureStatus::UDSS_Duplicate
						&& UserDefinedStruct->PrimaryStruct.IsValid())
					{
						Desc.ValueTypeObject = UserDefinedStruct->PrimaryStruct.Get();
					}
				}
			}
#endif			

			// << 是自己实现的
			Ar << PropertyDescs;
		}
		else if (Ar.IsLoading())
		{
			TArray<FAttributeBagPropertyDesc> PropertyDescs;
			// << 是自己实现的
			Ar << PropertyDescs;

			DataStruct = const_cast<UAttributeBagStruct*>(UAttributeBagStruct::GetOrCreateFromDescs(PropertyDescs));
		}
	}

	return true;
}

bool FAttributeDynamicBag::NetDeltaSerialize(FNetDeltaSerializeInfo& deltaParms)
{
	return Super::NetDeltaSerialize(deltaParms);
}

bool FAttributeDynamicBag::NetSerializeDirtyItem(FArchive& Ar, UPackageMap* Map, const FNetBitArray& Changes)
{
	uint8* data = GetMutableMemory();
	int32 PropNum = GetPropertyNum();

	for (FNetBitArray::FIterator It(Changes); It; ++It)
	{
		int32 index = *It;

		if (index >= PropNum)
		{
			break;
		}

		const FAttributeBagPropertyDesc* PropertyDesc = FindPropertyDescByIndex(index);
		NetSerializeItem(PropertyDesc->CachedProperty, Ar, Map, data + PropertyDesc->CachedProperty->GetOffset_ForInternal());

		if (Ar.IsError())
		{
			return false;
		}
	}

	return true;
}

const void* FAttributeDynamicBag::GetValueAddress(const FAttributeBagPropertyDesc* Desc) const
{
	if (Desc == nullptr || !IsDataValid())
	{
		return nullptr;
	}
	return GetMemory() + Desc->CachedProperty->GetOffset_ForInternal();
}

void* FAttributeDynamicBag::GetMutableValueAddress(const FAttributeBagPropertyDesc* Desc)
{
	if (Desc == nullptr || !IsDataValid())
	{
		return nullptr;
	}
	return GetMutableMemory() + Desc->CachedProperty->GetOffset_ForInternal();
}