#include "Component/StateAbility/StateAbilityBranch.h"

UStateAbilityCondition::UStateAbilityCondition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitializeConfigVars<FConfigVars_Bool>();
}

void UStateAbilityCondition::OnExecute(FActionExecContext& Conext) const
{
	FConstStructView BoolDataView = ConfigVarsBag.LoadData(this);
	const FConfigVars_Bool* ConfigVars_Bool = nullptr;

	if (BoolDataView.IsValid())
	{
		ConfigVars_Bool = BoolDataView.GetPtr<const FConfigVars_Bool>();
	}

	if (ConfigVars_Bool && ConfigVars_Bool->GetValue())
	{
		Conext.EnqueueImmediateEvent(True_Event);
	}
	else
	{
		Conext.EnqueueImmediateEvent(False_Event);
	}
}