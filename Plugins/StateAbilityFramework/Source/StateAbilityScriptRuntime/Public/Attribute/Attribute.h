// Copyright Andrei Sudarikov. All Rights Reserved.

#pragma once

#include "Runtime/Launch/Resources/Version.h"
#include "UObject/TextProperty.h"
#include "UObject/GarbageCollection.h"

namespace Attribute
{
	// checks what kind of type is T (Struct, Class or Interface)
	template <typename T>
	struct TValueTypeTraits
	{
		UE_STATIC_ASSERT_COMPLETE_TYPE(T, "T must be fully defined type, not just forward declared");

		template<typename U> static decltype(U::StaticStruct()) Func1(int);
		template<typename U> static decltype(U::StaticClass()) Func1(int);
		template<typename U> static uint8 Func1(...);
		template <typename U> static uint16 Func2(int U::*);
		template <typename U> static uint8  Func2(...);
		template<typename U> static decltype(&U::_getUObject) Func3(int);
		template<typename U> static uint8 Func3(...);

		using JustT = typename TDecay<T>::Type;

		static const bool IsStruct = std::is_same_v<decltype(Func1<JustT>(0)), UScriptStruct*>;
		static const bool IsClass = std::is_same_v<decltype(Func1<JustT>(0)), UClass*>;
        static const bool IsClassOrStruct = !__is_union(JustT) && sizeof(Func2<JustT>(0)) - 1;
		static const bool IsInterface = !std::is_same_v< char, decltype(Func3<JustT>(0)) > && !TIsDerivedFrom<JustT, UObject>::Value;
	};


    template <typename T, typename = void>
    struct TFieldTypeTraits
    {
        static UField* GetTypeField()
        {
            return nullptr;
        }

        static bool IsChildOf(UField* BaseField)
        {
            return false;
        }
    };

	template <typename T>
	struct TFieldTypeTraits<T, typename TEnableIf<TValueTypeTraits<T>::IsClass>::Type>
	{
        static UField* GetTypeField()
		{
			return T::StaticClass();
		}

		static bool IsChildOf(UField* BaseField)
		{
			UClass* BaseClass = (UClass*)BaseField;
            return T::StaticClass()->IsChildOf(BaseClass);
		}
	};

	template <typename T>
	struct TFieldTypeTraits<T, typename TEnableIf<TValueTypeTraits<T>::IsStruct>::Type>
	{
        static UField* GetTypeField()
		{
			return T::StaticStruct();
		}

		static bool IsChildOf(UField* BaseField)
		{
            UScriptStruct* BaseStruct = (UScriptStruct*)BaseField;
			return T::StaticStruct()->IsChildOf(BaseStruct);
		}
	};


    /**
     * This class creates FProperty objects based on requested TValue.
     * Use UECodeGen_Private::FBytePropertyParams to constructe compiled in properties.
     * There are other ways, please refer to the UAttributeBagStruct: : GetOrCreateFromDescs.
     */
    template <typename TValue, typename = void>
    struct TPropertyFactory
    {
        static constexpr bool IsSupportedByUnreal = false;
        static constexpr bool ContainsObjectReference = false;

        static void AddProperty(FFieldVariant Scope, uint16 FieldOffset, const FName& PropName)
        {
        }
    };

#if ENGINE_MAJOR_VERSION >= 5

#if ENGINE_MINOR_VERSION >= 3
#define COMMON_PROPERTY_PARAMS(PropertyGenType, ...) \
            Scope, { TCHAR_TO_UTF8(*PropName.ToString()), nullptr, EPropertyFlags::CPF_None, UECodeGen_Private::EPropertyGenFlags:: PropertyGenType, EObjectFlags::RF_Transient, nullptr, nullptr, 1, ##__VA_ARGS__ }
#else
#define COMMON_PROPERTY_PARAMS(PropertyGenType, ...) \
            Scope, { TCHAR_TO_UTF8(*PropName.ToString()), nullptr, EPropertyFlags::CPF_None, UECodeGen_Private::EPropertyGenFlags:: PropertyGenType, EObjectFlags::RF_Transient, 1, nullptr, nullptr, ##__VA_ARGS__ }
#endif

#define DECLARE_SIMPLE_PROPERTY_INNER(PropertyType, PropertyGenType) \
        new PropertyType(COMMON_PROPERTY_PARAMS( PropertyGenType, FieldOffset ));

#define DECLARE_WRAPPER_PROPERTY_INNER(PropertyType, PropertyGenType, InnerClass) \
        new PropertyType(COMMON_PROPERTY_PARAMS( PropertyGenType, FieldOffset, &InnerClass ));

#else

#define COMMON_PROPERTY_PARAMS(...)

#define DECLARE_SIMPLE_PROPERTY_INNER(PropertyType, PropertyGenType) \
        new PropertyType(Scope, PropName, EObjectFlags::RF_Transient, FieldOffset, EPropertyFlags::CPF_None);

#define DECLARE_WRAPPER_PROPERTY_INNER(PropertyType, PropertyGenType, InnerClass) \
        new PropertyType(Scope, PropName, EObjectFlags::RF_Transient, FieldOffset, EPropertyFlags::CPF_None, InnerClass());

#endif

#define DECLARE_SIMPLE_PROPERTY(VariableType, PropertyType, PropertyGenType) \
        template <> \
        struct TPropertyFactory<VariableType> \
        { \
            static constexpr bool IsSupportedByUnreal = true; \
            static constexpr bool ContainsObjectReference = false; \
            static void AddProperty(FFieldVariant Scope, uint16 FieldOffset, const FName& PropName) \
            { \
                DECLARE_SIMPLE_PROPERTY_INNER(PropertyType, PropertyGenType); \
            } \
        }

#define DECLARE_WRAPPER_PROPERTY(VariableType, PropertyType, PropertyGenType, ContainsReference, InnerClass) \
        template <typename TValue> \
        struct TPropertyFactory<VariableType> \
        { \
            static constexpr bool IsSupportedByUnreal = true; \
            static constexpr bool ContainsObjectReference = ContainsReference; \
            static void AddProperty(FFieldVariant Scope, uint16 FieldOffset, const FName& PropName) \
            { \
                DECLARE_WRAPPER_PROPERTY_INNER(PropertyType, PropertyGenType, InnerClass); \
            } \
        }

    // We have to use this hack, because Epic "forgot" to export constructor of FTextProperty that we were using. So we create our own property that may contain FText
    // It doesn't behave like proper FTextProperty, but we use it only to calculate sizes of TMap entries, so it doesn't matter
    class FFakeTextProperty : public FTextProperty_Super
    {
    public:
        FFakeTextProperty(FFieldVariant InOwner, const UECodeGen_Private::FTextPropertyParams& Prop)
            : FTextProperty_Super(InOwner, (const UECodeGen_Private::FPropertyParamsBaseWithOffset&)Prop)
        {}
    };

    DECLARE_SIMPLE_PROPERTY(uint8, FByteProperty, Byte);
    DECLARE_SIMPLE_PROPERTY(double, FDoubleProperty, Double);
    DECLARE_SIMPLE_PROPERTY(float, FFloatProperty, Float);
    DECLARE_SIMPLE_PROPERTY(int16, FInt16Property, Int16);
    DECLARE_SIMPLE_PROPERTY(int64, FInt64Property, Int64);
    DECLARE_SIMPLE_PROPERTY(int8, FInt8Property, Int8);
    DECLARE_SIMPLE_PROPERTY(int32, FIntProperty, Int);
    DECLARE_SIMPLE_PROPERTY(FName, FNameProperty, Name);
    DECLARE_SIMPLE_PROPERTY(FString, FStrProperty, Str);
    DECLARE_SIMPLE_PROPERTY(FText, FFakeTextProperty, Text); // should be FTextProperty, but its constructor is not exported in 5.3
    DECLARE_SIMPLE_PROPERTY(uint16, FUInt16Property, UInt16);
    DECLARE_SIMPLE_PROPERTY(uint32, FUInt32Property, UInt32);
    DECLARE_SIMPLE_PROPERTY(uint64, FUInt64Property, UInt64);

    DECLARE_WRAPPER_PROPERTY(TScriptInterface<TValue>, FInterfaceProperty, Interface, true, TValue::UClassType::StaticClass);
    DECLARE_WRAPPER_PROPERTY(TLazyObjectPtr<TValue>, FLazyObjectProperty, LazyObject, false, TValue::StaticClass);
    DECLARE_WRAPPER_PROPERTY(TSoftClassPtr<TValue>, FSoftClassProperty, SoftClass, false, TValue::StaticClass);
    DECLARE_WRAPPER_PROPERTY(TSoftObjectPtr<TValue>, FSoftObjectProperty, SoftObject, false, TValue::StaticClass);
    DECLARE_WRAPPER_PROPERTY(TWeakObjectPtr<TValue>, FWeakObjectProperty, WeakObject, false, TValue::StaticClass);

    /* bool property. It does not fit into DECLARE_SIMPLE_PROPERTY */
    template <>
    struct TPropertyFactory<bool>
    {
        static constexpr bool IsSupportedByUnreal = true;
        static constexpr bool ContainsObjectReference = false;
        static void AddProperty(FFieldVariant Scope, uint16 FieldOffset, const FName& PropName)
        {
#if ENGINE_MAJOR_VERSION >= 5
            new FBoolProperty(COMMON_PROPERTY_PARAMS(Bool, sizeof(bool), 0, nullptr));
#else
            new FBoolProperty(Scope, PropName, EObjectFlags::RF_NoFlags, FieldOffset, EPropertyFlags::CPF_None, 1, sizeof(bool), true);
#endif
        }
    };

    /* UObject pointer */
    template <typename TValue>
    struct TPropertyFactory<TValue*, typename TEnableIf<TValueTypeTraits<TValue>::IsClass>::Type>
    {
        static constexpr bool IsSupportedByUnreal = true;
        static constexpr bool ContainsObjectReference = true;

        static void AddProperty(FFieldVariant Scope, uint16 FieldOffset, const FName& PropName)
        {
            DECLARE_WRAPPER_PROPERTY_INNER(FObjectProperty, Object, TValue::StaticClass);
        }
    };

#if ENGINE_MAJOR_VERSION >= 5
    /* TObjectPtr<> pointer */
    template <typename TValue>
    struct TPropertyFactory<TObjectPtr<TValue>, typename TEnableIf<TValueTypeTraits<TValue>::IsClass>::Type>
    {
        static constexpr bool IsSupportedByUnreal = true;
        static constexpr bool ContainsObjectReference = true;

        static void AddProperty(FFieldVariant Scope, uint16 FieldOffset, const FName& PropName)
        {
            DECLARE_WRAPPER_PROPERTY_INNER(FObjectProperty, Object, TValue::StaticClass);
        }
    };
#endif

    /* UStruct value */
    template <typename TValue>
    struct TPropertyFactory<TValue, typename TEnableIf<TValueTypeTraits<TValue>::IsStruct>::Type>
    {
        static constexpr bool IsSupportedByUnreal = true;
        static constexpr bool ContainsObjectReference = true; // assume true, we cannot easily check this in compile time

        static void AddProperty(FFieldVariant Scope, uint16 FieldOffset, const FName& PropName)
        {
            DECLARE_WRAPPER_PROPERTY_INNER(FStructProperty, Struct, TValue::StaticStruct);
        }
    };

    /* TArray<> */
    template <typename TValue>
    struct TPropertyFactory<TArray<TValue>>
    {
        static constexpr bool IsSupportedByUnreal = true;
        static constexpr bool ContainsObjectReference = TPropertyFactory<TValue>::ContainsObjectReference;

        static void AddProperty(FFieldVariant Scope, uint16 FieldOffset, const FName& PropName)
        {
#if ENGINE_MAJOR_VERSION >= 5
            auto Prop = new FArrayProperty(COMMON_PROPERTY_PARAMS(Array, FieldOffset, EArrayPropertyFlags::None));
#else
            auto Prop = new FArrayProperty(Scope, PropName, EObjectFlags::RF_NoFlags, FieldOffset, EPropertyFlags::CPF_None, EArrayPropertyFlags::None);
#endif
            TPropertyFactory<TValue>::AddProperty(Prop, FieldOffset, FName(PropName.ToString() + TEXT("_ArrayValue")));
        }
    };

    /* TSet<> */
    template <typename TValue>
    struct TPropertyFactory<TSet<TValue>>
    {
        static constexpr bool IsSupportedByUnreal = true;
        static constexpr bool ContainsObjectReference = TPropertyFactory<TValue>::ContainsObjectReference;

        static void AddProperty(FFieldVariant Scope, uint16 FieldOffset, const FName& PropName)
        {
#if ENGINE_MAJOR_VERSION >= 5
            auto Prop = new FSetProperty(COMMON_PROPERTY_PARAMS(Set, FieldOffset));
#else
            auto Prop = new FSetProperty(Scope, PropName, EObjectFlags::RF_NoFlags, FieldOffset, EPropertyFlags::CPF_None);
#endif
            TPropertyFactory<TValue>::AddProperty(Prop, FieldOffset, FName(PropName.ToString() + TEXT("_SetValue")));
        }
    };

    /* TMap<,> */
    template <typename TKey, typename TValue>
    struct TPropertyFactory<TMap<TKey, TValue>>
    {
        static constexpr bool IsSupportedByUnreal = true;
        static constexpr bool ContainsObjectReference = TPropertyFactory<TKey>::ContainsObjectReference || TPropertyFactory<TValue>::ContainsObjectReference;

        static void AddProperty(FFieldVariant Scope, uint16 FieldOffset, const FName& PropName)
        {
            // if either TKey or TValue has references, then both of them MUST be supported
            // 
            // if your code triggers this assert, and you are absolutely sure that you need exactly this type,
            // then change your property to have _NF suffix and provide field manually
            static_assert(!ContainsObjectReference || TPropertyFactory<TKey>::IsSupportedByUnreal, "Unsupported Key Type");
            static_assert(!ContainsObjectReference || TPropertyFactory<TValue>::IsSupportedByUnreal, "Unsupported Value Type");

#if ENGINE_MAJOR_VERSION >= 5
            auto Prop = new FMapProperty(COMMON_PROPERTY_PARAMS(Map, FieldOffset, EMapPropertyFlags::None));
#else
            auto Prop = new FMapProperty(Scope, PropName, EObjectFlags::RF_NoFlags, FieldOffset, EPropertyFlags::CPF_None, EMapPropertyFlags::None);
#endif
            TPropertyFactory<TKey>::AddProperty(Prop, 0, FName(PropName.ToString() + TEXT("_MapKey")));
            TPropertyFactory<TValue>::AddProperty(Prop, 1, FName(PropName.ToString() + TEXT("_MapValue")));
        }
    };

#undef DECLARE_SIMPLE_PROPERTY
#undef DECLARE_SIMPLE_PROPERTY_INNER
#undef DECLARE_WRAPPER_PROPERTY
#undef DECLARE_WRAPPER_PROPERTY_INNER
#undef COMMON_PROPERTY_PARAMS

}
