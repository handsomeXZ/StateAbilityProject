#include "Attribute/Reactive/AttributeReactive.h"

void FReactiveModelBase::ClearBindEntry(const FReactivePropertyBase* Property) const
{
	ensureMsgf(Property, TEXT("Removing bindings failed because property passed is nullptr"));
	BindEntryContainer.ClearBindEntry(Property->GetAttributeID());
}

void FReactiveModelBase::MarkDirty(int32 LayerID)
{
	OnSetAttributeValue(LayerID);
}

void FReactiveModelBase::MarkDirty(const FReactivePropertyBase* Property)
{
	MarkDirty(Property->GetAttributeID());
}

void FReactiveModelBase::OnGetAttributeValue(int32 LayerID) const
{

}

void FReactiveModelBase::OnSetAttributeValue(int32 LayerID)
{
	FBindEntry& BindEntry = GetBindEntry(LayerID);
	BindEntry.Broadcast();
}

void FReactiveModelBase::OnGetAttributeValue_Effect(int32 LayerID) const
{
	FAttributeBindEffect::AddDependency(this, LayerID);
}
