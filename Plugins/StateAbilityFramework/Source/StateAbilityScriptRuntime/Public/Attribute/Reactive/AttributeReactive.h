

#pragma once

#include "Delegates/DelegateSignatureImpl.inl"

#include "AttributeTraits.h"
#include "AttributeRegistry.h"
#include "AttributeMacros.h"
#include "AttributeBinding.h"

struct STATEABILITYSCRIPTRUNTIME_API FReactiveModelBase
{
	template<typename TOwner>
	friend struct TReactiveFieldClassInitialization;

	FBindEntry& GetBindEntry(const class FReactivePropertyBase* Property);
	FORCEINLINE FBindEntry& GetBindEntry(int32 LayerID)
	{
		return BindEntryContainer.GetBindEntry(LayerID);
	}
	FORCEINLINE const FBindEntry& GetBindEntry(int32 LayerID) const
	{
		return BindEntryContainer.GetBindEntry(LayerID);
	}

	void RemoveBinding(const FBindEntryHandle& Handle);
private:
	FBindEntryContainer BindEntryContainer;
};


template<typename FieldClassType>
struct TReactiveModel : public FReactiveModelBase, public IAttributeBindTracker
{
	using ThisFieldClass = FieldClassType;

	template<typename TProperty>
	void OnGetAttributeValue(TProperty* Property);
	template<typename TProperty>
	void OnSetAttributeValue(TProperty* Property);

	template<typename TProperty>
	void OnGetAttributeValue_Effect(TProperty* Property);

private:
	// IAttributeBindTracker
	virtual void Invoke(const FBindEntryHandle& Handle) const override
	{
		const FBindEntry& BindEntry = GetBindEntry(Handle.GetLayerID());
		BindEntry.Execute(Handle);
	}

	virtual void RemoveDependency(const FBindEntryHandle& Handle) override
	{
		RemoveBinding(Handle);
	}

	virtual void CopyBindEntryItem(const FBindEntry::DelegateType& SrcEntryItem, int32 LayerID, FBindEntryHandle& OutHandle) override
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
		, FieldOffset(InFieldOffset)
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
	int32 GetFieldOffset() const
	{
		return FieldOffset;
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
	int32 FieldOffset;
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

	constexpr TReactiveProperty(FGetterPtr InGetter, FSetterPtr InSetter, int32 InFieldOffset, EAccessorVisibility GetterVisibility, EAccessorVisibility SetterVisibility, const ANSICHAR* InName)
		: FReactivePropertyBase(InName, InFieldOffset, GetterVisibility, SetterVisibility, InSetter != nullptr)
		, Getter(InGetter)
		, Setter(InSetter)
	{
	}

	/* Returns value of this property from given ViewModel */
	FGetterReturnType GetValue(FReactiveModelType* Owner) const
	{
		return (Owner->*Getter)();
	}

	/* Sets value of this property to given ViewModel */
	void SetValue(FReactiveModelType* Owner, FSetterArgumentType Value) const
	{
		if (Setter)
		{
			(Owner->*Setter)(Value);
		}
	}

private:
	FGetterPtr Getter;
	FSetterPtr Setter; // this variable MUST BE the last one due to a bug in MSVC compiler
};

template<typename TOwner>
struct TBindEntryHandle : public FBindEntryHandle
{
	TBindEntryHandle()
	{}

	TBindEntryHandle(FReactivePropertyBase& Property, FDelegateHandle DelegateHandle)
		: FBindEntryHandle(Property.GetAttributeID(), DelegateHandle)
	{}
};

inline FBindEntry& FReactiveModelBase::GetBindEntry(const FReactivePropertyBase* Property)
{
	int32 LayerID = Property->GetAttributeID();

	return BindEntryContainer.GetBindEntry(LayerID);
}

template<typename FieldClassType>
template<typename TProperty>
inline void TReactiveModel<FieldClassType>::OnGetAttributeValue(TProperty* Property)
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
inline void TReactiveModel<FieldClassType>::OnGetAttributeValue_Effect(TProperty* Property)
{
	FAttributeBindEffect::UpdateDependency(this, Property->GetAttributeID());
}

template<typename TOwner, typename TProperty, typename TCallback>
typename TEnableIf<TIsInvocable<TCallback, typename TProperty::FValueType>::Value, FBindEntryHandle>::Type
Bind(TOwner* ThisPtr, TProperty* Property, TCallback&& Callback)
{
	FBindEntry& BindEntry = ThisPtr->GetBindEntry(Property);
	return TBindEntryHandle<TProperty::FReactiveModelType>(*Property, MoveTemp(BindEntry.AddDelegate(FBindEntry::DelegateType::CreateLambda(Forward<TCallback>(Callback)))));
}

template<typename TOwner, typename TProperty, typename TMemberPtr>
typename TEnableIf<TIsMemberPointer<TMemberPtr>::Value, FBindEntryHandle>::Type
Bind(TOwner* ThisPtr, TProperty* Property, TMemberPtr Callback)
{
	FBindEntry& BindEntry = ThisPtr->GetBindEntry(Property);
	return TBindEntryHandle<TProperty::FReactiveModelType>(*Property, MoveTemp(BindEntry.AddDelegate(FBindEntry::DelegateType::CreateUObject(ThisPtr, Callback))));
}

//////////////////////////////////////////////////////////////////////////

/* Helper class that registers a property into reflection system */
template<typename TOwner, typename TValue, typename TReactiveProperty<TOwner, TValue>::FPropertyGetterPtr PropertyGetterPtr>
class TReactivePropertyRegistered : public TReactiveProperty<TOwner, TValue>
{
	using Super = TReactiveProperty<TOwner, TValue>;

public:
	constexpr TReactivePropertyRegistered(typename Super::FGetterPtr InGetter, typename Super::FSetterPtr InSetter, int32 InFieldOffset, typename Super::EAccessorVisibility GetterVisibility, typename Super::EAccessorVisibility SetterVisibility, const ANSICHAR* InName)
		: Super(InGetter, InSetter, InFieldOffset, GetterVisibility, SetterVisibility, InName)
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