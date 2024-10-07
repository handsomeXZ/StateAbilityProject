// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"

#include "ConfigVarsTypes.h"
#include "Component/StateAbility/StateAbilityConfigVars/StateAbilityConfigVarsTypes.h"

#include "StateAbilityNodeBase.generated.h"

class UStateAbilityScript;
class UStateAbilityScriptArchetype;

UENUM(BlueprintType)
enum class ENodeRepMode : uint8
{
	Default,	// 仅state会向Autonomous同步
	Local,		// state, Action会向Autonomous同步
	All,		// state, Action会向Autonomous，Simulated同步
};

UCLASS(Abstract)
class STATEABILITYSCRIPTRUNTIME_API UStateAbilityNodeBase : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	const UScriptStruct* GetDataStruct();

	// 所有通过EnqueueEvent标记的事件，都是在下一帧才处理。
	void EnqueueEvent(const FConfigVars_EventSlot& Event);

	template<typename TStruct>
	void InitializeConfigVars(bool bExplicitDataStruct = false);

#if WITH_EDITOR
	// 必须统一利用CreateInstance来创建实例
	template<typename NodeType>
	static NodeType* CreateInstance(UStateAbilityScriptArchetype* ScriptArchetype, UClass* Class)
	{
		return Cast<NodeType>(CreateInstance(ScriptArchetype, Class));
	}
	static UStateAbilityNodeBase* CreateInstance(UStateAbilityScriptArchetype* ScriptArchetype, UClass* Class);

	TMap<FName, FConfigVars_EventSlot> GetEventSlots();

	virtual void OnCreatedInEditor() {
		// 利用UObject的UniqueID来标记Node
		UniqueID = GetUniqueID();
	}
#endif

public:
	static const FName ConfigVarsBagName;
	static const FName DynamicEventSlotBagName;

	/**
	 * 仅能保证在Script内是独一无二的。
	 * 不允许手动修改
	 */
	UPROPERTY(VisibleAnywhere, Category = "一般")
	uint32 UniqueID = 0;
	UPROPERTY(EditAnywhere, Category = "Static Data", meta = (MetaFromOuter))
	FConfigVarsBag ConfigVarsBag;

	// 支持动态添加EventSlot
	UPROPERTY(meta = (DataStruct = "/Script/StateAbilityScriptRuntime.ConfigVars_EventSlotBag"))
	FConfigVarsBag DynamicEventSlotBag;

	UPROPERTY()
	FConfigVars_EventSlot ThenExec_Event;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "一般")
	ENodeRepMode NodeRepMode = ENodeRepMode::Default;
	UPROPERTY(EditAnywhere, Category = "一般")
	FName Comment;
	UPROPERTY(EditAnywhere, Category = "一般")
	FName DisplayName;
#endif

};

template<typename TStruct>
void UStateAbilityNodeBase::InitializeConfigVars(bool bExplicitDataStruct)
{
#if WITH_EDITOR
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		UScriptStruct* DataStruct = TStruct::StaticStruct();
		GetClass()->SetMetaData(TEXT("ConfigVarsStruct"), *(DataStruct->GetStructPathName().ToString()));
		if (bExplicitDataStruct)
		{
			GetClass()->SetMetaData(TEXT("ExplicitConfigVarsStruct"), nullptr);
		}
	}
#endif
}