#include "Component/Mover/CFrameMovementTransition.h"

#include "Component/Mover/CFrameMovementMode.h"
#include "Component/Mover/CFrameMovementContext.h"

DEFINE_LOG_CATEGORY(LogCFrameMoveModeTransition);

UCFrameMoveModeTransition::UCFrameMoveModeTransition()
	: Super()
	, FromModeClass(nullptr)
	, ToModeClass(nullptr)
{

}

UCFrameMoveModeTransition::UCFrameMoveModeTransition(UClass* InFromModeClass, UClass* InToModeClass)
	: Super()
	, FromModeClass(InFromModeClass)
	, ToModeClass(InToModeClass)
{

}

bool UCFrameMoveModeTransition::OnEvaluate(FCFrameMovementContext& Context) const
{
	ensureMsgf(Context.CurrentMode->GetClass()->IsChildOf(FromModeClass), TEXT("CurrentMode[%s] is does not match Transition[%s]"), 
		*(Context.CurrentMode->GetClass()->GetFName().ToString()), *(GetClass()->GetFName().ToString()));

	return true;
}

