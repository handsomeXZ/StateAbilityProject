#include "Component/StateAbility/StateAbilityAction.h"

#include "Component/StateAbility/Script/StateAbilityScript.h"

//////////////////////////////////////////////////////////////////////////
// FActionExecContext
FActionExecContext::FActionExecContext(UStateAbilityScript* InScript)
	: Result(EResult::Success), Script(InScript) 
{}

void FActionExecContext::EnqueueImmediateEvent(const FConfigVars_EventSlot& Event)
{
	ImmediateEvents.Add(Event);
}

void FActionExecContext::EnqueuePendingEvent(const FConfigVars_EventSlot& Event)
{
	if (IsValid(Script))
	{
		Script->EnqueueEvent(Event.UID);
	}
}

//////////////////////////////////////////////////////////////////////////
// UStateAbilityAction
void UStateAbilityAction::Execute(FActionExecContext& Conext) const
{
	Conext.Script->MarkActionExecuted(UniqueID);

	OnExecute(Conext);
	
	Conext.EnqueueImmediateEvent(ThenExec_Event);
}
UStateAbilityAction::UStateAbilityAction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UStateAbilityEventSlot::UStateAbilityEventSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FName("Event");
}

const FGuid UStateAbilityEventSlot::GetUID(UStateAbilityScript* InScript) const
{
	FConstStructView EventSlot = ConfigVarsBag.LoadData(this);
	if (EventSlot.IsValid())
	{
		return EventSlot.Get<const FConfigVars_EventSlot>().UID;
	}

	return FGuid(0, 0, 0, 0);
}

#if WITH_EDITOR
const FGuid UStateAbilityEventSlot::EditorGetUID(UStateAbilityScript* InScript) const
{
	FConstStructView EventSlot = ConfigVarsBag.EditorLoadData(this);
	if (EventSlot.IsValid())
	{
		return EventSlot.Get<const FConfigVars_EventSlot>().UID;
	}

	return FGuid(0, 0, 0, 0);
}
#endif