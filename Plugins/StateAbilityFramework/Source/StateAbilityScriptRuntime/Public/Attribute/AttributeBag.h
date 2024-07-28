#pragma once
#include "CoreMinimal.h"

#include "UObject/Interface.h"

#include "Attribute/NetBitArray.h"
#include "Attribute/Attribute.h"
#include "Attribute/AttributeEntity.h"

#include "AttributeBag.generated.h"


/*
void IDSExtraBackActionInterface::Execute_BindExtraBackAction(UObject* O, UInputAction* IA_Back)
{
	check(O != NULL);
	check(O->GetClass()->ImplementsInterface(UDSExtraBackActionInterface::StaticClass()));
	DSExtraBackActionInterface_eventBindExtraBackAction_Parms Parms;
	UFunction* const Func = O->FindFunction(NAME_UDSExtraBackActionInterface_BindExtraBackAction);
	if (Func)
	{
		Parms.IA_Back=IA_Back;
		O->ProcessEvent(Func, &Parms);
	}
	else if (auto I = (IDSExtraBackActionInterface*)(O->GetNativeInterfaceAddress(UDSExtraBackActionInterface::StaticClass())))
	{
		I->BindExtraBackAction_Implementation(IA_Back);
	}
}
*/
//////////////////////////////////////////////////////////////////////////
struct FAttributeNetGuidReference
{
	TSet<FNetworkGUID> UnmappedGUIDs;
	TSet<FNetworkGUID> MappedDynamicGUIDs;
	/** Buffer of data to re-serialize when the guids are mapped */
	TArray<uint8> Buffer;
	/** Number of bits in the buffer */
	int32 NumBufferBits;
};

//////////////////////////////////////////////////////////////////////////
// Mass Tag
USTRUCT()
struct FAttributeTagFragment : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FAttributeNetTagFragment : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FAttributeServerTagFragment : public FAttributeNetTagFragment
{
	GENERATED_BODY()
};
USTRUCT()
struct FAttributeAutonomousTagFragment : public FAttributeNetTagFragment
{
	GENERATED_BODY()
};
USTRUCT()
struct FAttributeSimulatedTagFragment : public FAttributeNetTagFragment
{
	GENERATED_BODY()
};

// Mass Fragment
USTRUCT()
struct FAttributeProviderFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<UObject*> Listeners;
};

USTRUCT()
struct FAttributeNetFragment : public FMassFragment
{
	GENERATED_BODY()

	typedef uint16 FRepPropIndex;
	TMap<FRepPropIndex, FAttributeNetGuidReference> GuidReferencesMap;
};

// Mass Shared Fragment
USTRUCT()
struct FAttributeNetSharedFragment : public FMassSharedFragment
{
	GENERATED_BODY()

	TMap<UScriptStruct*, TArray<FProperty*>> ReplicatedProps;
};

//////////////////////////////////////////////////////////////////////////

UINTERFACE()
class UAttributeListenerInterface : public UInterface
{
	GENERATED_BODY()
};

class IAttributeListenerInterface
{
	GENERATED_BODY()

public:
	virtual void OnDependencyChanged() {}

	void Initialize(UWorld* World);
	void Release(UObject* Owner);

	FMassEntityHandle EntityHandle;
	TWeakObjectPtr<UMassEntitySubsystem> EntitySubsystem;
};

USTRUCT()
struct FAttributeBag
{
	GENERATED_BODY()
public:
	FAttributeBag();
	virtual ~FAttributeBag() {}

	FAttributeBag(const FAttributeBag& InOther);
	FAttributeBag& operator=(const FAttributeBag& InOther);

	virtual void MarkDirty(const FName Name) {}
	virtual void MarkDirty(const int32 Index);
	virtual void MarkAllDirty();
	virtual void ClearDirty();

protected:
	virtual void UpdatePropertiesCompare(uint32 ReplicationFrame);
protected:
	FNetBitArray RawDirtyMark;
	FNetBitArray DirtyMark;

	uint32 LastReplicationFrame = 0;
};

//////////////////////////////////////////////////////////////////////////

USTRUCT()
struct FAttributeEntityBag : public FAttributeBag
{
	GENERATED_BODY()
public:
	FAttributeEntityBag();
	FAttributeEntityBag(const UScriptStruct* InDataStruct);

	FAttributeEntityBag(const FAttributeEntityBag& InOther);
	FAttributeEntityBag& operator=(const FAttributeEntityBag& InOther);

	virtual ~FAttributeEntityBag();
	virtual bool IsDataValid() const;
	virtual int32 GetPropertyNum() const;

	void Initialize(FAttributeEntityBuildParam& BuildParam);
	bool Serialize(FArchive& Ar);
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& deltaParms);

	const uint8* GetMemory() const;
	uint8* GetMutableMemory();
	const UScriptStruct* GetScriptStruct() const;

	template<typename T>
	T& Get();
protected:
	virtual bool SerializeRead(FNetDeltaSerializeInfo& deltaParms);
	virtual bool SerializeWrite(FNetDeltaSerializeInfo& deltaParms);
	virtual bool NetSerializeDirtyItem(FArchive& Ar, UPackageMap* Map, const FNetBitArray& Changes);

	bool NetSerializeItem(const FProperty* Prop, FArchive& Ar, UPackageMap* Map, void* Data);

	FAttributeNetFragment& GetNetFragment();
	FAttributeNetSharedFragment& GetNetSharedFragment();
protected:
	UPROPERTY()
	FGuid UID = FGuid::NewGuid();

	UPROPERTY()
	UScriptStruct* DataStruct;

	FAttributeEntity AttributeEntity;

};

template<typename T>
T& FAttributeEntityBag::Get()
{
	return AttributeEntity.Get<T>();
}

template<>
struct TStructOpsTypeTraits<FAttributeEntityBag> : public TStructOpsTypeTraitsBase2<FAttributeEntityBag>
{
	enum
	{
		WithSerializer = true,
		WithNetDeltaSerializer = true,
	};
};


//////////////////////////////////////////////////////////////////////////

USTRUCT()
struct FAttributeDynamicBag : public FAttributeEntityBag
{
	GENERATED_BODY()
public:
	FAttributeDynamicBag();
	virtual ~FAttributeDynamicBag();

	// void Initialize(FAttributeEntityBuildParam& BuildParam);

	const FAttributeBagPropertyDesc* FindPropertyDescByIndex(int32 PropertyIndex) const;
	const FAttributeBagPropertyDesc* FindPropertyDescByName(const FName Name) const;
	void ResetDataStruct(const UAttributeBagStruct* NewBagStruct);
	//////////////////////////////////////////////////////////////////////////

	virtual int32 GetPropertyNum() const override;

	const UAttributeBagStruct* GetAttributeBagStruct() const;

	bool Serialize(FArchive& Ar);
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& deltaParms);

protected:
	virtual bool NetSerializeDirtyItem(FArchive& Ar, UPackageMap* Map, const FNetBitArray& Changes) override;

	const void* GetValueAddress(const FAttributeBagPropertyDesc* Desc) const;
	void* GetMutableValueAddress(const FAttributeBagPropertyDesc* Desc);
};

template<> struct TStructOpsTypeTraits<FAttributeDynamicBag> : public TStructOpsTypeTraitsBase2<FAttributeDynamicBag>
{
	enum
	{
		WithSerializer = true,
		WithNetDeltaSerializer = true,
	};
};
