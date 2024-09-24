#pragma once
#include "CoreMinimal.h"

#include "StructView.h"
#include "Templates/ValueOrError.h"
#include "Containers/StaticArray.h"

#include "AttributeBag.generated.h"

typedef int32 int32;
typedef int32 ReplicateIndexType;

STATEABILITYSCRIPTRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogStateAbilityAttrubuteBag, Log, All);

namespace Attribute::StructUtils
{
	bool CanCastTo(const UStruct* From, const UStruct* To);
	uint64 GetObjectHash(const UObject* Object);
	uint64 CalcPropertyDescHash(const FAttributeBagPropertyDesc& Desc);
	uint64 CalcPropertyDescArrayHash(const TConstArrayView<FAttributeBagPropertyDesc> Descs);
	FAttributeBagContainerTypes GetContainerTypesFromProperty(const FProperty* InSourceProperty);
	EAttributeBagPropertyType GetValueTypeFromProperty(const FProperty* InSourceProperty);
	UObject* GetValueTypeObjectFromProperty(const FProperty* InSourceProperty);
	FProperty* CreatePropertyFromDesc(const FAttributeBagPropertyDesc& Desc, const FFieldVariant PropertyScope);
	
	// Helper functions to get and set property values

	//----------------------------------------------------------------//
	//  Getters
	//----------------------------------------------------------------//
	EAttributeBagResult GetPropertyAsInt64(const FAttributeBagPropertyDesc* Desc, const void* Address, int64& OutValue);
	EAttributeBagResult GetPropertyAsDouble(const FAttributeBagPropertyDesc* Desc, const void* Address, double& OutValue);
	// Generic property getter. Used for FName, FString, FText. 
	template<typename T, typename PropT>
	EAttributeBagResult GetPropertyValue(const FAttributeBagPropertyDesc* Desc, const void* Address, T& OutValue);
	EAttributeBagResult GetPropertyValueAsEnum(const FAttributeBagPropertyDesc* Desc, const UEnum* RequestedEnum, const void* Address, uint8& OutValue);
	EAttributeBagResult GetPropertyValueAsStruct(const FAttributeBagPropertyDesc* Desc, const UScriptStruct* RequestedStruct, const void* Address, FStructView& OutValue);
	EAttributeBagResult GetPropertyValueAsObject(const FAttributeBagPropertyDesc* Desc, const UClass* RequestedClass, const void* Address, UObject*& OutValue);
	EAttributeBagResult GetPropertyValueAsSoftPath(const FAttributeBagPropertyDesc* Desc, const void* Address, FSoftObjectPath& OutValue);


	FString GetPropertyValueAsString(const FAttributeBagPropertyDesc* Desc, const void* Address);	// 强制获取Value的String显示
	//----------------------------------------------------------------//
	//  Setters
	//----------------------------------------------------------------//
	EAttributeBagResult SetPropertyFromInt64(const FAttributeBagPropertyDesc* Desc, void* Address, const int64 InValue);
	EAttributeBagResult SetPropertyFromDouble(const FAttributeBagPropertyDesc* Desc, void* Address, const double InValue);
	// Generic property setter. Used for FName, FString, FText 
	template<typename T, typename PropT>
	EAttributeBagResult SetPropertyValue(const FAttributeBagPropertyDesc* Desc, void* Address, const T& InValue);
	EAttributeBagResult SetPropertyValueAsEnum(const FAttributeBagPropertyDesc* Desc, void* Address, const uint8 InValue, const UEnum* Enum);
	EAttributeBagResult SetPropertyValueAsStruct(const FAttributeBagPropertyDesc* Desc, void* Address, const FConstStructView InValue);
	EAttributeBagResult SetPropertyValueAsObject(const FAttributeBagPropertyDesc* Desc, void* Address, UObject* InValue);
	EAttributeBagResult SetPropertyValueAsSoftPath(const FAttributeBagPropertyDesc* Desc, void* Address, const FSoftObjectPath& InValue);
	void CopyMatchingValuesByIndex(const FConstStructView Source, FStructView Target);
	void RemovePropertyByName(TArray<FAttributeBagPropertyDesc>& Descs, const FName PropertyName, const int32 StartIndex = 0);
}

/** Getter and setter result code. */
UENUM()
enum class EAttributeBagResult : uint8
{
	Success,			// Operation succeeded.
	TypeMismatch,		// Tried to access mismatching type (e.g. setting a struct to bool)
	OutOfBounds,		// Tried to access an array property out of bounds.
	PropertyNotFound,	// Could not find property of specified name.
	ValueIsInValid,
	MissingImplementation,
};

/** Attribute bag property type, loosely based on BluePrint pin types. */
UENUM(BlueprintType)
enum class EAttributeBagPropertyType : uint8
{
	None UMETA(Hidden),
	Bool,
	Byte,
	Int32,
	Int64,
	Float,
	Double,
	Name,
	String,
	Text,
	Enum UMETA(Hidden),
	Struct UMETA(Hidden),
	Object UMETA(Hidden),
	SoftObject UMETA(Hidden),
	Class UMETA(Hidden),
	SoftClass UMETA(Hidden),

	Count UMETA(Hidden)
};

/** Attribute bag property container type. */
UENUM(BlueprintType)
enum class EAttributeBagContainerType : uint8
{
	None,
	Array,

	Count UMETA(Hidden)
};

/** Helper to manage container types, with nested container support. */
USTRUCT()
struct FAttributeBagContainerTypes
{
	GENERATED_BODY()

	FAttributeBagContainerTypes() = default;

	explicit FAttributeBagContainerTypes(EAttributeBagContainerType ContainerType)
	{
		if (ContainerType != EAttributeBagContainerType::None)
		{
			Add(ContainerType);
		}
	}

	FAttributeBagContainerTypes(const std::initializer_list<EAttributeBagContainerType>& InTypes)
	{
		for (const EAttributeBagContainerType ContainerType : InTypes)
		{
			if (ContainerType != EAttributeBagContainerType::None)
			{
				Add(ContainerType);
			}
		}
	}

	bool Add(const EAttributeBagContainerType AttributeBagContainerType)
	{
		if (ensure(NumContainers < MaxNestedTypes))
		{
			if (AttributeBagContainerType != EAttributeBagContainerType::None)
			{
				Types[NumContainers] = AttributeBagContainerType;
				NumContainers++;

				return true;
			}
		}

		return false;
	}

	void Reset()
	{
		for (EAttributeBagContainerType& Type : Types)
		{
			Type = EAttributeBagContainerType::None;
		}
		NumContainers = 0;
	}

	bool IsEmpty() const
	{
		return NumContainers == 0;
	}

	uint32 Num() const
	{
		return NumContainers;
	}

	bool CanAdd() const
	{
		return NumContainers < MaxNestedTypes;
	}

	EAttributeBagContainerType GetFirstContainerType() const
	{
		return NumContainers > 0 ? Types[0] : EAttributeBagContainerType::None;
	}

	EAttributeBagContainerType operator[] (int32 Index) const
	{
		return ensure(Index < NumContainers) ? Types[Index] : EAttributeBagContainerType::None;
	}

	EAttributeBagContainerType PopHead();

	void Serialize(FArchive& Ar);

	friend FArchive& operator<<(FArchive& Ar, FAttributeBagContainerTypes& ContainerTypesData)
	{
		ContainerTypesData.Serialize(Ar);
		return Ar;
	}

	bool operator == (const FAttributeBagContainerTypes& Other) const;

	FORCEINLINE bool operator !=(const FAttributeBagContainerTypes& Other) const
	{
		return !(Other == *this);
	}

	friend FORCEINLINE uint32 GetTypeHash(const FAttributeBagContainerTypes& AttributeBagContainerTypes)
	{
		return GetArrayHash(AttributeBagContainerTypes.Types.GetData(), AttributeBagContainerTypes.NumContainers);
	}

	EAttributeBagContainerType* begin() { return &Types[0]; }
	const EAttributeBagContainerType* begin() const { return &Types[0]; }
	EAttributeBagContainerType* end() { return &Types[NumContainers]; }
	const EAttributeBagContainerType* end() const { return &Types[NumContainers]; }

protected:
	static constexpr uint8 MaxNestedTypes = 2;

	TStaticArray<EAttributeBagContainerType, MaxNestedTypes> Types = TStaticArray<EAttributeBagContainerType, MaxNestedTypes>(InPlace, EAttributeBagContainerType::None);
	uint8 NumContainers = 0;
};

USTRUCT()
struct FAttributeBagPropertyDescMetaData
{
	GENERATED_BODY()

	FAttributeBagPropertyDescMetaData() = default;
	FAttributeBagPropertyDescMetaData(const FName InKey, const FString& InValue)
		: Key(InKey)
		, Value(InValue)
	{
	}

	UPROPERTY()
	FName Key;

	UPROPERTY()
	FString Value;

	void Serialize(FArchive& Ar);

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FAttributeBagPropertyDescMetaData& PropertyDescMetaData)
	{
		PropertyDescMetaData.Serialize(Ar);
		return Ar;
	}

	friend FORCEINLINE uint32 GetTypeHash(const FAttributeBagPropertyDescMetaData& PropertyDescMetaData)
	{
		return HashCombine(GetTypeHash(PropertyDescMetaData.Key), GetTypeHash(PropertyDescMetaData.Value));
	}

	friend FORCEINLINE uint32 GetTypeHash(const TArrayView<const FAttributeBagPropertyDescMetaData>& MetaData)
	{
		uint32 Hash = GetTypeHash(MetaData.Num());
		for (const FAttributeBagPropertyDescMetaData& PropertyDescMetaData : MetaData)
		{
			Hash = HashCombine(Hash, GetTypeHash(PropertyDescMetaData));
		}
		return Hash;
	}

	friend FORCEINLINE uint32 GetTypeHash(const TArray<FAttributeBagPropertyDescMetaData>& MetaData)
	{
		return GetTypeHash(TArrayView<const FAttributeBagPropertyDescMetaData>(MetaData.GetData(), MetaData.Num()));
	}
};

USTRUCT()
struct STATEABILITYSCRIPTRUNTIME_API FAttributeBagPropertyDesc
{
	GENERATED_BODY()

	FAttributeBagPropertyDesc() = default;
	FAttributeBagPropertyDesc(const FName InName, const FProperty* InSourceProperty);
	FAttributeBagPropertyDesc(const FName InName, const EAttributeBagPropertyType InValueType, const UObject* InValueTypeObject = nullptr, const int32 Index = 0)
		: ValueTypeObject(InValueTypeObject)
		, Index(Index)
		, Name(InName)
		, ValueType(InValueType)
	{
	}
	FAttributeBagPropertyDesc(const FName InName, const EAttributeBagContainerType InContainerType, const EAttributeBagPropertyType InValueType, const UObject* InValueTypeObject = nullptr, const int32 Index = 0)
		: ValueTypeObject(InValueTypeObject)
		, Index(Index)
		, Name(InName)
		, ValueType(InValueType)
		, ContainerTypes(InContainerType)
	{
	}

	FAttributeBagPropertyDesc(const FName InName, const FAttributeBagContainerTypes& InNestedContainers, const EAttributeBagPropertyType InValueType, UObject* InValueTypeObject = nullptr, const int32 Index = 0)
		: ValueTypeObject(InValueTypeObject)
		, Index(Index)
		, Name(InName)
		, ValueType(InValueType)
		, ContainerTypes(InNestedContainers)
	{
	}

	/** @return true if the two descriptors have the same type. Object types are compatible if Other can be cast to this type. */
	bool CompatibleType(const FAttributeBagPropertyDesc& Other) const;

	/** @return true if the property type is numeric (bool, int32, int64, float, double, enum) */
	bool IsNumericType() const;

	/** @return true if the property type is floating point numeric (float, double) */
	bool IsNumericFloatType() const;

	/** @return true if the property type is object or soft object */
	bool IsObjectType() const;

	/** @return true if the property type is class or soft class */
	bool IsClassType() const;

	/** Pointer to object that defines the UEnum, UStruct, or UClass. */
	UPROPERTY(EditAnywhere, Category = "Default")
	TObjectPtr<const UObject> ValueTypeObject = nullptr;

	/** Used as main identifier when copying values over. */
	UPROPERTY(EditAnywhere, Category = "Default")
	int32 Index;

	/** Name for the property. */
	UPROPERTY(EditAnywhere, Category = "Default")
	FName Name;

	/** Type of the value described by this property. */
	UPROPERTY(EditAnywhere, Category = "Default")
	EAttributeBagPropertyType ValueType = EAttributeBagPropertyType::None;

	/** Type of the container described by this property. */
	UPROPERTY(EditAnywhere, Category = "Default")
	FAttributeBagContainerTypes ContainerTypes;

#if WITH_EDITORONLY_DATA
	/** Editor-only meta data for CachedProperty */
	UPROPERTY(EditAnywhere, Category = "Default")
	TArray<FAttributeBagPropertyDescMetaData> MetaData;
#endif

	/** Cached property pointer, set in UAttributeBagStruct::GetOrCreateFromDescs. */
	const FProperty* CachedProperty = nullptr;
};

/**
 * Dummy types used to mark up missing types when creating property bags. These are used in the UI to display error message.
 */
UENUM()
enum class EAttributeBagMissingEnum : uint8
{
	Missing,
};

USTRUCT()
struct FAttributeBagMissingStruct
{
	GENERATED_BODY()
};

UCLASS()
class UAttributeBagMissingObject : public UObject
{
	GENERATED_BODY()
};

/**
 * A script struct that is used to store the value of the property bag instance.
 * References to UAttributeBagStruct cannot be serialized, instead the array of the properties
 * is serialized and new class is create on load based on the composition of the properties.
 *
 * Note: Should not be used directly.
 */
UCLASS(Transient)
class STATEABILITYSCRIPTRUNTIME_API UAttributeBagStruct : public UScriptStruct
{
public:
	GENERATED_BODY()

	/**
	 * Creates new UAttributeBagStruct struct based on the properties passed in.
	 * If there are multiple properties that have the same name, only the first one is added.
	 */
	static const UAttributeBagStruct* GetOrCreateFromDescs(const TConstArrayView<FAttributeBagPropertyDesc> InPropertyDescs);
	static const UAttributeBagStruct* GetOrCreateFromScriptStruct(const UScriptStruct* ScriptStruct);

	TConstArrayView<FAttributeBagPropertyDesc> GetPropertyDescs() const { return PropertyDescs; }
	const FAttributeBagPropertyDesc* FindPropertyDescByIndex(const int32 Index) const;
	const FAttributeBagPropertyDesc* FindPropertyDescByName(const FName Name) const;

	int32 GetPropertyDescsNum() const { return PropertyDescs.Num(); }

#if WITH_ENGINE && WITH_EDITOR
	/** @return true if any of the properties on the bag has type of the specified user defined struct. */
	bool ContainsUserDefinedStruct(const UUserDefinedStruct* UserDefinedStruct) const;
#endif
protected:
	friend struct FAttributeDynamicBag;

	void DecrementRefCount() const;
	void IncrementRefCount() const;

	virtual void InitializeStruct(void* Dest, int32 ArrayDim = 1) const override;
	virtual void DestroyStruct(void* Dest, int32 ArrayDim = 1) const override;
	virtual void FinishDestroy();

	UPROPERTY()
	TArray<FAttributeBagPropertyDesc> PropertyDescs;

	std::atomic<int32> RefCount = 0;
};



template<typename T, typename PropT>
EAttributeBagResult Attribute::StructUtils::SetPropertyValue(const FAttributeBagPropertyDesc* Desc, void* Address, const T& InValue)
{
	if (Desc == nullptr || Desc->CachedProperty == nullptr)
	{
		return EAttributeBagResult::PropertyNotFound;
	}
	if (Address == nullptr)
	{
		return EAttributeBagResult::OutOfBounds;
	}
	if (Desc->ContainerTypes.Num() > 0)
	{
		return EAttributeBagResult::TypeMismatch;
	}

	if (!Desc->CachedProperty->IsA<PropT>())
	{
		return EAttributeBagResult::TypeMismatch;
	}

	const PropT* Property = CastFieldChecked<PropT>(Desc->CachedProperty);
	Property->SetPropertyValue(Address, InValue);

	return EAttributeBagResult::Success;
};

template<typename T, typename PropT>
EAttributeBagResult Attribute::StructUtils::GetPropertyValue(const FAttributeBagPropertyDesc* Desc, const void* Address, T& OutValue)
{
	if (Desc == nullptr || Desc->CachedProperty == nullptr)
	{
		return EAttributeBagResult::PropertyNotFound;
	}
	if (Address == nullptr)
	{
		return EAttributeBagResult::OutOfBounds;
	}
	if (Desc->ContainerTypes.Num() > 0)
	{
		return EAttributeBagResult::TypeMismatch;
	}

	if (!Desc->CachedProperty->IsA<PropT>())
	{
		return EAttributeBagResult::TypeMismatch;
	}

	const PropT* Property = CastFieldChecked<PropT>(Desc->CachedProperty);
	OutValue = Property->GetPropertyValue(Address);

	return EAttributeBagResult::Success;
};

// TFieldIterator 的访问顺序是从子类往基类遍历。
// TFieldIteratorFromBaseStruct 则是从基类往子类遍历。
template <class T>
class TFieldIteratorFromBaseStruct
{
private:
	/** The object being searched for the specified field */
	const UStruct* Struct;

	int32 StructDepth = 0;

	/** The current location in the list of fields being iterated */
	typename T::BaseFieldClass* Field;
	/** The index of the current interface being iterated */
	int32 InterfaceIndex;
	/** Whether to include the super class or not */
	const bool bIncludeSuper;
	/** Whether to include deprecated fields or not */
	const bool bIncludeDeprecated;
	/** Whether to include interface fields or not */
	const bool bIncludeInterface;

public:
	TFieldIteratorFromBaseStruct(const UStruct* InStruct, EFieldIterationFlags InIterationFlags = EFieldIterationFlags::Default)
		: Struct(InStruct)
		, Field(nullptr)
		, InterfaceIndex(-1)
		, bIncludeSuper(EnumHasAnyFlags(InIterationFlags, EFieldIterationFlags::IncludeSuper))
		, bIncludeDeprecated(EnumHasAnyFlags(InIterationFlags, EFieldIterationFlags::IncludeDeprecated))
		, bIncludeInterface(EnumHasAnyFlags(InIterationFlags, EFieldIterationFlags::IncludeInterfaces) && InStruct&& InStruct->IsA(UClass::StaticClass()))
	{
		const UStruct* CurrentStruct = Struct;

		while (CurrentStruct)
		{
			StructLink.Add(CurrentStruct);
			CurrentStruct = CurrentStruct->GetInheritanceSuper();
		}

		StructDepth = StructLink.Num() - 1;

		Field = GetChildFieldsFromStruct<typename T::BaseFieldClass>(StructLink[StructDepth]);

		IterateToNext();
	}

	/** Legacy version taking the flags as 3 separate values */
	TFieldIteratorFromBaseStruct(const UStruct* InStruct,
		EFieldIteratorFlags::SuperClassFlags         InSuperClassFlags,
		EFieldIteratorFlags::DeprecatedPropertyFlags InDeprecatedFieldFlags = EFieldIteratorFlags::IncludeDeprecated,
		EFieldIteratorFlags::InterfaceClassFlags     InInterfaceFieldFlags = EFieldIteratorFlags::ExcludeInterfaces)
		: TFieldIteratorFromBaseStruct(InStruct, (EFieldIterationFlags)(InSuperClassFlags | InDeprecatedFieldFlags | InInterfaceFieldFlags))
	{
	}

	/** conversion to "bool" returning true if the iterator is valid. */
	FORCEINLINE explicit operator bool() const
	{
		return Field != NULL;
	}
	/** inverse of the "bool" operator */
	FORCEINLINE bool operator !() const
	{
		return !(bool)*this;
	}

	inline bool operator==(const TFieldIteratorFromBaseStruct<T>& Rhs) const { return Field == Rhs.Field; }
	inline bool operator!=(const TFieldIteratorFromBaseStruct<T>& Rhs) const { return Field != Rhs.Field; }

	inline void operator++()
	{
		checkSlow(Field);
		Field = Field->Next;
		IterateToNext();
	}
	inline T* operator*()
	{
		checkSlow(Field);
		return (T*)Field;
	}
	inline const T* operator*() const
	{
		checkSlow(Field);
		return (const T*)Field;
	}
	inline T* operator->()
	{
		checkSlow(Field);
		return (T*)Field;
	}
	inline const UStruct* GetStruct()
	{
		return Struct;
	}
protected:
	TArray<const UStruct*> StructLink;

	inline void IterateToNext()
	{
		typename T::BaseFieldClass* CurrentField = Field;
		const UStruct* CurrentStruct = StructLink[StructDepth];

		while (CurrentStruct)
		{
			while (CurrentField)
			{
				typename T::FieldTypeClass* FieldClass = CurrentField->GetClass();

				if (FieldClass->HasAllCastFlags(T::StaticClassCastFlags()) &&
					(
						bIncludeDeprecated
						|| !FieldClass->HasAllCastFlags(CASTCLASS_FProperty)
						|| !((FProperty*)CurrentField)->HasAllPropertyFlags(CPF_Deprecated)
						)
					)
				{
					Struct = CurrentStruct;
					Field = CurrentField;
					return;
				}

				CurrentField = CurrentField->Next;
			}

			if (bIncludeInterface)
			{
				// We shouldn't be able to get here for non-classes
				UClass* CurrentClass = (UClass*)CurrentStruct;
				++InterfaceIndex;
				if (InterfaceIndex < CurrentClass->Interfaces.Num())
				{
					FImplementedInterface& Interface = CurrentClass->Interfaces[InterfaceIndex];
					CurrentField = Interface.Class ? GetChildFieldsFromStruct<typename T::BaseFieldClass>(Interface.Class) : nullptr;
					continue;
				}
			}

			if (bIncludeSuper)
			{
				--StructDepth;
				if (StructDepth >= 0)
				{
					CurrentStruct = StructLink[StructDepth];
					CurrentField = GetChildFieldsFromStruct<typename T::BaseFieldClass>(CurrentStruct);
					InterfaceIndex = -1;
					continue;
				}
			}

			break;
		}

		Struct = CurrentStruct;
		Field = CurrentField;
	}
};