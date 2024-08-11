
#pragma once

#include "CoreMinimal.h"
#include "Node/GraphAbilityNode.h"
#include "GraphAbilityNode_State.generated.h"

class UObject;
class UStateTreeBaseNode;
class UStateTreeStateNode;
class FScriptStateTreeViewModel;


USTRUCT()
struct FStackedPinInfo
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<UStateTreeBaseNode*, FName> StackedEventNode;
	UPROPERTY()
	TObjectPtr<UStateTreeBaseNode> ConnectedEventNode = nullptr;
	UPROPERTY()
	TObjectPtr<UStateTreeBaseNode> RootEventNode = nullptr;
};

/**
 * 此State节点比较特殊，自身包含一个StateTree的视图，虽然是GraphAbilityNode，但与StateTree共享一个RootStateInstance.
 * State的父子关系由StateTreeNode记录
 */
UCLASS(MinimalAPI)
class UGraphAbilityNode_State : public UGraphAbilityNode
{
	GENERATED_BODY()
public:
	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual bool CanDuplicateNode() const override { return true; }
	//~ End UEdGraphNode Interface

	//~ Begin USCTGraphNode Interface
	virtual UEdGraphPin* GetInputPin() const override;
	virtual UEdGraphPin* GetOutputPin() const override;
	virtual FString GetNodeName() const override;
	virtual void InitializeNode(UEdGraph* InGraph) override;
	//~ End USCTGraphNode Interface

	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	//~ End UObject Interface

	TSharedPtr<FScriptStateTreeViewModel> GetOrCreateStateTreeViewModel();
	UClass* GetNodeClass() const;
	FStackedPinInfo* FindStackedPinInfo(UEdGraphPin* Pin);
	UStateTreeStateNode* GetRootStateNode() { return RootStateNode; }
protected:
	void ReGeneratePins();
	// 创建堆叠的OutPing
	void CreateStackedPins(UStateTreeBaseNode* Node, FStackedPinInfo PrevStackedPinInfo, TMap<UStateTreeBaseNode*, TArray<UEdGraphPin*>>& LinkedToHistory);

	void HandleModelNodeAdded(UStateTreeBaseNode* ParentNode, UStateTreeBaseNode* NewNode);
	void HandleModelNodesMoved(const TSet<UStateTreeBaseNode*>& AffectedParents, const TSet<UStateTreeBaseNode*>& MovedNodes);
	void HandleDetailsViewChangingProperties(UObject* SelectedObject);
	void HandleUpdateStateTree();
protected:
	UPROPERTY()
	TObjectPtr<UStateTreeStateNode> RootStateNode = nullptr;	// 不会在StateTreeView中显示出来
	UPROPERTY()
	TMap<FGuid, FStackedPinInfo> StackedPins;
	UPROPERTY()
	TMap<FGuid, TObjectPtr<UStateTreeBaseNode>> RootEventPins;

	TSharedPtr<FScriptStateTreeViewModel> StateTreeViewModel;
};
