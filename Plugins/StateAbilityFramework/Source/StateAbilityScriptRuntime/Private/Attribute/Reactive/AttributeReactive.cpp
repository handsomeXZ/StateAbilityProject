#include "Attribute/Reactive/AttributeReactive.h"

void FReactiveModelBase::RemoveBinding(const FBindEntryHandle& Handle) const
{
	BindEntryContainer.RemoveBinding(Handle);
}

void FReactiveModelBase::ClearBindEntry(const FReactivePropertyBase* Property) const
{
	ensureMsgf(Property, TEXT("Removing bindings failed because property passed is nullptr"));
	BindEntryContainer.ClearBindEntry(Property->GetAttributeID());
}

void FReactiveModelBase::ClearAllBindEntry() const
{
	BindEntryContainer.ClearAllBindEntry();
}
