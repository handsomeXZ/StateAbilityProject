

#pragma once

#include "Delegates/DelegateSignatureImpl.inl"

#include "Attribute/Reactive/AttributeTraits.h"
#include "Attribute/Reactive/AttributeRegistry.h"
#include "Attribute/Reactive/AttributeMacros.h"
#include "Attribute/Reactive/AttributeBinding.h"

struct STATEABILITYSCRIPTRUNTIME_API FReactiveModelBase
{
	template<typename TOwner>
	friend struct TReactiveFieldClassInitialization;

	FReactiveModelBase()
	{
		// By default, one BindEntry is allocated and is not used for binding properties, 
		// It is only enabled in special scenarios, such as reactive effects, etc.
		BindEntryContainer.Allocate(1);
	}

	template <typename TValue>
	bool SetValue(TValue& Field, typename Attribute::Reactive::TPropertyTypeSelector<TValue>::SetterType InValue);

	FBindEntry& GetBindEntry(const class FReactivePropertyBase* Property) const;
	FORCEINLINE FBindEntry& GetBindEntry(int32 LayerID) const
	{
		return BindEntryContainer.GetBindEntry(LayerID);
	}
	FORCEINLINE int32 GetBindEntriesNum() const
	{
		return BindEntryContainer.GetBindEntriesNum();
	}

	void RemoveBinding(const FBindEntryHandle& Handle) const;
	void ClearBindEntry(const class FReactivePropertyBase* Property) const;
	void ClearAllBindEntry() const;
private:
	mutable FBindEntryContainer BindEntryContainer;
};


template<typename FieldClassType>
struct TReactiveModel : public FReactiveModelBase, public IAttributeBindTracker
{
	using ThisFieldClass = FieldClassType;

	FORCEINLINE void MarkDirty(const class FReactivePropertyBase* Property)
	{
		OnSetAttributeValue(Property);
	}
protected:
	template<typename TProperty>
	void OnGetAttributeValue(TProperty* Property) const;
	template<typename TProperty>
	void OnSetAttributeValue(TProperty* Property);

	template<typename TProperty>
	void OnGetAttributeValue_Effect(TProperty* Property) const;

private:
	// IAttributeBindTracker
	virtual void Invoke(const FBindEntryHandle& Handle) const override
	{
		const FBindEntry& BindEntry = GetBindEntry(Handle.GetLayerID());
		BindEntry.Execute(Handle);
	}

	virtual void RemoveDependency(const FBindEntryHandle& Handle) const override
	{
		RemoveBinding(Handle);
	}

	virtual void CopyBindEntryItem(const FBindEntry::DelegateType& SrcEntryItem, int32 LayerID, FBindEntryHandle& OutHandle) const override
	{
		FBindEntry& BindEntry = GetBindEntry(LayerID);

		OutHandle = BindEntry.AddDelegateCopyFrom(SrcEntryItem);
	}
	virtual const FBindEntry& GetTrackerBindEntry(int32 LayerID) const override
	{
		return GetBindEntry(LayerID);
	}
	// ~IAttributeBindTracker

};

/** Base non-template class for use as an Id */
class STATEABILITYSCRIPTRUNTIME_API FReactivePropertyBase
{
public:
	enum class EAccessorVisibility { V_public, V_protected, V_private };

	constexpr FReactivePropertyBase(const ANSICHAR* InName, int32 InFieldOffset, EAccessorVisibility GetterVisibility, EAccessorVisibility SetterVisibility, bool bInHasSetter)
		: Name(InName)
		, AttributeID(INDEX_NONE)
		, FieldByteOffset(InFieldOffset)
		, bGetterIsPublic(GetterVisibility == EAccessorVisibility::V_public)
		, bSetterIsPublic(SetterVisibility == EAccessorVisibility::V_public)
		, bHasSetter(bInHasSetter)
	{
	}

	/* Returns Name of a property */
	FName GetName() const
	{
		return Name;
	}

	/* Returns Offset of a backing field from beginning of owning object */
	int32 GetFieldByteOffset() const
	{
		return FieldByteOffset;
	}

	/* Returns whether this property has Getter with public visibility */
	bool HasPublicGetter() const
	{
		return bGetterIsPublic;
	}

	/* Returns whether this property has Setter with public visibility */
	bool HasPublicSetter() const
	{
		return bSetterIsPublic;
	}

	/* Returns whether this property has Setter or is Getter-only */
	bool HasSetter() const
	{
		return bHasSetter;
	}

	int32 GetAttributeID() const
	{
		return AttributeID;
	}
private:
	friend class Attribute::Reactive::FReactiveRegistry;

	const ANSICHAR* Name;

	/** Used as main identifier, witch is the ordered AttributeID of the registered property. */
	mutable int32 AttributeID;
	int32 FieldByteOffset;
	uint8 bGetterIsPublic : 1;
	uint8 bSetterIsPublic : 1;
	uint8 bHasSetter : 1;
};

/*
 * Property of a Reactive
 */
template<typename TOwner, typename TValue>
class TReactiveProperty : public FReactivePropertyBase
{
public:
	using FReactiveModelType = TOwner;
	using FValueType = TValue;

	using FGetterReturnType = typename Attribute::Reactive::TPropertyTypeSelector<TValue>::GetterType;
	using FSetterArgumentType = typename Attribute::Reactive::TPropertyTypeSelector<TValue>::SetterType;

	using FPropertyGetterPtr = const TReactiveProperty<TOwner, TValue>* (*)();
	using FGetterPtr = FGetterReturnType(TOwner::*) ();
	using FSetterPtr = void (TOwner::*) (FSetterArgumentType);

	constexpr TReactiveProperty(FGetterPtr InGetter, FSetterPtr InSetter, FGetterPtr InEffectGetter, int32 InFieldOffset, EAccessorVisibility GetterVisibility, EAccessorVisibility SetterVisibility, const ANSICHAR* InName)
		: FReactivePropertyBase(InName, InFieldOffset, GetterVisibility, SetterVisibility, InSetter != nullptr)
		, Getter(InGetter)
		, Setter(InSetter)
		, EffectGetter(InEffectGetter)
	{

	}

	FORCEINLINE FGetterReturnType GetValue(FReactiveModelType* Owner) const
	{
		return (Owner->*Getter)();
	}

	FORCEINLINE TValue& GetValueRef(FReactiveModelType* Owner) const
	{
		return *(TValue*)((uint8*)Owner + GetFieldByteOffset());
	}

	FORCEINLINE FGetterReturnType GetValue_Effect(FReactiveModelType* Owner) const
	{
		return (Owner->*EffectGetter)();
	}

	FORCEINLINE void SetValue(FReactiveModelType* Owner, FSetterArgumentType Value) const
	{
		if (Setter)
		{
			(Owner->*Setter)(Value);
		}
	}
private:
	FGetterPtr Getter;
	FSetterPtr Setter;

	FGetterPtr EffectGetter;
};

template<typename TValue>
inline bool FReactiveModelBase::SetValue(TValue& Field, typename Attribute::Reactive::TPropertyTypeSelector<TValue>::SetterType InValue)
{
	if constexpr (TReactivePropertyTypeTraits<TValue>::WithSetterIdentical)
	{
		if constexpr (TStructOpsTypeTraits<TValue>::WithIdentical)
		{
			if (!Field.Identical(&InValue, 0))
			{
				Field = InValue;
				return true;
			}
			return false;
		}
		else
		{
			if (!(Field == InValue))
			{
				Field = InValue;
				return true;
			}
			return false;
		}
	}
	else
	{
		Field = InValue;
		return true;
	}
}

inline FBindEntry& FReactiveModelBase::GetBindEntry(const FReactivePropertyBase* Property) const
{
	int32 LayerID = Property->GetAttributeID();

	return BindEntryContainer.GetBindEntry(LayerID);
}

template<typename FieldClassType>
template<typename TProperty>
inline void TReactiveModel<FieldClassType>::OnGetAttributeValue(TProperty* Property) const
{
	
}

template<typename FieldClassType>
template<typename TProperty>
inline void TReactiveModel<FieldClassType>::OnSetAttributeValue(TProperty* Property)
{
	FBindEntry& BindEntry = GetBindEntry(Property);
	BindEntry.Broadcast();
}

template<typename FieldClassType>
template<typename TProperty>
inline void TReactiveModel<FieldClassType>::OnGetAttributeValue_Effect(TProperty* Property) const
{
	FAttributeBindEffect::UpdateDependency(this, Property->GetAttributeID());
}

//////////////////////////////////////////////////////////////////////////

template<typename TOwner, typename TProperty, typename TCallback>
typename TEnableIf<TIsInvocable<TCallback>::Value, FBindEntryHandle>::Type
inline Bind(TOwner* ThisPtr, TProperty* Property, TCallback&& Callback)
{
	FBindEntry& BindEntry = ThisPtr->GetBindEntry(Property);
	
	return BindEntry.AddDelegate(FBindEntry::DelegateType::CreateLambda(Forward<TCallback>(Callback)));
}

template<typename TOwner, typename TProperty, typename TMemberPtr>
typename TEnableIf<TIsMemberPointer<TMemberPtr>::Value, FBindEntryHandle>::Type
inline Bind(TOwner* ThisPtr, TProperty* Property, TMemberPtr Callback)
{
	FBindEntry& BindEntry = ThisPtr->GetBindEntry(Property);
	return BindEntry.AddDelegate(FBindEntry::DelegateType::CreateUObject(ThisPtr, Callback));
}

template<typename TOwner>
inline void UnBind(TOwner* ThisPtr, const FBindEntryHandle& Handle)
{
	ThisPtr->RemoveBinding(Handle);
}

template<typename TOwner, typename TCallback>
typename TEnableIf<TIsInvocable<TCallback>::Value, FAttributeBindEffect>::Type
inline MakeEffect(TOwner* ThisPtr, TCallback&& Callback)
{
	FBindEntry& BindEntry = ThisPtr->GetBindEntry(0);
	FBindEntryHandle Handle = BindEntry.AddDelegate(FBindEntry::DelegateType::CreateLambda(Forward<TCallback>(Callback)));

	return FAttributeBindEffect(ThisPtr, Handle);
}

template<typename TOwner, typename TMemberPtr>
typename TEnableIf<TIsMemberPointer<TMemberPtr>::Value, FAttributeBindEffect>::Type
inline MakeEffect(TOwner* ThisPtr, TMemberPtr Callback)
{
	FBindEntry& BindEntry = ThisPtr->GetBindEntry(0);
	FBindEntryHandle Handle = BindEntry.AddDelegate(FBindEntry::DelegateType::CreateUObject(ThisPtr, Callback));

	return FAttributeBindEffect(ThisPtr, Handle);
}

template<typename TOwner, typename TCallback>
typename TEnableIf<TIsInvocable<TCallback>::Value, FAttributeBindSharedEffect>::Type
inline MakeSharedEffect(TOwner* ThisPtr, TCallback&& Callback)
{
	FBindEntry& BindEntry = ThisPtr->GetBindEntry(0);
	FBindEntryHandle Handle = BindEntry.AddDelegate(FBindEntry::DelegateType::CreateLambda(Forward<TCallback>(Callback)));

	return FAttributeBindSharedEffect(ThisPtr, Handle);
}

template<typename TOwner, typename TMemberPtr>
typename TEnableIf<TIsMemberPointer<TMemberPtr>::Value, FAttributeBindSharedEffect>::Type
inline MakeSharedEffect(TOwner* ThisPtr, TMemberPtr Callback)
{
	FBindEntry& BindEntry = ThisPtr->GetBindEntry(0);
	FBindEntryHandle Handle = BindEntry.AddDelegate(FBindEntry::DelegateType::CreateUObject(ThisPtr, Callback));

	return FAttributeBindSharedEffect(ThisPtr, Handle);
}

//////////////////////////////////////////////////////////////////////////

/* Helper class that registers a property into reflection system */
template<typename TOwner, typename TValue, typename TReactiveProperty<TOwner, TValue>::FPropertyGetterPtr PropertyGetterPtr>
class TReactivePropertyRegistered : public TReactiveProperty<TOwner, TValue>
{
	using Super = TReactiveProperty<TOwner, TValue>;

public:
	constexpr TReactivePropertyRegistered(typename Super::FGetterPtr InGetter, typename Super::FSetterPtr InSetter, typename Super::FGetterPtr InEffectGetterPtr, int32 InFieldOffset, typename Super::EAccessorVisibility GetterVisibility, typename Super::EAccessorVisibility SetterVisibility, const ANSICHAR* InName)
		: Super(InGetter, InSetter, InEffectGetterPtr, InFieldOffset, GetterVisibility, SetterVisibility, InName)
	{
	}

	static const uint8 Registered;
};

template<typename TOwner, typename TValue, typename TReactiveProperty<TOwner, TValue>::FPropertyGetterPtr PropertyGetterPtr>
const uint8 TReactivePropertyRegistered<TOwner, TValue, PropertyGetterPtr>::Registered = Attribute::Reactive::FReactiveRegistryHelper_Property<TOwner, TValue>::RegisterProperty(PropertyGetterPtr);


template<typename TOwner>
class TReactiveFieldClassRegistered
{
public:
	constexpr TReactiveFieldClassRegistered()
	{
		static_assert(TIsDerivedFrom<TOwner, struct FReactiveModelBase>::Value, "Must be derived from FReactiveModelBase");
	}

	static const uint8 Registered;
};
template<typename TOwner>
const uint8 TReactiveFieldClassRegistered<TOwner>::Registered = Attribute::Reactive::FReactiveRegistryHelper_Field<TOwner>::RegisterFieldClass();

template<typename TOwner>
struct TReactiveFieldClassInitialization
{
	TReactiveFieldClassInitialization(struct FReactiveModelBase* ModelPtr)
	{
		ModelPtr->BindEntryContainer.Allocate(FReactiveModelTypeTraitsBase<TOwner>::AttributeCount);
	}
};