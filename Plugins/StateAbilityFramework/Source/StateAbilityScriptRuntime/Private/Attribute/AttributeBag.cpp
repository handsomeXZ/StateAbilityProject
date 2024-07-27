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

bool FAttributeDynamicBag::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	return Super::NetSerialize(Ar, Map, bOutSuccess);
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
		PropertyDesc->CachedProperty->NetSerializeItem(Ar, Map, data + PropertyDesc->CachedProperty->GetOffset_ForInternal());

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
	BuildParam.ArchetypeFragment.Add(DataStruct);
	BuildParam.ArchetypeFragment.Add(FAttributeProviderFragment::StaticStruct());
	BuildParam.ArchetypeFragment.Add(FAttributeTagFragment::StaticStruct());
	//BuildParam.ArchetypeFragment.Add(FAttributeNetRoleTagFragment::StaticStruct());

	AttributeEntity.Initialize(BuildParam);
}

bool FAttributeEntityBag::Serialize(FArchive& Ar)
{
	Ar << UID;
	Ar << DataStruct;

	return true;
}

bool FAttributeEntityBag::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	if (Ar.IsSaving())
	{
		UPackageMapClient* PackageMapClient = Cast<UPackageMapClient>(Map);
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

				if (!(this->NetSerializeDirtyItem(Ar, Map, changes)))
				{
					bOutSuccess = false;
					return false;
				}

			}
		}

	}
	else if (Ar.IsLoading())
	{
		bool bChangesIsEmpty;
		Ar << bChangesIsEmpty;

		if (!bChangesIsEmpty)
		{
			uint8* data = GetMutableMemory();

			int32 PropNum = GetPropertyNum();

			FNetBitArray changes(PropNum);
			Ar << changes;

			if (!(this->NetSerializeDirtyItem(Ar, Map, changes)))
			{
				bOutSuccess = false;
				return false;
			}

			DirtyMark |= changes;
			RawDirtyMark |= changes;
		}
	}

	bOutSuccess &= true;
	return true;
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
			Prop->NetSerializeItem(Ar, Map, data);
		}

		if (Ar.IsError())
		{
			return false;
		}
	}

	return true;
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