#pragma once

// Calls Macro usign Args. Args are expected to be inside parantheses, 
// Helps code editors understand macros properly.
#define INDIRECT_CALL(Macro, Args) Macro Args

/**
 * REACTIVE_NARGS - returns number of arguments passed.
 * Instead of PREPROCESSOR_APPEND_VA_ARG_COUNT,
 * implemented it by self because the macros provided by UE are too complex and cause the code editor to not provide good code hints. 
 */
#if defined(_MSC_VER) && (!defined(_MSVC_TRADITIONAL) || _MSVC_TRADITIONAL)
// Works with the MSVC compiler
#define REACTIVE_NARGS(...) REACTIVE_NARGS_MSVC(unused, ##__VA_ARGS__, 4, 3, 2, 1, 0)
#define REACTIVE_NARGS_MSVC(_1, _2, _3, _4, _5, N, ...) N
#else
// Works with other compilers
#define REACTIVE_NARGS(...) REACTIVE_NARGS_OTHER(0, ##__VA_ARGS__, 5, 4, 3, 2, 1, 0)
#define REACTIVE_NARGS_OTHER(_0, _1, _2, _3, _4, _5, N, ...) N
#endif

#define REACTIVE_ATTRIBUTE_IMPL_REGISTER(ValueType, Name, GetterPtr, SetterPtr, EffectGetterPtr, FieldOffset, GetterVisibility, SetterVisibility) \
    static const TReactiveProperty<ThisFieldClass, PREPROCESSOR_REMOVE_OPTIONAL_PARENS(ValueType)>* Name##Property() \
    { \
        using EVisibility = TReactiveProperty<ThisFieldClass, PREPROCESSOR_REMOVE_OPTIONAL_PARENS(ValueType) >::EAccessorVisibility; \
        using FPropertyType = TReactivePropertyRegistered<ThisFieldClass, PREPROCESSOR_REMOVE_OPTIONAL_PARENS(ValueType), Name##Property>; \
        static constexpr FPropertyType Property(GetterPtr, SetterPtr, EffectGetterPtr, FieldOffset, EVisibility::V_##GetterVisibility, EVisibility::V_##SetterVisibility, #Name); \
        /* Avoid compiler optimizations  */ \
        const uint8& Register = FPropertyType::Registered; \
        return &Property; \
    }

#define REACTIVE_BODY(TOwner) using ThisFieldClass = TOwner; \
    static void RegisterFieldClass() \
    { \
		static constexpr TReactiveFieldClassRegistered<TOwner> RegisteredFieldClass; \
		const uint8& Register = TReactiveFieldClassRegistered<TOwner>::Registered; \
    } \
    TReactiveFieldClassInitialization<ThisFieldClass> InitializedFieldClass = TReactiveFieldClassInitialization<ThisFieldClass>(this)

// creates common methods for property with provided Getter and Setter visibility
#define REACTIVE_ATTRIBUTE_IMPL_PROP_2(ValueType, Name, GetterVisibility, SetterVisibility) \
GetterVisibility: \
    typename Attribute::Reactive::TPropertyTypeSelector< PREPROCESSOR_REMOVE_OPTIONAL_PARENS(ValueType) >::GetterType Get##Name() REACTIVE_ATTRIBUTE_IMPL_PROP_GETTER(Name) \
    typename const Attribute::Reactive::TPropertyTypeSelector< PREPROCESSOR_REMOVE_OPTIONAL_PARENS(ValueType) >::GetterType Get##Name() const REACTIVE_ATTRIBUTE_IMPL_PROP_GETTER(Name) \
SetterVisibility: \
    void Set##Name(typename Attribute::Reactive::TPropertyTypeSelector< PREPROCESSOR_REMOVE_OPTIONAL_PARENS(ValueType) >::SetterType InNewValue) REACTIVE_ATTRIBUTE_IMPL_PROP_SETTER(Name) \
public: \
    typename Attribute::Reactive::TPropertyTypeSelector< PREPROCESSOR_REMOVE_OPTIONAL_PARENS(ValueType) >::GetterType Get##Name##_Effect() REACTIVE_ATTRIBUTE_IMPL_PROP_GETTER_EFFECT(Name) \
    typename const Attribute::Reactive::TPropertyTypeSelector< PREPROCESSOR_REMOVE_OPTIONAL_PARENS(ValueType) >::GetterType Get##Name##_Effect() const REACTIVE_ATTRIBUTE_IMPL_PROP_GETTER_EFFECT(Name) \
    REACTIVE_ATTRIBUTE_IMPL_REGISTER(ValueType, Name, &ThisFieldClass::Get##Name, &ThisFieldClass::Set##Name, &ThisFieldClass::Get##Name##_Effect, STRUCT_OFFSET(ThisFieldClass, Name##Field), GetterVisibility, SetterVisibility) \
private: \
    REACTIVE_ATTRIBUTE_IMPL_PROP_FIELD(ValueType, Name)

// creates common methods for property with provided Setter visibility and default Getter visibility (public)
#define REACTIVE_ATTRIBUTE_IMPL_PROP_1(ValueType, Name, SetterVisibility) \
    REACTIVE_ATTRIBUTE_IMPL_PROP_2(ValueType, Name, public, SetterVisibility)

// creates common methods for property with default Getter and Setter visibility (public, public)
#define REACTIVE_ATTRIBUTE_IMPL_PROP_0(ValueType, Name) \
    REACTIVE_ATTRIBUTE_IMPL_PROP_2(ValueType, Name, public, public)


// creates body for automatic Getter method
#define REACTIVE_ATTRIBUTE_IMPL_PROP_GETTER(Name) \
    { \
        OnGetAttributeValue(Name##Property()); \
        return Name##Field; \
    }


// creates body for automatic Setter method
#define REACTIVE_ATTRIBUTE_IMPL_PROP_SETTER(Name) \
    { \
        if(SetValue(Name##Field, InNewValue)) \
        { \
            OnSetAttributeValue(Name##Property()); \
        } \
    }

#define MARK_ATTRIBUTE_DIRTY(Name) \
    { \
        OnSetAttributeValue(Name##Property()); \
    }

#define MARK_MODEL_ATTRIBUTE_DIRTY(ModelPtr, ModelType, Name) \
    ModelPtr->MarkDirty(ModelType::Name##Property()); 

#define REACTIVE_ATTRIBUTE_IMPL_PROP_GETTER_EFFECT(Name) \
    { \
		OnGetAttributeValue_Effect(Name##Property()); \
		return Name##Field; \
	}

// creates automatic Field
#define REACTIVE_ATTRIBUTE_IMPL_PROP_FIELD(ValueType, Name) \
    mutable typename Attribute::Reactive::TPropertyTypeSelector< PREPROCESSOR_REMOVE_OPTIONAL_PARENS(ValueType) >::FieldType Name##Field;

/*
 * Creates Reactive property with auto getter and setter
 * Pass optional Getter and Setter visibility via VA_ARGS
 */
#define REACTIVE_ATTRIBUTE(ValueType, Name, ... /* GetterVisibility, SetterVisibility */) \
    INDIRECT_CALL( PREPROCESSOR_JOIN(REACTIVE_ATTRIBUTE_IMPL_PROP_, REACTIVE_NARGS(__VA_ARGS__)), (ValueType, Name, ##__VA_ARGS__) )