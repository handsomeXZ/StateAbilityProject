
#pragma once


#include "Templates/ChooseClass.h"
#include "Templates/IsArithmetic.h"
#include "Attribute/Attribute.h"

/*
 * Base struct that describe traits of a type if it is used as value of Reactive Property
 * This struct uses same idea as TStructOpsTypeTraitsBase2
 */
template <typename TPropertyValueType>
struct TReactivePropertyTypeTraitsBase
{
    enum
    {
        WithSetterArgumentByValue   = false,            // Forces generated setter method to accept argument By Value rather than By Reference
        WithSetterComparison        = true,             // Defines whether we need to perform comparison between existing value and new value in property Setter
		WithOptional				= false,
    };
};

/*
 * Specialization of TReactivePropertyTypeTraitsBase for each type
 * If you need to override some settings, make specialization of this struct for your custom type
 */
template <typename TPropertyValueType>
struct TReactivePropertyTypeTraits : public TReactivePropertyTypeTraitsBase<TPropertyValueType>
{
	enum
	{
		WithOptional				= false,
	};
};

template <typename TPropertyValueType>
struct TReactivePropertyTypeTraits<TOptional<TPropertyValueType>> : public TReactivePropertyTypeTraitsBase<TPropertyValueType>
{
	enum
	{
		WithOptional				= true,
	};
};

//////////////////////////////////////////////////////////////////////////

template<typename T>
struct FReactiveModelTypeTraitsBase
{
	static inline int32 AttributeCount = 0;
};


//////////////////////////////////////////////////////////////////////////

namespace Attribute::Reactive
{
	// helper used to select by-value setter for arithmetic types
	template <typename T>
	using TValueOrRef = TChooseClass< TIsArithmetic<T>::Value || TIsEnum<T>::Value || TReactivePropertyTypeTraits<T>::WithSetterArgumentByValue, T, const T& >;

	template <typename T>
	struct TPropertyTypeSelector
	{
		using GetterType = T;
		using SetterType = typename TValueOrRef<T>::Result;
		using FieldType = T;
	};

	template <typename T>
	struct TPropertyTypeSelector<const T&>
	{
		using GetterType = const T&;
		using SetterType = typename TValueOrRef<T>::Result;
		using FieldType = T;
	};

	template <typename T>
	struct TPropertyTypeSelector<T*>
	{
		using GetterType = T*;
		using SetterType = T*;
		using FieldType = T*;
	};
}