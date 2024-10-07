#pragma once
#include "CoreMinimal.h"

#include "UObject/Interface.h"

#include "Attribute/AttributeBag/AttributeBag.h"
#include "Attribute/NetBitArray.h"
#include "Attribute/AttributeEntity.h"

#include "Attribute/Reactive/AttributeReactive.h"
#include "AttributeBagUtils.generated.h"


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


/************************************************************************/
/* 不允许AttributeBag之间嵌套。因为属于未定义行为								*/
/************************************************************************/


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

USTRUCT()
struct STATEABILITYSCRIPTRUNTIME_API FAttributeBag
{
	GENERATED_BODY()
public:
	FAttributeBag();
	virtual ~FAttributeBag() {}

	FAttributeBag(const FAttributeBag& InOther);
	FAttributeBag& operator=(const FAttributeBag& InOther);

	virtual void MarkDirty(const FName Name, bool bValueChanged = false) {}
	virtual void MarkDirty(const int32 Index, bool bValueChanged = false);
	virtual void MarkAllDirty(bool bValueChanged = false);
	virtual void ClearDirty();
	virtual const UScriptStruct* GetScriptStruct() const { return nullptr; }

	const FGuid& GetUID() { return UID; }
protected:
	virtual void UpdatePropertiesCompare(uint32 ReplicationFrame);
protected:
	UPROPERTY()
	FGuid UID = FGuid::NewGuid();

	FNetBitArray RawDirtyMark;
	FNetBitArray DirtyMark;

	uint32 LastReplicationFrame = 0;
};

//////////////////////////////////////////////////////////////////////////

USTRUCT()
struct STATEABILITYSCRIPTRUNTIME_API FAttributeEntityBag : public FAttributeBag
{
	GENERATED_BODY()
public:
	using Super::MarkDirty;

	FAttributeEntityBag();
	FAttributeEntityBag(const UScriptStruct* InDataStruct);

	FAttributeEntityBag(const FAttributeEntityBag& InOther);
	FAttributeEntityBag& operator=(const FAttributeEntityBag& InOther);

	virtual ~FAttributeEntityBag();
	virtual void MarkDirty(const FName Name, bool bValueChanged = false) override;
	virtual const UScriptStruct* GetScriptStruct() const override;
	virtual int32 GetPropertyNum() const;
	virtual bool IsDataValid() const;
	virtual void Initialize(FAttributeEntityBuildParam& BuildParam);

	bool Serialize(FArchive& Ar);
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& deltaParms);

	const uint8* GetMemory() const;
	uint8* GetMutableMemory();

	template<typename T>
	T& Get()
	{
		return AttributeEntity.Get<T>();
	}
protected:
	virtual bool SerializeRead(FNetDeltaSerializeInfo& deltaParms);
	virtual bool SerializeWrite(FNetDeltaSerializeInfo& deltaParms);
	virtual bool NetSerializeDirtyItem(FArchive& Ar, UPackageMap* Map, const FNetBitArray& Changes);

	bool NetSerializeItem(const FProperty* Prop, FArchive& Ar, UPackageMap* Map, void* Data);

	FAttributeNetFragment& GetNetFragment();
	FAttributeNetSharedFragment& GetNetSharedFragment();
protected:
	UPROPERTY()
	UScriptStruct* DataStruct;

	FAttributeEntity AttributeEntity;

};

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
struct STATEABILITYSCRIPTRUNTIME_API FAttributeDynamicBag : public FAttributeEntityBag
{
	GENERATED_BODY()
public:
	using Super::MarkDirty;

	FAttributeDynamicBag();
	FAttributeDynamicBag(const UScriptStruct* InDataStruct);

	FAttributeDynamicBag(const FAttributeDynamicBag& InOther) : Super(InOther) {}
	FAttributeDynamicBag& operator=(const FAttributeDynamicBag& InOther);

	virtual ~FAttributeDynamicBag();

	// void Initialize(FAttributeEntityBuildParam& BuildParam);

	const FAttributeBagPropertyDesc* FindPropertyDescByIndex(int32 PropertyIndex) const;
	const FAttributeBagPropertyDesc* FindPropertyDescByName(const FName Name) const;
	virtual void ResetDataStruct(const UAttributeBagStruct* NewBagStruct);

	virtual void MarkDirty(const FName Name, bool bValueChanged = false) override;
	virtual int32 GetPropertyNum() const override;
	//////////////////////////////////////////////////////////////////////////

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


//////////////////////////////////////////////////////////////////////////

USTRUCT()
struct STATEABILITYSCRIPTRUNTIME_API FAttributeReactiveBag : public FAttributeDynamicBag
{
	GENERATED_BODY()
public:
	using Super::MarkDirty;

	FAttributeReactiveBag();
	//FAttributeReactiveBag(const UScriptStruct* InModelStruct); 不允许直接传ModelStruct，因为无法校验类型

	FAttributeReactiveBag(const FAttributeReactiveBag& InOther);
	FAttributeReactiveBag& operator=(const FAttributeReactiveBag& InOther);

	virtual ~FAttributeReactiveBag() {}

	virtual bool IsDataValid() const override;
	virtual void Initialize(FAttributeEntityBuildParam& BuildParam) override;
	virtual void MarkDirty(const int32 Index, bool bValueChanged = false) override;
	virtual void MarkAllDirty(bool bValueChanged = false) override;
	
	template<typename TStruct>
	void InitializeReactive();

	bool Serialize(FArchive& Ar);
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& deltaParms);

	const void* GetValueAddress(const FAttributeBagPropertyDesc* Desc) const;
	void* GetMutableValueAddress(const FAttributeBagPropertyDesc* Desc);
protected:
	UScriptStruct* ModelStruct;

	int32 CppPropCount;
};

template<> struct TStructOpsTypeTraits<FAttributeReactiveBag> : public TStructOpsTypeTraitsBase2<FAttributeReactiveBag>
{
	enum
	{
		WithSerializer = true,
		WithNetDeltaSerializer = true,
	};
};

template<typename TStruct>
inline void FAttributeReactiveBag::InitializeReactive()
{
	static_assert(TIsDerivedFrom<TStruct, FReactiveModelBase>::IsDerived, "Must be initialized by a struct derived from FReactiveModelBase.");
	static_assert(TIsDerivedFrom<TStruct, FMassFragment>::IsDerived, "Must be initialized by a struct derived from FMassFragment.");

	ModelStruct = TStruct::StaticStruct();

	DataStruct = UAttributeBagStruct::GetOrCreateFromScriptStruct_NoShrink(ModelStruct);
	DataStruct->SetSuperStruct(FMassFragment::StaticStruct());

	CppPropCount = FReactiveModelTypeTraitsBase<TStruct>::AttributeCountTotal;
}

USTRUCT()
struct STATEABILITYSCRIPTRUNTIME_API FAttributeReactiveBagDataBase : public FMassFragment, public TReactiveModel<FAttributeReactiveBagDataBase>
{
	GENERATED_BODY()
	REACTIVE_BODY(FAttributeReactiveBagDataBase);
};
