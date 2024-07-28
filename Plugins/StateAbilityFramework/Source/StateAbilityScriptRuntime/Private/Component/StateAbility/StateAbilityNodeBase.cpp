#include "Component/StateAbility/StateAbilityNodeBase.h"

#include "Component/StateAbility/Script/StateAbilityScript.h"

#define INHERITS_PROP_META(PropName, MetaName) if (GetClass()->HasMetaData(MetaName)) {	\
	GetClass()->FindPropertyByName(PropName)->SetMetaData(MetaName, *(GetClass()->GetMetaData(MetaName))); }	\

const FName UStateAbilityNodeBase::ConfigVarsBagName = TEXT("ConfigVarsBag");
const FName UStateAbilityNodeBase::DynamicEventSlotBagName = TEXT("DynamicEventSlotBag");

static const FName NAME_DataStruct = "DataStruct";
static const FName NAME_InheritedStruct = "InheritedDataStruct";
static const FName NAME_ConfigVarsIcon = "ConfigVarsIcon";

UStateAbilityNodeBase::UStateAbilityNodeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NodeRepMode(ENodeRepMode::Default)
	, Comment(FName())
	, DisplayName(FName())
{
	INHERITS_PROP_META(ConfigVarsBagName, NAME_DataStruct);
	INHERITS_PROP_META(ConfigVarsBagName, NAME_InheritedStruct);
	INHERITS_PROP_META(ConfigVarsBagName, NAME_ConfigVarsIcon);
}

const UScriptStruct* UStateAbilityNodeBase::GetDataStruct()
{

	UScriptStruct* ConfigVarsDataStruct = nullptr;
	{
		const FString& DataStructName = GetClass()->GetMetaData(NAME_DataStruct);
		if (!DataStructName.IsEmpty())
		{
			ConfigVarsDataStruct = UClass::TryFindTypeSlow<UScriptStruct>(DataStructName);
			if (!ConfigVarsDataStruct)
			{
				ConfigVarsDataStruct = LoadObject<UScriptStruct>(nullptr, *DataStructName);
			}
		}
	}

	return ConfigVarsDataStruct ? ConfigVarsDataStruct : FConfigVarsData_Empty::StaticStruct();
}

void UStateAbilityNodeBase::EnqueueEvent(const FConfigVars_EventSlot& Event)
{
	if (UStateAbilityScript* Script = GetTypedOuter<UStateAbilityScript>())
	{
		Script->EnqueueEvent(Event.UID);
	}
}

#if WITH_EDITOR
TMap<FName, FConfigVars_EventSlot> UStateAbilityNodeBase::GetEventSlots()
{
	// @TODO：开销太大了

	TMap<FName, FConfigVars_EventSlot> Result;

	// Native Event
	for (TFieldIterator<FProperty> PropertyIter(GetClass()); PropertyIter; ++PropertyIter)
	{
		FProperty* ChildProperty = *PropertyIter;
		FStructProperty* StructProperty = CastField<FStructProperty>(ChildProperty);

		if (!StructProperty || !StructProperty->Struct->IsChildOf(FConfigVars_EventSlot::StaticStruct()))
		{
			continue;
		}

		FConfigVars_EventSlot* EventSlotPtr = StructProperty->ContainerPtrToValuePtr<FConfigVars_EventSlot>(this);

		Result.Emplace(FName(StructProperty->GetDisplayNameText().ToString()), *EventSlotPtr);
	}


	// Dynamic Event
	FStructView EventSlotBagView = DynamicEventSlotBag.EditorLoadData(this);
	if (EventSlotBagView.IsValid())
	{
		Result.Append(EventSlotBagView.Get<FConfigVars_EventSlotBag>().EventSlots);
	}

	return Result;
}

UStateAbilityNodeBase* UStateAbilityNodeBase::CreateInstance(UStateAbilityScript* Script, UClass* Class)
{
	if (!Script || !Class || !Class->IsChildOf(UStateAbilityNodeBase::StaticClass()))
	{
		return nullptr;
	}
	// 加入Script，方便后续清扫，不用担心序列化问题，因为没有RF_Standalone标记，所以这里是根据引用序列化的。
	UStateAbilityNodeBase* Instance = NewObject<UStateAbilityNodeBase>(Script, Class);

	Instance->OnCreatedInEditor();
	Instance->ConfigVarsBag.EditorLoadOrAddData(Instance, Instance->GetDataStruct());

	//Script->AddDataExportIndex(Instance->UniqueID, Instance->ConfigVarsBag.GetExportIndex());

	return Instance;
}
#endif