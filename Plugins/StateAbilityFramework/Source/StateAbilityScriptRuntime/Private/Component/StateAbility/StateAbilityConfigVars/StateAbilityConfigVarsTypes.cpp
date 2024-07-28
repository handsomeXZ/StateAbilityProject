#include "Component/StateAbility/StateAbilityConfigVars/StateAbilityConfigVarsTypes.h"


//////////////////////////////////////////////////////////////////////////
// FConfigVars_EventSlot
FConfigVars_EventSlot::FConfigVars_EventSlot()
	: UID(FGuid::NewGuid())
{
}

FConfigVars_EventSlot::FConfigVars_EventSlot(const FConfigVars_EventSlot& Other)
	: UID(Other.UID)
{
}

FConfigVars_EventSlot& FConfigVars_EventSlot::operator=(const FConfigVars_EventSlot& Other)
{
	UID = Other.UID;
	return *this;
}
