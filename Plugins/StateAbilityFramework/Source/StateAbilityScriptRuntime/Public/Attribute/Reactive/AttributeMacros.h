#pragma once

#define REACTIVE_ATTRIBUTE_IMPL_REGISTER(ValueType, Name, GetterPtr, SetterPtr, FieldOffset, GetterVisibility, SetterVisibility) \
    static const TReactiveProperty<ThisFieldClass, PREPROCESSOR_REMOVE_OPTIONAL_PARENS(ValueType)>* Name##Property() \
    { \
        using EVisibility = TReactiveProperty<ThisFieldClass, PREPROCESSOR_REMOVE_OPTIONAL_PARENS(ValueType) >::EAccessorVisibility; \
        using FPropertyType = TReactivePropertyRegistered<ThisFieldClass, PREPROCESSOR_REMOVE_OPTIONAL_PARENS(ValueType), Name##Property>; \
        static constexpr FPropertyType Property(GetterPtr, SetterPtr, FieldOffset, EVisibility::V_##GetterVisibility, EVisibility::V_##SetterVisibility, #Name); \
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
    TReactiveFieldClassInitialization<ThisFieldClass> InitializedFieldClass = TReactiveFieldClassInitialization<ThisFieldClass>(this);

// creates common methods for property with provided Getter and Setter visibility
#define REACTIVE_ATTRIBUTE_IMPL_PROP_2(ValueType, Name, GetterBody, SetterBody, FieldBody, GetterVisibility, SetterVisibility) \
GetterVisibility: \
    typename Attribute::Reactive::TPropertyTypeSelector< PREPROCESSOR_REMOVE_OPTIONAL_PARENS(ValueType) >::GetterType Get##Name() GetterBody \
SetterVisibility: \
    void Set##Name(typename Attribute::Reactive::TPropertyTypeSelector< PREPROCESSOR_REMOVE_OPTIONAL_PARENS(ValueType) >::SetterType InNewValue) \
    SetterBody \
public: \
    REACTIVE_ATTRIBUTE_IMPL_REGISTER(ValueType, Name, &ThisFieldClass::Get##Name, &ThisFieldClass::Set##Name, STRUCT_OFFSET(ThisFieldClass, Name##Field), GetterVisibility, SetterVisibility) \
private: \
    PREPROCESSOR_REMOVE_OPTIONAL_PARENS(FieldBody)

// creates common methods for property with provided Setter visibility and default Getter visibility (public)
#define REACTIVE_ATTRIBUTE_IMPL_PROP_1(ValueType, Name, GetterBody, SetterBody, FieldBody, SetterVisibility) \
    REACTIVE_ATTRIBUTE_IMPL_PROP_2(ValueType, Name, GetterBody, SetterBody, FieldBody, public, SetterVisibility)

// creates common methods for property with default Getter and Setter visibility (public, private)
#define REACTIVE_ATTRIBUTE_IMPL_PROP_0(ValueType, Name, GetterBody, SetterBody, FieldBody) \
    REACTIVE_ATTRIBUTE_IMPL_PROP_2(ValueType, Name, GetterBody, SetterBody, FieldBody, public, private)


// creates body for automatic Getter method
#define REACTIVE_ATTRIBUTE_IMPL_PROP_GETTER(Name) \
    { \
        OnGetAttributeValue(Name##Property()); \
        return Name##Field; \
    }


// creates body for automatic Setter method
#define REACTIVE_ATTRIBUTE_IMPL_PROP_SETTER(Name) \
    { \
        Name##Field = InNewValue; \
        OnSetAttributeValue(Name##Property()); \
    }

#define MARK_ATTRIBUTE_DIRTY(Name) \
    { \
        OnSetAttributeValue(Name##Property()); \
    }

// creates automatic Field
#define REACTIVE_ATTRIBUTE_IMPL_PROP_FIELD(ValueType, Name) \
    typename Attribute::Reactive::TPropertyTypeSelector< PREPROCESSOR_REMOVE_OPTIONAL_PARENS(ValueType) >::FieldType Name##Field;

#define REACTIVE_ATTRIBUTE(ValueType, Name, ... /* GetterVisibility, SetterVisibility */) \
PREPROCESSOR_JOIN(REACTIVE_ATTRIBUTE_IMPL_PROP_, PREPROCESSOR_VA_ARG_COUNT(__VA_ARGS__)) (ValueType, Name, REACTIVE_ATTRIBUTE_IMPL_PROP_GETTER(Name), REACTIVE_ATTRIBUTE_IMPL_PROP_SETTER(Name), (REACTIVE_ATTRIBUTE_IMPL_PROP_FIELD(ValueType, Name)), ##__VA_ARGS__)