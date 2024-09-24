#include "Attribute/Reactive/AttributeReactive.h"

void FReactiveModelBase::RemoveBinding(const FBindEntryHandle& Handle)
{
	BindEntryContainer.RemoveBinding(Handle);
}
