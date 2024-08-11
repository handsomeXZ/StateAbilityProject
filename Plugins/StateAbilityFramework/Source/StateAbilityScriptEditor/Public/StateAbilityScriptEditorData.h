#pragma once

#include "CoreMinimal.h"

#include "Component/StateAbility/StateAbilityConfigVars/StateAbilityConfigVarsTypes.h"

#include "StateAbilityScriptEditorData.generated.h"

class UGraphAbilityNode;
class UStateTreeBaseNode;
class UStateAbilityNodeBase;
enum class EScriptStateTreeNodeType : uint8;
enum class EStateEventSlotType : uint8;

class FAbilityScriptViewModel : public TSharedFromThis<FAbilityScriptViewModel>
{
public:
	FAbilityScriptViewModel() {}
	FAbilityScriptViewModel(TSharedRef<FUICommandList> InToolkitCommands);

	TSharedPtr<FUICommandList> ToolkitCommands;
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnStateTreeNodeSelected, const TSet<UObject*>& /*SelectedNodes*/);
	FOnStateTreeNodeSelected OnStateTreeNodeSelected;

	void RecordEventSlotWithPin(UEdGraphPin* Pin, const FConfigVars_EventSlot& EventSlot);
	void ClearRecordedEventslot(UEdGraphPin* Pin);
	FConfigVars_EventSlot* FindEventSlotByPin(UEdGraphPin* Pin);
private:
	TMap<UEdGraphPin*, FConfigVars_EventSlot> PinToEventSlot;
};

UCLASS()
class UStateAbilityScriptEditorData : public UObject
{
	GENERATED_BODY()
	friend class FScriptStateTreeViewModel;
public:
	UStateAbilityScriptEditorData();

	virtual void Init();
	virtual void OpenEdit(TSharedRef<FUICommandList> InToolkitCommands);
	virtual void Save();

	void TryRemoveOrphanedObjects();

	TSharedPtr<FAbilityScriptViewModel> GetScriptViewModel(){ return ScriptViewModel; }
	UEdGraph* GetStateTreeGraph() { return StateTreeGraph; }
private:
	void SaveScriptStateTree();

	// 清扫
	void RemoveOrphanedObjects();
	void CollectAllNodeInstancesAndPersistObjects(TSet<UObject*>& NodeInstances);
	bool CanRemoveNestedObject(UObject* TestObject);

	void TraverseStateTreeNodeRecursive(UStateTreeBaseNode* PrevNode, UStateTreeBaseNode* CurrentNode, TFunction<void(UStateTreeBaseNode* PrevNode, UStateTreeBaseNode* CurrentNode)> InProcessor);
	void TraverseGraphNodeRecursive(UGraphAbilityNode* PrevGraphNode, UGraphAbilityNode* CurrentGraphNode, TFunction<void(UGraphAbilityNode* PrevGraphNode, UGraphAbilityNode* CurrentGraphNode)> InProcessor);

	// EventSlot
	void SaveEventSlotToNode(UObject* Node);

private:
	UPROPERTY()
	TObjectPtr<UEdGraph> StateTreeGraph;

	TSharedPtr<FAbilityScriptViewModel> ScriptViewModel;
};