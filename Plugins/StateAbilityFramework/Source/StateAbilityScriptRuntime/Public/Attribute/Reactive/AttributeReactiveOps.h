#pragma once

#include "AttributeReflection.h"
#include "AttributeRegistry.h"
#include "Attribute/Attribute.h"

template<typename TOwner, typename TValue>
class TReactiveProperty;

namespace Attribute::Reactive
{
	template <typename TOwner, typename TValue>
	struct TBaseOperation : public FReactivePropertyOperations
	{
		using TDecayedValue = typename TDecay<TValue>::Type;

		const TReactiveProperty<TOwner, TValue>* GetCastedProperty() const
		{
			return static_cast<const TReactiveProperty<TOwner, TValue>*>(Property);
		}
	};

	/* Implementation of GetValue method */
	template <typename TBaseOp, typename TOwner, typename TValue, bool bOptional>
	struct TGetValueOperation;

	template <typename TBaseOp, typename TOwner, typename TValue>
	struct TGetValueOperation<TBaseOp, TOwner, TValue, true> : public TBaseOp
	{
		void GetValue(void* InOwner, void* OutValue, bool& OutHasValue) const override
		{
			check(InOwner);
			check(OutValue);

			TValue Value = this->GetCastedProperty()->GetValue((TOwner*)InOwner);
			OutHasValue = Value.IsSet();
			if (OutHasValue)
			{
				*((typename TBaseOp::TDecayedValue::ElementType*)OutValue) = Value.GetValue();
			}
		}
	};

	template <typename TBaseOp, typename TOwner, typename TValue>
	struct TGetValueOperation<TBaseOp, TOwner, TValue, false> : public TBaseOp
	{
		void GetValue(void* InOwner, void* OutValue, bool& OutHasValue) const override
		{
			check(InOwner);
			check(OutValue);

			*((typename TBaseOp::TDecayedValue*)OutValue) = this->GetCastedProperty()->GetValue((TOwner*)InOwner);
			OutHasValue = true;
		}
	};

	/* Implementation of SetValue method */
	template <typename TBaseOp, typename TOwner, typename TValue, bool bOptional>
	struct TSetValueOperation;

	template <typename TBaseOp, typename TOwner, typename TValue>
	struct TSetValueOperation<TBaseOp, TOwner, TValue, false> : public TBaseOp
	{
		void SetValue(void* InOwner, void* InValue, bool InHasValue = true) const override
		{
			check(InOwner);
			check(InValue);

			this->GetCastedProperty()->SetValue((TOwner*)InOwner, *((typename TBaseOp::TDecayedValue*)InValue));
		}
	};

	template <typename TBaseOp, typename TOwner, typename TValue>
	struct TSetValueOperation<TBaseOp, TOwner, TValue, true> : public TBaseOp
	{
		void SetValue(void* InOwner, void* InValue, bool InHasValue = true) const override
		{
			check(InOwner);
			check(InValue);

			if (InHasValue)
			{
				this->GetCastedProperty()->SetValue((TOwner*)InOwner, typename TBaseOp::TDecayedValue(*(typename TBaseOp::TDecayedValue::ElementType*)InValue));
			}
			else
			{
				this->GetCastedProperty()->SetValue((TOwner*)InOwner, typename TBaseOp::TDecayedValue());
			}
		}
	};


	/* Implementation of AddFieldClassProperty method and ContainsObjectReference method */
	template <typename TBaseOp, typename TOwner, typename TValue>
	struct TAddFieldClassPropertyOperation : public TBaseOp
	{
		void AddFieldClassProperty(UField* TargetFieldClass) const override
		{
			check(TargetFieldClass);

			const TReactiveProperty<TOwner, TValue>* Prop = this->GetCastedProperty();
			if (Prop->GetFieldOffset() > 0)
			{
				TPropertyFactory<typename TDecay<TValue>::Type>::AddProperty(TargetFieldClass, Prop->GetFieldOffset(), Prop->GetName());
			}
		}

		bool ContainsObjectReference(bool bIncludeNoFieldProperties) const override
		{
			return TPropertyFactory<typename TDecay<TValue>::Type>::ContainsObjectReference &&
				(this->GetCastedProperty()->GetFieldOffset() > 0 || bIncludeNoFieldProperties);
		}
	};

	template <typename TBaseOp, typename TOwner, typename TValue, typename = void>
	struct TReactiveModelFieldClassOperation : public TBaseOp
	{

	};

	/* Implementation of ReactiveModelClass method */
	template <typename TBaseOp, typename TOwner, typename TValue>
	struct TReactiveModelFieldClassOperation<TBaseOp, TOwner, TValue, typename TEnableIf<TValueTypeTraits<TOwner>::IsClass>::Type> : public TBaseOp
	{
		UField* GetReactiveModelFieldClass() const override
		{
			return TOwner::StaticClass();
		}
	};

	/* Implementation of ReactiveModelScriptStruct method */
	template <typename TBaseOp, typename TOwner, typename TValue>
	struct TReactiveModelFieldClassOperation<TBaseOp, TOwner, TValue, typename TEnableIf<TValueTypeTraits<TOwner>::IsStruct>::Type> : public TBaseOp
	{
		UField* GetReactiveModelFieldClass() const override
		{
			return TOwner::StaticStruct();
		}
	};

	template <typename TBaseOp>
	struct TReactivePropertyOperations : public TBaseOp
	{
		TReactivePropertyOperations(const FReactivePropertyBase* Prop)
		{
			check(Prop);
			this->Property = Prop;
		}
	};

}