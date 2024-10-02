#pragma once

#include "AttributeReflection.h"
#include "AttributeRegistry.h"
#include "Attribute/Attribute.h"

template<typename TOwner, typename TValue>
class TReactiveProperty;

struct FReactivePropertyOperations : public FPropertyOperationsBase
{
	virtual ~FReactivePropertyOperations() {}

	template<typename TOwner>
	void* GetValueAddress_Check(TOwner* InOwner) const
	{
		ensureMsgf(Attribute::TFieldTypeTraits<TOwner>::IsChildOf(GetReactiveModelFieldClass()), TEXT("This reactive property is not valid in TOwner"));

		if (Attribute::TFieldTypeTraits<TOwner>::IsChildOf(GetReactiveModelFieldClass()))
		{
			return GetValueAddress(InOwner);
		}
		else
		{
			return nullptr;
		}
	}

	template<typename TOwner, typename TValue>
	TValue& GetValueRef(void* InOwner) const
	{
		return *((TValue*)GetValueAddress_Check((TOwner*)InOwner));
	}

	template<typename TValue>
	TValue& GetValueRef(void* InOwner) const
	{
		return *((TValue*)GetValueAddress(InOwner));
	}

	// Pointer to a FViewModelPropertyBase
	const FReactivePropertyBase* Property = nullptr;
};

namespace Attribute::Reactive
{
	template <typename TOwner, typename TValue>
	struct TBaseOperation : public FReactivePropertyOperations
	{
		using TDecayedValue = TDecay<TValue>::Type;

		const TReactiveProperty<TOwner, TValue>* GetCastedProperty() const
		{
			return static_cast<const TReactiveProperty<TOwner, TValue>*>(Property);
		}

		virtual void AddFieldClassProperty(UField* TargetFieldClass) const override
		{
			check(TargetFieldClass);

			const TReactiveProperty<TOwner, TValue>* Prop = GetCastedProperty();
			if (Prop->GetFieldByteOffset() > 0)
			{
				TPropertyFactory<TDecay<TValue>::Type>::AddProperty(TargetFieldClass, Prop->GetFieldByteOffset(), Prop->GetName());
			}
		}

		virtual void* GetValueAddress(void* InOwner) const override
		{
			check(InOwner);

			return ((uint8*)InOwner + GetCastedProperty()->GetFieldByteOffset());
		}

		virtual void GetValueCopy(void* InOwner, void* OutValue) const override
		{
			check(InOwner);

			*((TDecayedValue*)OutValue) = GetCastedProperty()->GetValue((TOwner*)InOwner);
		}

		virtual void GetValue_Effect(void* InOwner, void* OutValue) const override
		{
			check(InOwner);

			*((TDecayedValue*)OutValue) = GetCastedProperty()->GetValue_Effect((TOwner*)InOwner);
		}

		virtual void SetValue(void* InOwner, void* InValue) const override
		{
			check(InOwner);

			GetCastedProperty()->SetValue((TOwner*)InOwner, *((TDecayedValue*)InValue));
		}

		virtual bool ContainsObjectReference(bool bIncludeNoFieldProperties) const override
		{
			return TPropertyFactory<TDecay<TValue>::Type>::ContainsObjectReference &&
				(GetCastedProperty()->GetFieldByteOffset() > 0 || bIncludeNoFieldProperties);
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
		virtual UField* GetReactiveModelFieldClass() const override
		{
			return TOwner::StaticClass();
		}
	};

	/* Implementation of ReactiveModelScriptStruct method */
	template <typename TBaseOp, typename TOwner, typename TValue>
	struct TReactiveModelFieldClassOperation<TBaseOp, TOwner, TValue, typename TEnableIf<TValueTypeTraits<TOwner>::IsStruct>::Type> : public TBaseOp
	{
		virtual UField* GetReactiveModelFieldClass() const override
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