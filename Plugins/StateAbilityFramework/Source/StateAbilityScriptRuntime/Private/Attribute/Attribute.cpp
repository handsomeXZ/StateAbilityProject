#include "Attribute/Attribute.h"

#include "UObject/EnumProperty.h"
#include "UObject/Package.h"
#include "UObject/TextProperty.h"

#if WITH_ENGINE && WITH_EDITOR
#include "Engine/UserDefinedStruct.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ArchiveUObject.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#endif

DEFINE_LOG_CATEGORY(LogStateAbilityAttrubuteBag);

namespace Attribute::StructUtils
{

	bool CanCastTo(const UStruct* From, const UStruct* To)
	{
		return From != nullptr && To != nullptr && From->IsChildOf(To);
	}

	uint64 GetObjectHash(const UObject* Object)
	{
		const FString PathName = GetPathNameSafe(Object);
		return CityHash64((const char*)GetData(PathName), PathName.Len() * sizeof(TCHAR));
	}

	uint64 CalcPropertyDescHash(const FAttributeBagPropertyDesc& Desc)
	{
#if WITH_EDITORONLY_DATA
		const uint32 Hashes[] = { GetTypeHash(Desc.Index), GetTypeHash(Desc.Name), GetTypeHash(Desc.ValueType), GetTypeHash(Desc.ContainerTypes), GetTypeHash(Desc.MetaData) };
#else
		const uint32 Hashes[] = { GetTypeHash(Desc.Index), GetTypeHash(Desc.Name), GetTypeHash(Desc.ValueType), GetTypeHash(Desc.ContainerTypes) };
#endif
		return CityHash64WithSeed((const char*)Hashes, sizeof(Hashes), GetObjectHash(Desc.ValueTypeObject));
	}

	uint64 CalcPropertyDescArrayHash(const TConstArrayView<FAttributeBagPropertyDesc> Descs)
	{
		uint64 Hash = 0;
		for (const FAttributeBagPropertyDesc& Desc : Descs)
		{
			Hash = CityHash128to64(Uint128_64(Hash, CalcPropertyDescHash(Desc)));
		}
		return Hash;
	}

	FAttributeBagContainerTypes GetContainerTypesFromProperty(const FProperty* InSourceProperty)
	{
		FAttributeBagContainerTypes ContainerTypes;

		while (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InSourceProperty))
		{
			if (ContainerTypes.Add(EAttributeBagContainerType::Array))
			{
				InSourceProperty = CastField<FArrayProperty>(ArrayProperty->Inner);
			}
			else // we reached the nested containers limit
			{
				ContainerTypes.Reset();
				break;
			}
		}

		return ContainerTypes;
	}

	EAttributeBagPropertyType GetValueTypeFromProperty(const FProperty* InSourceProperty)
	{
		if (CastField<FBoolProperty>(InSourceProperty))
		{
			return EAttributeBagPropertyType::Bool;
		}
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(InSourceProperty))
		{
			return ByteProperty->IsEnum() ? EAttributeBagPropertyType::Enum : EAttributeBagPropertyType::Byte;
		}
		if (CastField<FIntProperty>(InSourceProperty))
		{
			return EAttributeBagPropertyType::Int32;
		}
		if (CastField<FInt64Property>(InSourceProperty))
		{
			return EAttributeBagPropertyType::Int64;
		}
		if (CastField<FFloatProperty>(InSourceProperty))
		{
			return EAttributeBagPropertyType::Float;
		}
		if (CastField<FDoubleProperty>(InSourceProperty))
		{
			return EAttributeBagPropertyType::Double;
		}
		if (CastField<FNameProperty>(InSourceProperty))
		{
			return EAttributeBagPropertyType::Name;
		}
		if (CastField<FStrProperty>(InSourceProperty))
		{
			return EAttributeBagPropertyType::String;
		}
		if (CastField<FTextProperty>(InSourceProperty))
		{
			return EAttributeBagPropertyType::Text;
		}
		if (CastField<FEnumProperty>(InSourceProperty))
		{
			return EAttributeBagPropertyType::Enum;
		}
		if (CastField<FStructProperty>(InSourceProperty))
		{
			return EAttributeBagPropertyType::Struct;
		}
		if (CastField<FObjectProperty>(InSourceProperty))
		{
			return EAttributeBagPropertyType::Object;
		}
		if (CastField<FSoftObjectProperty>(InSourceProperty))
		{
			return EAttributeBagPropertyType::SoftObject;
		}
		if (CastField<FClassProperty>(InSourceProperty))
		{
			return EAttributeBagPropertyType::Class;
		}
		if (CastField<FSoftClassProperty>(InSourceProperty))
		{
			return EAttributeBagPropertyType::SoftClass;
		}

		// Handle array property
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InSourceProperty))
		{
			return GetValueTypeFromProperty(ArrayProperty->Inner);
		}

		return EAttributeBagPropertyType::None;
	}

	UObject* GetValueTypeObjectFromProperty(const FProperty* InSourceProperty)
	{
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(InSourceProperty))
		{
			if (ByteProperty->IsEnum())
			{
				return ByteProperty->Enum;
			}
		}
		if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(InSourceProperty))
		{
			return EnumProp->GetEnum();
		}
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(InSourceProperty))
		{
			return StructProperty->Struct;
		}
		if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InSourceProperty))
		{
			return ObjectProperty->PropertyClass;
		}
		if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(InSourceProperty))
		{
			return SoftObjectProperty->PropertyClass;
		}
		if (const FClassProperty* ClassProperty = CastField<FClassProperty>(InSourceProperty))
		{
			return ClassProperty->PropertyClass;
		}
		if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(InSourceProperty))
		{
			return SoftClassProperty->PropertyClass;
		}

		// Handle array property
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InSourceProperty))
		{
			return GetValueTypeObjectFromProperty(ArrayProperty->Inner);
		}


		return nullptr;
	}

	FProperty* CreatePropertyFromDesc(const FAttributeBagPropertyDesc& Desc, const FFieldVariant PropertyScope)
	{
		// Handle array and nested containers properties
		if (Desc.ContainerTypes.Num() > 0)
		{
			FProperty* Prop = nullptr; // the first created container will fill the return value, nested ones will fill the inner

			// support for nested containers, i.e. : TArray<TArray<float>>
			FFieldVariant PropertyOwner = PropertyScope;
			FProperty** ValuePropertyPtr = &Prop;
			FString ParentName;

			// Create the container list
			for (EAttributeBagContainerType BagContainerType : Desc.ContainerTypes)
			{
				switch (BagContainerType)
				{
				case EAttributeBagContainerType::Array:
				{
					// create an array property as a container for the tail
					const FString PropName = ParentName.IsEmpty() ? Desc.Name.ToString() : ParentName + TEXT("_InnerArray");
					FArrayProperty* ArrayProperty = new FArrayProperty(PropertyOwner, FName(PropName), RF_Public);
					*ValuePropertyPtr = ArrayProperty;
					ValuePropertyPtr = &ArrayProperty->Inner;
					PropertyOwner = ArrayProperty;
					ParentName = PropName;
					break;
				}
				default:
					ensureMsgf(false, TEXT("Unsuported container type %s"), *UEnum::GetValueAsString(BagContainerType));
					break;
				}
			}

			// finally create the tail type
			FAttributeBagPropertyDesc InnerDesc = Desc;
			InnerDesc.Name = FName(ParentName + TEXT("_InnerType"));
			InnerDesc.ContainerTypes.Reset();
			*ValuePropertyPtr = CreatePropertyFromDesc(InnerDesc, PropertyOwner);

			return Prop;
		}

		switch (Desc.ValueType)
		{
		case EAttributeBagPropertyType::Bool:
		{
			FBoolProperty* Prop = new FBoolProperty(PropertyScope, Desc.Name, RF_Public);
			Prop->SetBoolSize(sizeof(bool), true); // Enable native access (init the whole byte, rather than just first bit)
			return Prop;
		}
		case EAttributeBagPropertyType::Byte:
		{
			FByteProperty* Prop = new FByteProperty(PropertyScope, Desc.Name, RF_Public);
			Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
			return Prop;
		}
		case EAttributeBagPropertyType::Int32:
		{
			FIntProperty* Prop = new FIntProperty(PropertyScope, Desc.Name, RF_Public);
			Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
			return Prop;
		}
		case EAttributeBagPropertyType::Int64:
		{
			FInt64Property* Prop = new FInt64Property(PropertyScope, Desc.Name, RF_Public);
			Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
			return Prop;
		}
		case EAttributeBagPropertyType::Float:
		{
			FFloatProperty* Prop = new FFloatProperty(PropertyScope, Desc.Name, RF_Public);
			Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
			return Prop;
		}
		case EAttributeBagPropertyType::Double:
		{
			FDoubleProperty* Prop = new FDoubleProperty(PropertyScope, Desc.Name, RF_Public);
			Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
			return Prop;
		}
		case EAttributeBagPropertyType::Name:
		{
			FNameProperty* Prop = new FNameProperty(PropertyScope, Desc.Name, RF_Public);
			Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
			return Prop;
		}
		case EAttributeBagPropertyType::String:
		{
			FStrProperty* Prop = new FStrProperty(PropertyScope, Desc.Name, RF_Public);
			Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
			return Prop;
		}
		case EAttributeBagPropertyType::Text:
		{
			FTextProperty* Prop = new FTextProperty(PropertyScope, Desc.Name, RF_Public);
			return Prop;
		}
		case EAttributeBagPropertyType::Enum:
			if (UEnum* Enum = const_cast<UEnum*>(Cast<UEnum>(Desc.ValueTypeObject)))
			{
				FEnumProperty* Prop = new FEnumProperty(PropertyScope, Desc.Name, RF_Public);
				FNumericProperty* UnderlyingProp = new FByteProperty(Prop, "UnderlyingType", RF_Public); // HACK: Hardwire to byte property for now for BP compatibility
				Prop->SetEnum(Enum);
				Prop->AddCppProperty(UnderlyingProp);
				return Prop;
			}
			break;
		case EAttributeBagPropertyType::Struct:
			if (UScriptStruct* ScriptStruct = const_cast<UScriptStruct*>(Cast<UScriptStruct>(Desc.ValueTypeObject)))
			{
				FStructProperty* Prop = new FStructProperty(PropertyScope, Desc.Name, RF_Public);
				Prop->Struct = ScriptStruct;

				if (ScriptStruct->GetCppStructOps() && ScriptStruct->GetCppStructOps()->HasGetTypeHash())
				{
					Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				}

				if (ScriptStruct->StructFlags & STRUCT_HasInstancedReference)
				{
					Prop->SetPropertyFlags(CPF_ContainsInstancedReference);
				}

				return Prop;
			}
			break;
		case EAttributeBagPropertyType::Object:
			if (UClass* Class = const_cast<UClass*>(Cast<UClass>(Desc.ValueTypeObject)))
			{
				FObjectProperty* Prop = new FObjectProperty(PropertyScope, Desc.Name, RF_Public);
				if (Class->HasAnyClassFlags(CLASS_DefaultToInstanced))
				{
					Prop->SetPropertyFlags(CPF_InstancedReference);
				}
				Prop->SetPropertyClass(Class);
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
			break;
		case EAttributeBagPropertyType::SoftObject:
			if (UClass* Class = const_cast<UClass*>(Cast<UClass>(Desc.ValueTypeObject)))
			{
				FSoftObjectProperty* Prop = new FSoftObjectProperty(PropertyScope, Desc.Name, RF_Public);
				if (Class->HasAnyClassFlags(CLASS_DefaultToInstanced))
				{
					Prop->SetPropertyFlags(CPF_InstancedReference);
				}
				Prop->SetPropertyClass(Class);
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
			break;
		case EAttributeBagPropertyType::Class:
			if (UClass* Class = const_cast<UClass*>(Cast<UClass>(Desc.ValueTypeObject)))
			{
				FClassProperty* Prop = new FClassProperty(PropertyScope, Desc.Name, RF_Public);
				Prop->SetMetaClass(Class);
				Prop->PropertyClass = UClass::StaticClass();
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
			break;
		case EAttributeBagPropertyType::SoftClass:
			if (UClass* Class = const_cast<UClass*>(Cast<UClass>(Desc.ValueTypeObject)))
			{
				FSoftClassProperty* Prop = new FSoftClassProperty(PropertyScope, Desc.Name, RF_Public);
				Prop->SetMetaClass(Class);
				Prop->PropertyClass = UClass::StaticClass();
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
			break;
		default:
			ensureMsgf(false, TEXT("Unhandled stype %s"), *UEnum::GetValueAsString(Desc.ValueType));
		}

		return nullptr;
	}

	// Helper functions to get and set property values

	//----------------------------------------------------------------//
	//  Getters
	//----------------------------------------------------------------//

	EAttributeBagResult GetPropertyAsInt64(const FAttributeBagPropertyDesc* Desc, const void* Address, int64& OutValue)
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

		switch (Desc->ValueType)
		{
		case EAttributeBagPropertyType::Bool:
		{
			const FBoolProperty* Property = CastFieldChecked<FBoolProperty>(Desc->CachedProperty);
			OutValue = Property->GetPropertyValue(Address) ? 1 : 0;
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Byte:
		{
			const FByteProperty* Property = CastFieldChecked<FByteProperty>(Desc->CachedProperty);
			OutValue = Property->GetPropertyValue(Address);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Int32:
		{
			const FIntProperty* Property = CastFieldChecked<FIntProperty>(Desc->CachedProperty);
			OutValue = Property->GetPropertyValue(Address);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Int64:
		{
			const FInt64Property* Property = CastFieldChecked<FInt64Property>(Desc->CachedProperty);
			OutValue = Property->GetPropertyValue(Address);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Float:
		{
			const FFloatProperty* Property = CastFieldChecked<FFloatProperty>(Desc->CachedProperty);
			OutValue = (int64)Property->GetPropertyValue(Address);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Double:
		{
			const FDoubleProperty* Property = CastFieldChecked<FDoubleProperty>(Desc->CachedProperty);
			OutValue = (int64)Property->GetPropertyValue(Address);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Enum:
		{
			const FEnumProperty* EnumProperty = CastFieldChecked<FEnumProperty>(Desc->CachedProperty);
			const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
			check(UnderlyingProperty);
			OutValue = UnderlyingProperty->GetSignedIntPropertyValue(Address);
			return EAttributeBagResult::Success;
		}
		default:
			return EAttributeBagResult::TypeMismatch;
		}
	}

	EAttributeBagResult GetPropertyAsDouble(const FAttributeBagPropertyDesc* Desc, const void* Address, double& OutValue)
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

		switch (Desc->ValueType)
		{
		case EAttributeBagPropertyType::Bool:
		{
			const FBoolProperty* Property = CastFieldChecked<FBoolProperty>(Desc->CachedProperty);
			OutValue = Property->GetPropertyValue(Address) ? 1.0 : 0.0;
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Byte:
		{
			const FByteProperty* Property = CastFieldChecked<FByteProperty>(Desc->CachedProperty);
			OutValue = Property->GetPropertyValue(Address);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Int32:
		{
			const FIntProperty* Property = CastFieldChecked<FIntProperty>(Desc->CachedProperty);
			OutValue = Property->GetPropertyValue(Address);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Int64:
		{
			const FInt64Property* Property = CastFieldChecked<FInt64Property>(Desc->CachedProperty);
			OutValue = Property->GetPropertyValue(Address);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Float:
		{
			const FFloatProperty* Property = CastFieldChecked<FFloatProperty>(Desc->CachedProperty);
			OutValue = Property->GetPropertyValue(Address);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Double:
		{
			const FDoubleProperty* Property = CastFieldChecked<FDoubleProperty>(Desc->CachedProperty);
			OutValue = Property->GetPropertyValue(Address);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Enum:
		{
			const FEnumProperty* EnumProperty = CastFieldChecked<FEnumProperty>(Desc->CachedProperty);
			const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
			check(UnderlyingProperty);
			OutValue = UnderlyingProperty->GetSignedIntPropertyValue(Address);
			return EAttributeBagResult::Success;
		}
		default:
			return EAttributeBagResult::TypeMismatch;
		}
	}

	EAttributeBagResult GetPropertyValueAsEnum(const FAttributeBagPropertyDesc* Desc, const UEnum* RequestedEnum, const void* Address, uint8& OutValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EAttributeBagResult::PropertyNotFound;
		}
		if (Desc->ValueType != EAttributeBagPropertyType::Enum)
		{
			return EAttributeBagResult::TypeMismatch;
		}
		if (Address == nullptr)
		{
			return EAttributeBagResult::OutOfBounds;
		}
		if (Desc->ContainerTypes.Num() > 0)
		{
			return EAttributeBagResult::TypeMismatch;
		}

		const FEnumProperty* EnumProperty = CastFieldChecked<FEnumProperty>(Desc->CachedProperty);
		const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
		check(UnderlyingProperty);

		if (RequestedEnum != EnumProperty->GetEnum())
		{
			return EAttributeBagResult::TypeMismatch;
		}

		OutValue = (uint8)UnderlyingProperty->GetUnsignedIntPropertyValue(Address);

		return EAttributeBagResult::Success;
	}

	EAttributeBagResult GetPropertyValueAsStruct(const FAttributeBagPropertyDesc* Desc, const UScriptStruct* RequestedStruct, const void* Address, FStructView& OutValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EAttributeBagResult::PropertyNotFound;
		}
		if (Desc->ValueType != EAttributeBagPropertyType::Struct)
		{
			return EAttributeBagResult::TypeMismatch;
		}
		if (Address == nullptr)
		{
			return EAttributeBagResult::OutOfBounds;
		}
		if (Desc->ContainerTypes.Num() > 0)
		{
			return EAttributeBagResult::TypeMismatch;
		}

		const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Desc->CachedProperty);
		check(StructProperty->Struct);

		if (RequestedStruct != nullptr && CanCastTo(StructProperty->Struct, RequestedStruct) == false)
		{
			return EAttributeBagResult::TypeMismatch;
		}

		OutValue = FStructView(StructProperty->Struct, (uint8*)Address);

		return EAttributeBagResult::Success;
	}

	EAttributeBagResult GetPropertyValueAsObject(const FAttributeBagPropertyDesc* Desc, const UClass* RequestedClass, const void* Address, UObject*& OutValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EAttributeBagResult::PropertyNotFound;
		}
		if (Desc->ValueType != EAttributeBagPropertyType::Object
			&& Desc->ValueType != EAttributeBagPropertyType::SoftObject
			&& Desc->ValueType != EAttributeBagPropertyType::Class
			&& Desc->ValueType != EAttributeBagPropertyType::SoftClass)
		{
			return EAttributeBagResult::TypeMismatch;
		}
		if (Address == nullptr)
		{
			return EAttributeBagResult::OutOfBounds;
		}
		if (Desc->ContainerTypes.Num() > 0)
		{
			return EAttributeBagResult::TypeMismatch;
		}

		const FObjectPropertyBase* ObjectProperty = CastFieldChecked<FObjectPropertyBase>(Desc->CachedProperty);
		check(ObjectProperty->PropertyClass);

		if (RequestedClass != nullptr && CanCastTo(ObjectProperty->PropertyClass, RequestedClass) == false)
		{
			return EAttributeBagResult::TypeMismatch;
		}

		OutValue = ObjectProperty->GetObjectPropertyValue(Address);

		return EAttributeBagResult::Success;
	}

	EAttributeBagResult GetPropertyValueAsSoftPath(const FAttributeBagPropertyDesc* Desc, const void* Address, FSoftObjectPath& OutValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EAttributeBagResult::PropertyNotFound;
		}
		if (Desc->ValueType != EAttributeBagPropertyType::SoftObject
			&& Desc->ValueType != EAttributeBagPropertyType::SoftClass)
		{
			return EAttributeBagResult::TypeMismatch;
		}
		if (Address == nullptr)
		{
			return EAttributeBagResult::OutOfBounds;
		}
		if (Desc->ContainerTypes.Num() > 0)
		{
			return EAttributeBagResult::TypeMismatch;
		}

		const FSoftObjectProperty* SoftObjectProperty = CastFieldChecked<FSoftObjectProperty>(Desc->CachedProperty);
		check(SoftObjectProperty->PropertyClass);

		OutValue = SoftObjectProperty->GetPropertyValue(Address).ToSoftObjectPath();

		return EAttributeBagResult::Success;
	}

	FString GetPropertyValueAsString(const FAttributeBagPropertyDesc* Desc, const void* Address)
	{
		switch (Desc->ValueType)
		{
		case EAttributeBagPropertyType::Bool:
		{
			bool Value;
			EAttributeBagResult Result = GetPropertyValue<bool, FBoolProperty>(Desc, Address, Value);

			if (Result == EAttributeBagResult::Success)
			{
				return Value ? TEXT("True") : TEXT("False");
			}
			else
			{
				return StaticEnum<EAttributeBagResult>()->GetNameStringByValue((uint8)Result);
			}
		}
		case EAttributeBagPropertyType::Byte:
		{
			uint8 Value;
			EAttributeBagResult Result = GetPropertyValue<uint8, FByteProperty>(Desc, Address, Value);

			if (Result == EAttributeBagResult::Success)
			{
				return LexToString(Value);
			}
			else
			{
				return StaticEnum<EAttributeBagResult>()->GetNameStringByValue((uint8)Result);
			}
		}
		case EAttributeBagPropertyType::Int32:
		{
			int32 Value;
			EAttributeBagResult Result = GetPropertyValue<int32, FIntProperty>(Desc, Address, Value);

			if (Result == EAttributeBagResult::Success)
			{
				return LexToString(Value);
			}
			else
			{
				return StaticEnum<EAttributeBagResult>()->GetNameStringByValue((uint8)Result);
			}
		}
		case EAttributeBagPropertyType::Int64:
		{
			int64 Value;
			EAttributeBagResult Result = GetPropertyValue<int64, FIntProperty>(Desc, Address, Value);

			if (Result == EAttributeBagResult::Success)
			{
				return LexToString(Value);
			}
			else
			{
				return StaticEnum<EAttributeBagResult>()->GetNameStringByValue((uint8)Result);
			}
		}
		case EAttributeBagPropertyType::Float:
		{
			float Value;
			EAttributeBagResult Result = GetPropertyValue<float, FFloatProperty>(Desc, Address, Value);

			if (Result == EAttributeBagResult::Success)
			{
				return LexToString(Value);
			}
			else
			{
				return StaticEnum<EAttributeBagResult>()->GetNameStringByValue((uint8)Result);
			}
		}
		case EAttributeBagPropertyType::Double:
		{
			double Value;
			EAttributeBagResult Result = GetPropertyValue<double, FDoubleProperty>(Desc, Address, Value);

			if (Result == EAttributeBagResult::Success)
			{
				return LexToString(Value);
			}
			else
			{
				return StaticEnum<EAttributeBagResult>()->GetNameStringByValue((uint8)Result);
			}
		}
		case EAttributeBagPropertyType::Name:
		{
			FName Value;
			EAttributeBagResult Result = GetPropertyValue<FName, FNameProperty>(Desc, Address, Value);

			if (Result == EAttributeBagResult::Success)
			{
				return Value.ToString();
			}
			else
			{
				return StaticEnum<EAttributeBagResult>()->GetNameStringByValue((uint8)Result);
			}
		}
		case EAttributeBagPropertyType::String:
		{
			FString Value;
			EAttributeBagResult Result = GetPropertyValue<FString, FStrProperty>(Desc, Address, Value);

			if (Result == EAttributeBagResult::Success)
			{
				return Value;
			}
			else
			{
				return StaticEnum<EAttributeBagResult>()->GetNameStringByValue((uint8)Result);
			}
		}
		case EAttributeBagPropertyType::Text:
		{
			FText Value;
			EAttributeBagResult Result = GetPropertyValue<FText, FTextProperty>(Desc, Address, Value);

			if (Result == EAttributeBagResult::Success)
			{
				return Value.ToString();
			}
			else
			{
				return StaticEnum<EAttributeBagResult>()->GetNameStringByValue((uint8)Result);
			}
		}
		case EAttributeBagPropertyType::Enum:
		{
			uint8 Value;

			const UEnum* Enum = CastFieldChecked<FEnumProperty>(Desc->CachedProperty)->GetEnum();
			EAttributeBagResult Result = GetPropertyValueAsEnum(Desc, Enum, Address, Value);

			if (Result == EAttributeBagResult::Success)
			{
				return Enum->GetNameStringByValue(Value);
			}
			else
			{
				return StaticEnum<EAttributeBagResult>()->GetNameStringByValue((uint8)Result);
			}
		}
		case EAttributeBagPropertyType::Struct:
		{
			return TEXT("This is a Struct");
		}
		case EAttributeBagPropertyType::Object:
		{
			return TEXT("This is a Object");
		}
		case EAttributeBagPropertyType::SoftObject:
		{
			return TEXT("This is a SoftObject");
		}
		case EAttributeBagPropertyType::Class:
		{
			return TEXT("This is a Class");
		}
		case EAttributeBagPropertyType::SoftClass:
		{
			return TEXT("This is a SoftClass");
		}
		default:
			ensureMsgf(false, TEXT("Unhandled stype %s"), *UEnum::GetValueAsString(Desc->ValueType));
			return FString();
		}
	}

	//----------------------------------------------------------------//
	//  Setters
	//----------------------------------------------------------------//

	EAttributeBagResult SetPropertyFromInt64(const FAttributeBagPropertyDesc* Desc, void* Address, const int64 InValue)
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

		switch (Desc->ValueType)
		{
		case EAttributeBagPropertyType::Bool:
		{
			const FBoolProperty* Property = CastFieldChecked<FBoolProperty>(Desc->CachedProperty);
			Property->SetPropertyValue(Address, InValue != 0);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Byte:
		{
			const FByteProperty* Property = CastFieldChecked<FByteProperty>(Desc->CachedProperty);
			Property->SetPropertyValue(Address, (uint8)InValue);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Int32:
		{
			const FIntProperty* Property = CastFieldChecked<FIntProperty>(Desc->CachedProperty);
			Property->SetPropertyValue(Address, (int32)InValue);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Int64:
		{
			const FInt64Property* Property = CastFieldChecked<FInt64Property>(Desc->CachedProperty);
			Property->SetPropertyValue(Address, InValue);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Float:
		{
			const FFloatProperty* Property = CastFieldChecked<FFloatProperty>(Desc->CachedProperty);
			Property->SetPropertyValue(Address, (float)InValue);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Double:
		{
			const FDoubleProperty* Property = CastFieldChecked<FDoubleProperty>(Desc->CachedProperty);
			Property->SetPropertyValue(Address, (double)InValue);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Enum:
		{
			const FEnumProperty* EnumProperty = CastFieldChecked<FEnumProperty>(Desc->CachedProperty);
			const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
			check(UnderlyingProperty);
			UnderlyingProperty->SetIntPropertyValue(Address, (uint64)InValue);
			return EAttributeBagResult::Success;
		}
		default:
			return EAttributeBagResult::TypeMismatch;
		}
	}

	EAttributeBagResult SetPropertyFromDouble(const FAttributeBagPropertyDesc* Desc, void* Address, const double InValue)
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

		switch (Desc->ValueType)
		{
		case EAttributeBagPropertyType::Bool:
		{
			const FBoolProperty* Property = CastFieldChecked<FBoolProperty>(Desc->CachedProperty);
			Property->SetPropertyValue(Address, FMath::IsNearlyZero(InValue) ? false : true);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Byte:
		{
			const FByteProperty* Property = CastFieldChecked<FByteProperty>(Desc->CachedProperty);
			Property->SetPropertyValue(Address, FMath::RoundToInt32(InValue));
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Int32:
		{
			const FIntProperty* Property = CastFieldChecked<FIntProperty>(Desc->CachedProperty);
			Property->SetPropertyValue(Address, FMath::RoundToInt32(InValue));
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Int64:
		{
			const FInt64Property* Property = CastFieldChecked<FInt64Property>(Desc->CachedProperty);
			Property->SetPropertyValue(Address, FMath::RoundToInt64(InValue));
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Float:
		{
			const FFloatProperty* Property = CastFieldChecked<FFloatProperty>(Desc->CachedProperty);
			Property->SetPropertyValue(Address, (float)InValue);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Double:
		{
			const FDoubleProperty* Property = CastFieldChecked<FDoubleProperty>(Desc->CachedProperty);
			Property->SetPropertyValue(Address, InValue);
			return EAttributeBagResult::Success;
		}
		case EAttributeBagPropertyType::Enum:
		{
			const FEnumProperty* EnumProperty = CastFieldChecked<FEnumProperty>(Desc->CachedProperty);
			const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
			check(UnderlyingProperty);
			UnderlyingProperty->SetIntPropertyValue(Address, (uint64)InValue);
			return EAttributeBagResult::Success;
		}
		default:
			return EAttributeBagResult::TypeMismatch;
		}
	}

	EAttributeBagResult SetPropertyValueAsEnum(const FAttributeBagPropertyDesc* Desc, void* Address, const uint8 InValue, const UEnum* Enum)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EAttributeBagResult::PropertyNotFound;
		}
		if (Desc->ValueType != EAttributeBagPropertyType::Enum)
		{
			return EAttributeBagResult::TypeMismatch;
		}
		if (Address == nullptr)
		{
			return EAttributeBagResult::OutOfBounds;
		}
		if (Desc->ContainerTypes.Num() > 0)
		{
			return EAttributeBagResult::TypeMismatch;
		}

		const FEnumProperty* EnumProperty = CastFieldChecked<FEnumProperty>(Desc->CachedProperty);
		const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
		check(UnderlyingProperty);

		if (Enum != EnumProperty->GetEnum())
		{
			return EAttributeBagResult::TypeMismatch;
		}

		UnderlyingProperty->SetIntPropertyValue(Address, (uint64)InValue);

		return EAttributeBagResult::Success;
	}

	EAttributeBagResult SetPropertyValueAsStruct(const FAttributeBagPropertyDesc* Desc, void* Address, const FConstStructView InValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EAttributeBagResult::PropertyNotFound;
		}
		if (Desc->ValueType != EAttributeBagPropertyType::Struct)
		{
			return EAttributeBagResult::TypeMismatch;
		}
		if (Address == nullptr)
		{
			return EAttributeBagResult::OutOfBounds;
		}
		if (Desc->ContainerTypes.Num() > 0)
		{
			return EAttributeBagResult::TypeMismatch;
		}

		const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Desc->CachedProperty);
		check(StructProperty->Struct);

		if (InValue.IsValid())
		{
			if (InValue.GetScriptStruct() != StructProperty->Struct)
			{
				return EAttributeBagResult::TypeMismatch;
			}
			StructProperty->Struct->CopyScriptStruct(Address, InValue.GetMemory());
		}
		else
		{
			StructProperty->Struct->ClearScriptStruct(Address);
		}

		return EAttributeBagResult::Success;
	}

	EAttributeBagResult SetPropertyValueAsObject(const FAttributeBagPropertyDesc* Desc, void* Address, UObject* InValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EAttributeBagResult::PropertyNotFound;
		}
		if (Desc->ValueType != EAttributeBagPropertyType::Object
			&& Desc->ValueType != EAttributeBagPropertyType::SoftObject
			&& Desc->ValueType != EAttributeBagPropertyType::Class
			&& Desc->ValueType != EAttributeBagPropertyType::SoftClass)
		{
			return EAttributeBagResult::TypeMismatch;
		}
		if (Address == nullptr)
		{
			return EAttributeBagResult::OutOfBounds;
		}
		if (Desc->ContainerTypes.Num() > 0)
		{
			return EAttributeBagResult::TypeMismatch;
		}

		const FObjectPropertyBase* ObjectProperty = CastFieldChecked<FObjectPropertyBase>(Desc->CachedProperty);
		check(ObjectProperty->PropertyClass);
		check(Desc->ValueTypeObject);

		if (Desc->ValueType == EAttributeBagPropertyType::Object
			|| Desc->ValueType == EAttributeBagPropertyType::SoftObject)
		{
			if (InValue && CanCastTo(InValue->GetClass(), ObjectProperty->PropertyClass) == false)
			{
				return EAttributeBagResult::TypeMismatch;
			}
		}
		else
		{
			const UClass* Class = Cast<UClass>(InValue);
			const UClass* PropClass = nullptr;

			if (const FClassProperty* ClassProperty = CastFieldChecked<FClassProperty>(Desc->CachedProperty))
			{
				PropClass = ClassProperty->MetaClass;
			}
			else if (const FSoftClassProperty* SoftClassProperty = CastFieldChecked<FSoftClassProperty>(Desc->CachedProperty))
			{
				PropClass = SoftClassProperty->MetaClass;
			}

			if (!Class || !PropClass || !Class->IsChildOf(PropClass))
			{
				return EAttributeBagResult::TypeMismatch;
			}
		}

		ObjectProperty->SetObjectPropertyValue(Address, InValue);

		return EAttributeBagResult::Success;
	}

	EAttributeBagResult SetPropertyValueAsSoftPath(const FAttributeBagPropertyDesc* Desc, void* Address, const FSoftObjectPath& InValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EAttributeBagResult::PropertyNotFound;
		}
		if (Desc->ValueType != EAttributeBagPropertyType::SoftObject &&
			Desc->ValueType != EAttributeBagPropertyType::SoftClass)
		{
			return EAttributeBagResult::TypeMismatch;
		}
		if (Address == nullptr)
		{
			return EAttributeBagResult::OutOfBounds;
		}
		if (Desc->ContainerTypes.Num() > 0)
		{
			return EAttributeBagResult::TypeMismatch;
		}

		const FSoftObjectProperty* SoftObjectProperty = CastFieldChecked<FSoftObjectProperty>(Desc->CachedProperty);
		check(SoftObjectProperty->PropertyClass);
		check(Desc->ValueTypeObject);

		SoftObjectProperty->SetPropertyValue(Address, FSoftObjectPtr(InValue));

		return EAttributeBagResult::Success;
	}

	void CopyMatchingValuesByIndex(const FConstStructView Source, FStructView Target)
	{
		if (!Source.IsValid() || !Target.IsValid())
		{
			return;
		}

		const UAttributeBagStruct* SourceBagStruct = Cast<const UAttributeBagStruct>(Source.GetScriptStruct());
		const UAttributeBagStruct* TargetBagStruct = Cast<const UAttributeBagStruct>(Target.GetScriptStruct());

		if (!SourceBagStruct || !TargetBagStruct)
		{
			return;
		}

		// Iterate over source and copy to target if possible. Source is expected to usually have less items.
		for (const FAttributeBagPropertyDesc& SourceDesc : SourceBagStruct->GetPropertyDescs())
		{
			const FAttributeBagPropertyDesc* PotentialTargetDesc = TargetBagStruct->FindPropertyDescByIndex(SourceDesc.Index);
			if (PotentialTargetDesc == nullptr
				|| PotentialTargetDesc->CachedProperty == nullptr
				|| SourceDesc.CachedProperty == nullptr)
			{
				continue;
			}

			const FAttributeBagPropertyDesc& TargetDesc = *PotentialTargetDesc;
			void* TargetAddress = Target.GetMemory() + TargetDesc.CachedProperty->GetOffset_ForInternal();
			const void* SourceAddress = Source.GetMemory() + SourceDesc.CachedProperty->GetOffset_ForInternal();

			if (TargetDesc.CompatibleType(SourceDesc))
			{
				TargetDesc.CachedProperty->CopyCompleteValue(TargetAddress, SourceAddress);
			}
			else if (TargetDesc.ContainerTypes.Num() == 0
				&& SourceDesc.ContainerTypes.Num() == 0)
			{
				if (TargetDesc.IsNumericType() && SourceDesc.IsNumericType())
				{
					// Try to convert numeric types.
					if (TargetDesc.IsNumericFloatType())
					{
						double Value = 0;
						if (GetPropertyAsDouble(&SourceDesc, SourceAddress, Value) == EAttributeBagResult::Success)
						{
							SetPropertyFromDouble(&TargetDesc, TargetAddress, Value);
						}
					}
					else
					{
						int64 Value = 0;
						if (GetPropertyAsInt64(&SourceDesc, SourceAddress, Value) == EAttributeBagResult::Success)
						{
							SetPropertyFromInt64(&TargetDesc, TargetAddress, Value);
						}
					}
				}
				else if ((TargetDesc.IsObjectType() && SourceDesc.IsObjectType())
					|| (TargetDesc.IsClassType() && SourceDesc.IsClassType()))
				{
					// Try convert between compatible objects and classes.
					const UClass* TargetObjectClass = Cast<const UClass>(TargetDesc.ValueTypeObject);
					const UClass* SourceObjectClass = Cast<const UClass>(SourceDesc.ValueTypeObject);
					if (CanCastTo(SourceObjectClass, TargetObjectClass))
					{
						const FObjectPropertyBase* TargetProp = CastFieldChecked<FObjectPropertyBase>(TargetDesc.CachedProperty);
						const FObjectPropertyBase* SourceProp = CastFieldChecked<FObjectPropertyBase>(SourceDesc.CachedProperty);
						TargetProp->SetObjectPropertyValue(TargetAddress, SourceProp->GetObjectPropertyValue(SourceAddress));
					}
				}
			}
		}
	}

	void RemovePropertyByName(TArray<FAttributeBagPropertyDesc>& Descs, const FName PropertyName, const int32 StartIndex)
	{
		// Remove properties which dont have unique name.
		for (int32 Index = StartIndex; Index < Descs.Num(); Index++)
		{
			if (Descs[Index].Name == PropertyName)
			{
				Descs.RemoveAt(Index);
				Index--;
			}
		}
	}

}; // Attribute::StructUtils

//----------------------------------------------------------------//
//  FAttributeBagContainerTypes
//----------------------------------------------------------------//
EAttributeBagContainerType FAttributeBagContainerTypes::PopHead()
{
	EAttributeBagContainerType Head = EAttributeBagContainerType::None;

	if (NumContainers > 0)
	{
		Swap(Head, Types[0]);

		uint8 Index = NumContainers - 1;
		while (Index > 0)
		{
			Types[Index - 1] = Types[Index];
			Types[Index] = EAttributeBagContainerType::None;
			Index--;
		}
		NumContainers--;
	}

	return Head;
}

void FAttributeBagContainerTypes::Serialize(FArchive& Ar)
{
	Ar << NumContainers;
	for (int i = 0; i < NumContainers; ++i)
	{
		Ar << Types[i];
	}
}

bool FAttributeBagContainerTypes::operator == (const FAttributeBagContainerTypes& Other) const
{
	if (NumContainers != Other.NumContainers)
	{
		return false;
	}

	for (int i = 0; i < NumContainers; ++i)
	{
		if (Types[i] != Other.Types[i])
		{
			return false;
		}
	}

	return true;
}

//----------------------------------------------------------------//
//  FAttributeBagPropertyDesc
//----------------------------------------------------------------//
void FAttributeBagPropertyDescMetaData::Serialize(FArchive& Ar)
{
	Ar << Key;
	Ar << Value;
}


FAttributeBagPropertyDesc::FAttributeBagPropertyDesc(const FName InName, const FProperty* InSourceProperty)
	: Name(InName)
{
	ValueType = Attribute::StructUtils::GetValueTypeFromProperty(InSourceProperty);

	ValueTypeObject = Attribute::StructUtils::GetValueTypeObjectFromProperty(InSourceProperty);
	// @todo : improve error handling - if we reach the nested containers limit, the Desc will be invalid (empty container types)
	ContainerTypes = Attribute::StructUtils::GetContainerTypesFromProperty(InSourceProperty);

#if WITH_EDITORONLY_DATA
	if (const TMap<FName, FString>* SourcePropertyMetaData = InSourceProperty->GetMetaDataMap())
	{
		for (const TPair<FName, FString>& MetaDataPair : *SourcePropertyMetaData)
		{
			MetaData.Add({ MetaDataPair.Key, MetaDataPair.Value });
		}
	}
#endif
}

bool FAttributeBagPropertyDesc::IsNumericType() const
{
	switch (ValueType)
	{
	case EAttributeBagPropertyType::Bool: return true;
	case EAttributeBagPropertyType::Byte: return true;
	case EAttributeBagPropertyType::Int32: return true;
	case EAttributeBagPropertyType::Int64: return true;
	case EAttributeBagPropertyType::Float: return true;
	case EAttributeBagPropertyType::Double: return true;
	default: return false;
	}
}

bool FAttributeBagPropertyDesc::IsNumericFloatType() const
{
	switch (ValueType)
	{
	case EAttributeBagPropertyType::Float: return true;
	case EAttributeBagPropertyType::Double: return true;
	default: return false;
	}
}

bool FAttributeBagPropertyDesc::IsObjectType() const
{
	switch (ValueType)
	{
	case EAttributeBagPropertyType::Object: return true;
	case EAttributeBagPropertyType::SoftObject: return true;
	default: return false;
	}
}

bool FAttributeBagPropertyDesc::IsClassType() const
{
	switch (ValueType)
	{
	case EAttributeBagPropertyType::Class: return true;
	case EAttributeBagPropertyType::SoftClass: return true;
	default: return false;
	}
}

bool FAttributeBagPropertyDesc::CompatibleType(const FAttributeBagPropertyDesc& Other) const
{
	// Containers must match
	if (ContainerTypes != Other.ContainerTypes)
	{
		return false;
	}

	// Values must match.
	if (ValueType != Other.ValueType)
	{
		return false;
	}

	// Struct and enum must have same value type class
	if (ValueType == EAttributeBagPropertyType::Enum || ValueType == EAttributeBagPropertyType::Struct)
	{
		return ValueTypeObject == Other.ValueTypeObject;
	}

	// Objects should be castable.
	if (ValueType == EAttributeBagPropertyType::Object)
	{
		const UClass* ObjectClass = Cast<const UClass>(ValueTypeObject);
		const UClass* OtherObjectClass = Cast<const UClass>(Other.ValueTypeObject);
		return Attribute::StructUtils::CanCastTo(OtherObjectClass, ObjectClass);
	}

	return true;
}


//----------------------------------------------------------------//
//  UAttributeBagStruct
//----------------------------------------------------------------//
const UAttributeBagStruct* UAttributeBagStruct::GetOrCreateFromDescs(const TConstArrayView<FAttributeBagPropertyDesc> PropertyDescs)
{
	const uint64 BagHash = Attribute::StructUtils::CalcPropertyDescArrayHash(PropertyDescs);
	const FString ScriptStructName = FString::Printf(TEXT("AttributeBag_%llx"), BagHash);

	if (const UAttributeBagStruct* ExistingBag = FindObject<UAttributeBagStruct>(GetTransientPackage(), *ScriptStructName))
	{
		return ExistingBag;
	}

	UAttributeBagStruct* NewBag = NewObject<UAttributeBagStruct>(GetTransientPackage(), *ScriptStructName, RF_Standalone | RF_Transient);

	NewBag->PropertyDescs = PropertyDescs;

	// Fix missing structs, enums, and objects.
	for (FAttributeBagPropertyDesc& Desc : NewBag->PropertyDescs)
	{
		if (Desc.ValueType == EAttributeBagPropertyType::Struct)
		{
			if (Desc.ValueTypeObject == nullptr || Desc.ValueTypeObject->GetClass()->IsChildOf(UScriptStruct::StaticClass()) == false)
			{
				UE_LOG(LogStateAbilityAttrubuteBag, Warning, TEXT("AttributeBag: Struct property '%s' is missing type."), *Desc.Name.ToString());
				Desc.ValueTypeObject = FAttributeBagMissingStruct::StaticStruct();
			}
		}
		else if (Desc.ValueType == EAttributeBagPropertyType::Enum)
		{
			if (Desc.ValueTypeObject == nullptr || Desc.ValueTypeObject->GetClass()->IsChildOf(UEnum::StaticClass()) == false)
			{
				UE_LOG(LogStateAbilityAttrubuteBag, Warning, TEXT("AttributeBag: Enum property '%s' is missing type."), *Desc.Name.ToString());
				Desc.ValueTypeObject = StaticEnum<EAttributeBagMissingEnum>();
			}
		}
		else if (Desc.ValueType == EAttributeBagPropertyType::Object || Desc.ValueType == EAttributeBagPropertyType::SoftObject)
		{
			if (Desc.ValueTypeObject == nullptr)
			{
				UE_LOG(LogStateAbilityAttrubuteBag, Warning, TEXT("AttributeBag: Object property '%s' is missing type."), *Desc.Name.ToString());
				Desc.ValueTypeObject = UAttributeBagMissingObject::StaticClass();
			}
		}
		else if (Desc.ValueType == EAttributeBagPropertyType::Class || Desc.ValueType == EAttributeBagPropertyType::SoftClass)
		{
			if (Desc.ValueTypeObject == nullptr || Desc.ValueTypeObject->GetClass()->IsChildOf(UClass::StaticClass()) == false)
			{
				UE_LOG(LogStateAbilityAttrubuteBag, Warning, TEXT("AttributeBag: Class property '%s' is missing type."), *Desc.Name.ToString());
				Desc.ValueTypeObject = UAttributeBagMissingObject::StaticClass();
			}
		}
	}

	// Remove properties with same name
	for (int32 Index = 0; Index < NewBag->PropertyDescs.Num() - 1; Index++)
	{
		Attribute::StructUtils::RemovePropertyByName(NewBag->PropertyDescs, NewBag->PropertyDescs[Index].Name, Index + 1);
	}

	// Add properties (AddCppProperty() adds them backwards in the linked list)
	for (int32 DescIndex = NewBag->PropertyDescs.Num() - 1; DescIndex >= 0; DescIndex--)
	{
		FAttributeBagPropertyDesc& Desc = NewBag->PropertyDescs[DescIndex];

		Desc.Index = DescIndex;


		if (FProperty* NewProperty = Attribute::StructUtils::CreatePropertyFromDesc(Desc, NewBag))
		{
#if WITH_EDITORONLY_DATA
			// Add metadata
			for (const FAttributeBagPropertyDescMetaData& PropertyDescMetaData : Desc.MetaData)
			{
				NewProperty->SetMetaData(*PropertyDescMetaData.Key.ToString(), *PropertyDescMetaData.Value);
			}
#endif

			NewProperty->SetPropertyFlags(CPF_Edit);
			NewBag->AddCppProperty(NewProperty);
			Desc.CachedProperty = NewProperty;
		}
	}

	NewBag->Bind();
	NewBag->StaticLink(/*RelinkExistingProperties*/true);

	return NewBag;
}

const UAttributeBagStruct* UAttributeBagStruct::GetOrCreateFromScriptStruct(const UScriptStruct* ScriptStruct)
{
	const uint64 BagHash = Attribute::StructUtils::GetObjectHash(ScriptStruct);
	const FString ScriptStructName = FString::Printf(TEXT("AttributeBag_%llx"), BagHash);

	if (const UAttributeBagStruct* ExistingBag = FindObject<UAttributeBagStruct>(GetTransientPackage(), *ScriptStructName))
	{
		return ExistingBag;
	}

	TArray<FAttributeBagPropertyDesc> PropertyDescs;

	int32 index = 0;
	for (TFieldIterator<FProperty> PropertyIter(ScriptStruct); PropertyIter; ++PropertyIter)
	{
		static const FName NAME_DisplayName(TEXT("DisplayName"));
		FProperty* Property = *PropertyIter;
		FName AttributeName = NAME_None;
#if WITH_EDITOR
		if (const FString* FoundMetaData = Property->FindMetaData(NAME_DisplayName))
		{
			AttributeName = FName(*FoundMetaData);
		}
		else
#endif
		{
			AttributeName = Property->GetFName();
		}
		PropertyDescs.Emplace(FAttributeBagPropertyDesc(AttributeName, Property));
		PropertyDescs.Last().Index = index++;
	}

	return GetOrCreateFromDescs(PropertyDescs);
}

#if WITH_ENGINE && WITH_EDITOR
bool UAttributeBagStruct::ContainsUserDefinedStruct(const UUserDefinedStruct* UserDefinedStruct) const
{
	if (!UserDefinedStruct)
	{
		return false;
	}

	for (const FAttributeBagPropertyDesc& Desc : PropertyDescs)
	{
		if (Desc.ValueType == EAttributeBagPropertyType::Struct)
		{
			if (const UUserDefinedStruct* OwnedUserDefinedStruct = Cast<UUserDefinedStruct>(Desc.ValueTypeObject))
			{
				if (OwnedUserDefinedStruct == UserDefinedStruct
					|| OwnedUserDefinedStruct->PrimaryStruct == UserDefinedStruct
					|| OwnedUserDefinedStruct == UserDefinedStruct->PrimaryStruct
					|| OwnedUserDefinedStruct->PrimaryStruct == UserDefinedStruct->PrimaryStruct)
				{
					return true;
				}
			}
		}
	}
	return false;
}
#endif

void UAttributeBagStruct::DecrementRefCount() const
{
	// Do ref counting based on struct usage.
	// This ensures that if the UAttributeBagStruct is still valid in the C++ destructor of
	// the last instance of the bag.

	UAttributeBagStruct* NonConstThis = const_cast<UAttributeBagStruct*>(this);
	const int32 OldCount = NonConstThis->RefCount.fetch_sub(1, std::memory_order_acq_rel);
	if (OldCount == 1)
	{
		NonConstThis->RemoveFromRoot();
	}
	if (OldCount <= 0)
	{
		UE_LOG(LogStateAbilityAttrubuteBag, Error, TEXT("AttributeBag: DestroyStruct is called when RefCount is %d."), OldCount);
	}
}

void UAttributeBagStruct::IncrementRefCount() const
{
	// Do ref counting based on struct usage.
	// This ensures that if the UAttributeBagStruct is still valid in the C++ destructor of
	// the last instance of the bag.
	UAttributeBagStruct* NonConstThis = const_cast<UAttributeBagStruct*>(this);
	const int32 OldCount = NonConstThis->RefCount.fetch_add(1, std::memory_order_acq_rel);
	if (OldCount == 0)
	{
		NonConstThis->AddToRoot();
	}
}

void UAttributeBagStruct::InitializeStruct(void* Dest, int32 ArrayDim) const
{
	Super::InitializeStruct(Dest, ArrayDim);

	IncrementRefCount();
}

void UAttributeBagStruct::DestroyStruct(void* Dest, int32 ArrayDim) const
{
	Super::DestroyStruct(Dest, ArrayDim);

	DecrementRefCount();
}

void UAttributeBagStruct::FinishDestroy()
{
	const int32 Count = RefCount.load();
	if (Count > 0)
	{
		UE_LOG(LogStateAbilityAttrubuteBag, Error, TEXT("AttributeBag: Expecting RefCount to be zero on destructor, but it is %d."), Count);
	}

	Super::FinishDestroy();
}

const FAttributeBagPropertyDesc* UAttributeBagStruct::FindPropertyDescByIndex(const int32 Index) const
{
	if (PropertyDescs.Num() > Index)
	{
		return &PropertyDescs[Index];
	}

	return nullptr;
}

const FAttributeBagPropertyDesc* UAttributeBagStruct::FindPropertyDescByName(const FName Name) const
{
	return PropertyDescs.FindByPredicate([&Name](const FAttributeBagPropertyDesc& Desc) { return Desc.Name == Name; });
}