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

#if WITH_EDITOR
	// 必须统一利用CreateInstance来创建实例
	template<typename NodeType>
	static NodeType* CreateInstance(UStateAbilityScript* Script, UClass* Class);
	static UStateAbilityNodeBase* CreateInstance(UStateAbilityScript* Script, UClass* Class);

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
	 * 不允许手动修改哦~ (∩_∩)
	 */
	UPROPERTY(VisibleAnywhere, Category = "一般")
	uint32 UniqueID;
	UPROPERTY(EditAnywhere, Category = "Static Data")
	FConfigVarsBag ConfigVarsBag;

	// 支持动态添加EventSlot
	UPROPERTY(meta = (DataStruct = "/Script/StateAbilityScriptRuntime.ConfigVars_EventSlotBag"))
	FConfigVarsBag DynamicEventSlotBag;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "一般")
	ENodeRepMode NodeRepMode = ENodeRepMode::Default;
	UPROPERTY(EditAnywhere, Category = "一般")
	FName Comment;
	UPROPERTY(EditAnywhere, Category = "一般")
	FName DisplayName;
#endif

};

#if WITH_EDITOR
template<typename NodeType>
NodeType* UStateAbilityNodeBase::CreateInstance(UStateAbilityScript* Script, UClass* Class)
{
	return Cast<NodeType>(CreateInstance(Script, Class));
}
#endif
