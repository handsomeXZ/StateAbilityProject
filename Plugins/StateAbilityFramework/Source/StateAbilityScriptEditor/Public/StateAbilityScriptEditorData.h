#pragma once

#include "CoreMinimal.h"
#include "StateAbilityScriptEditorData.generated.h"

class UGraphAbilityNode;
class UStateTreeBaseNode;
class UStateAbilityNodeBase;
enum class EScriptStateTreeNodeType : uint8;
enum class EStateEventSlotType : uint8;

UCLASS()
class UStateAbilityScriptEditorData : public UObject
{
	GENERATED_BODY()
	friend class FScriptStateTreeViewModel;
public:
	UStateAbilityScriptEditorData();

	virtual void Init();
	virtual void Save();
private:
	void SaveScriptStateTree();

	// 清扫
	void RemoveOrphanedObjects();
	void CollectAllNodeInstancesAndPersistObjects(TSet<UObject*>& NodeInstances);
	bool CanRemoveNestedObject(UObject* TestObject);

	void TraverseAllGraph(TFunction<void(UEdGraph*)> InProcessor);
	void TraverseAllNodeInstance(TFunction<void(UStateAbilityNodeBase*)> InProcessor);
	void TraverseStateTreeNodeRecursive(UStateTreeBaseNode* CurrentNode, TFunction<void(UStateTreeBaseNode*)> InProcessor);
	void TraverseGraphNodeRecursive(UGraphAbilityNode* PrevNode, UGraphAbilityNode* CurrentNode, TFunction<void(UGraphAbilityNode* PrevNode, UGraphAbilityNode* CurrentNode)> InProcessor);
	void TraverseGraphRecursive(UStateTreeBaseNode* CurrentNode, TFunction<void(UEdGraph*)> InProcessor);

	void ResetObjectOwner(UObject* NewOwner, UObject* Object, ERenameFlags AdditionalFlags = RF_NoFlags);

	UStateTreeBaseNode& AddRootState();

private:
	UPROPERTY()
	TArray<TObjectPtr<UStateTreeBaseNode>> TreeRoots;	// 默认现在只有一个Root
};