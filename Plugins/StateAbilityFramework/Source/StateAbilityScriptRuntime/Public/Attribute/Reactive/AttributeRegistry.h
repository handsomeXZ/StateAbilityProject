#pragma once

#include "AttributeTraits.h"
#include "AttributeReactiveOps.h"

namespace Attribute::Reactive
{
	template<typename TOwner, typename = void>
	struct FReactiveRegistryHelper_Field
	{
		static uint8 RegisterFieldClass();
		static const FReactivePropertyOperations* FindPropertyOperations(const FName& InPropertyName);
	};

	template<typename TOwner, typename TValue, typename = void>
	struct FReactiveRegistryHelper_Property
	{
		static uint8 RegisterProperty(typename TReactiveProperty<TOwner, TValue>::FPropertyGetterPtr PropertyGetterPtr);
	};

	class STATEABILITYSCRIPTRUNTIME_API FReactiveRegistry
	{
	public:
		template<typename TOwner, typename TValue>
		friend struct FReactiveRegistryHelper_Field;
		template<typename TOwner, typename TValue, typename Enable>
		friend struct FReactiveRegistryHelper_Property;

		using FFieldClassGetterPtr = UField* (*)();
		using FAttributeReflection = FReactivePropertyReflection<FReactivePropertyOperations>;

		struct FUnprocessedPropertyEntry
		{
			FFieldClassGetterPtr GetFieldClass;
			FAttributeReflection Reflection;
		};

		static void ProcessPendingRegistrations();

		static const FReactivePropertyOperations* FindPropertyOperations(UClass* Class, const FName& InPropertyName);
		static const FReactivePropertyOperations* FindPropertyOperations(UScriptStruct* Struct, const FName& InPropertyName);
		static TConstArrayView<FAttributeReflection> GetPropertyOpsList(UClass* Class);
		static TConstArrayView<FAttributeReflection> GetPropertyOpsList(UScriptStruct* Struct);
	protected:
		template<typename TOwner, typename TValue>
		static uint8 RegisterProperty_Internal(typename TReactiveProperty<TOwner, TValue>::FPropertyGetterPtr PropertyGetterPtr, FUnprocessedPropertyEntry& Entry);

		// Since 5.3, TokenStream has been changed to Schema
		static void AssembleReferenceSchema(UClass* Class);
		static void AssembleReferenceSchema(UScriptStruct* Struct);

		// List of properties that were not yet added to lookup table
		static TArray<FUnprocessedPropertyEntry>& GetUnprocessedClassProperties();
		static TArray<FUnprocessedPropertyEntry>& GetUnprocessedStructProperties();
		static TSet<FFieldClassGetterPtr>& GetUnprocessedClass();
		static TSet<FFieldClassGetterPtr>& GetUnprocessedStruct();

		static void RegisterClassProperties(TArray<UClass*>& LocalRegisteredClass, TSet<UClass*>& ProcessedClassSet);
		static void RegisterStructProperties(TArray<UScriptStruct*>& LocalRegisteredStruct, TSet<UScriptStruct*>& ProcessedStructSet);
	private:
		// Map of <ReactiveModelFieldClass, Properties>
		static TMap<UClass*, TArray<FAttributeReflection>> ClassProperties;
		static TMap<UScriptStruct*, TArray<FAttributeReflection>> StructProperties;
	};

	template<typename TOwner, typename TValue>
	uint8 FReactiveRegistry::RegisterProperty_Internal(typename TReactiveProperty<TOwner, TValue>::FPropertyGetterPtr PropertyGetterPtr, FUnprocessedPropertyEntry& Entry)
	{
		using TDecayedValueType = typename TDecay<TValue>::Type;

		using FBaseOps = TBaseOperation<TOwner, TValue>;
		using FFinalReactiveOps = TReactiveModelFieldClassOperation<FBaseOps, TOwner, TValue>;
		using FEffectiveOpsType = TReactivePropertyOperations<FFinalReactiveOps>;

		static_assert(sizeof(FReactivePropertyOperations) == sizeof(FEffectiveOpsType), "Generated Operations type cannot fit into OpsBuffer");

		FReactiveRegistry::FAttributeReflection& Reflection = Entry.Reflection;
		const TReactiveProperty<TOwner, TValue>* Prop = PropertyGetterPtr();

		// The 0 ID is not used for property binding.
		Prop->AttributeID = FReactiveModelTypeTraitsBase<TOwner>::AttributeCount;

		new (Reflection.GetOperations()) FEffectiveOpsType(Prop);

		Reflection.Flags.IsOptional = TReactivePropertyTypeTraits<TDecayedValueType>::WithOptional;
		Reflection.Flags.HasPublicGetter = Prop->HasPublicGetter();
		Reflection.Flags.HasPublicSetter = Prop->HasPublicSetter();
		return 1;
	}

	//////////////////////////////////////////////////////////////////////////

	template<typename TOwner, typename TValue>
	struct FReactiveRegistryHelper_Property<TOwner, TValue, typename TEnableIf<TValueTypeTraits<TOwner>::IsClass>::Type>
	{
		static uint8 RegisterProperty(typename TReactiveProperty<TOwner, TValue>::FPropertyGetterPtr PropertyGetterPtr)
		{
			++FReactiveModelTypeTraitsBase<TOwner>::AttributeCount;
			++FReactiveModelTypeTraitsBase<TOwner>::AttributeCountTotal;

			FReactiveRegistry::FUnprocessedPropertyEntry& Entry = FReactiveRegistry::GetUnprocessedClassProperties().AddDefaulted_GetRef();
			Entry.GetFieldClass = (FReactiveRegistry::FFieldClassGetterPtr) &TOwner::StaticClass;

			return FReactiveRegistry::RegisterProperty_Internal<TOwner, TValue>(PropertyGetterPtr, Entry);
		}
	};

	template<typename TOwner, typename TValue>
	struct FReactiveRegistryHelper_Property<TOwner, TValue, typename TEnableIf<TValueTypeTraits<TOwner>::IsStruct>::Type>
	{
		static uint8 RegisterProperty(typename TReactiveProperty<TOwner, TValue>::FPropertyGetterPtr PropertyGetterPtr)
		{
			++FReactiveModelTypeTraitsBase<TOwner>::AttributeCount;
			++FReactiveModelTypeTraitsBase<TOwner>::AttributeCountTotal;

			FReactiveRegistry::FUnprocessedPropertyEntry& Entry = FReactiveRegistry::GetUnprocessedStructProperties().AddDefaulted_GetRef();
			Entry.GetFieldClass = (FReactiveRegistry::FFieldClassGetterPtr) &TOwner::StaticStruct;

			return FReactiveRegistry::RegisterProperty_Internal<TOwner, TValue>(PropertyGetterPtr, Entry);
		}
	};

	template<typename TOwner>
	struct FReactiveRegistryHelper_Field<TOwner, typename TEnableIf<TValueTypeTraits<TOwner>::IsClass>::Type>
	{
		static uint8 RegisterFieldClass()
		{
			FReactiveModelTypeTraitsBase<TOwner>::AttributeCountTotal += FReactiveModelTypeTraitsBase<TOwner::Super>::AttributeCount;

			FReactiveRegistry::GetUnprocessedClass().Add((FReactiveRegistry::FFieldClassGetterPtr) &TOwner::StaticClass);

			return 1;
		}
		static const FReactivePropertyOperations* FindPropertyOperations(const FName& InPropertyName)
		{
			return FReactiveRegistry::FindPropertyOperations(TOwner::StaticClass(), InPropertyName);
		}
	};

	template<typename TOwner>
	struct FReactiveRegistryHelper_Field<TOwner, typename TEnableIf<TValueTypeTraits<TOwner>::IsStruct>::Type>
	{
		static uint8 RegisterFieldClass()
		{
			FReactiveModelTypeTraitsBase<TOwner>::AttributeCountTotal += FReactiveModelTypeTraitsBase<TOwner::Super>::AttributeCount;

			FReactiveRegistry::GetUnprocessedStruct().Add((FReactiveRegistry::FFieldClassGetterPtr) &TOwner::StaticStruct);

			return 1;
		}
		static const FReactivePropertyOperations* FindPropertyOperations(const FName& InPropertyName)
		{
			return FReactiveRegistry::FindPropertyOperations(TOwner::StaticStruct(), InPropertyName);
		}
	};
}