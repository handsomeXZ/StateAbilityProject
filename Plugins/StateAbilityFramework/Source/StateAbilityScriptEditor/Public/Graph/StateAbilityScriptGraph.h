// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "Graph/StateAbilityGraph.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "StateAbilityScriptGraph.generated.h"

class UObject;
class UStateAbilityScript;
class UStateAbilityScriptArchetype;
class UGraphAbilityNode;
class UStateAbilityNodeBase;
class UStateAbilityEventSlot;
class UConfigVarsData;

class ULinkerPlaceholderExportObjectExternal;
typedef ULinkerPlaceholderExportObjectExternal UConfigVarsLazyDataBag;


UCLASS(MinimalAPI)
class UStateAbilityScriptGraph : public UStateAbilityGraph
{
	GENERATED_UCLASS_BODY()
public:
	static UStateAbilityScriptGraph* CreateNewGraph(UObject* ParentScope, const FName& GraphName, TSubclassOf<class UEdGraph> GraphClass, TSubclassOf<class UEdGraphSchema> SchemaClass);

	virtual void OnCompile() override;
	virtual void NotifyGraphChanged(const FEdGraphEditAction& Action) override;

	void CompileNode_Recursive(UStateAbilityScriptArchetype* ScriptArchetype, UGraphAbilityNode* RootGraphNode);
	void Release();
	void Release_Recursive(UStateAbilityScriptArchetype* ScriptArchetype, UGraphAbilityNode* RootGraphNode);

	const TMap<FGuid, TObjectPtr<UStateAbilityEventSlot>>& CollectAllEventSlot() { return EventSlotsMap; }

	// Parent instance node
	UPROPERTY()
	TObjectPtr<UGraphAbilityNode> OwnerSDTGraphNode;

	UPROPERTY()
	TMap<FGuid, TObjectPtr<UStateAbilityEventSlot>> EventSlotsMap;
};

