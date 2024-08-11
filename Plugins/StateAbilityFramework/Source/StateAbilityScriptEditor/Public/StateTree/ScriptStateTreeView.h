// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Framework/Docking/TabManager.h"

#include "ScriptStateTreeView.generated.h"

namespace ESelectInfo { enum Type : int; }

class UEdGraph;
class UStateAbilityScript;
class UStateAbilityScriptArchetype;
class UStateAbilityState;
class UStateTreeBaseNode;
class UStateAbilityScriptEditorData;

UENUM()
enum class EScriptStateTreeNodeType : uint8
{
	None,
	Slot,
	Action,
	State,
	Shared,
};

enum class EScriptStateTreeViewModelInsert : uint8
{
	Before,
	After,
	Into,
};

/**
 * ModelView for editing StateAbilityScript's StateTree.
 */
class FScriptStateTreeViewModel : public FEditorUndoClient, public TSharedFromThis<FScriptStateTreeViewModel>
{
public:
	FScriptStateTreeViewModel(UStateAbilityScriptEditorData* InEditorData);
	virtual ~FScriptStateTreeViewModel() override;

	void Init(UStateTreeStateNode* InRootStateNode);

	//~ FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	// Selection handling.
	void ClearSelection();
	void ClearSelection(UObject* ClearSelected);
	void SetSelection(UStateTreeBaseNode* Selected);
	void SetSelection(const TArray<TWeakObjectPtr<UStateTreeBaseNode>>& InSelectedNodes);
	bool IsSelected(const UStateTreeBaseNode* Node) const;
	bool IsChildOfSelection(const UStateTreeBaseNode* Node) const;
	void GetSelectedNodes(TArray<UStateTreeBaseNode*>& OutSelectedNodes);
	void GetSelectedNodes(TArray<TWeakObjectPtr<UStateTreeBaseNode>>& OutSelectedNodes);
	UStateTreeBaseNode* GetFirstSelectedNode() { return (SelectedNodes.IsEmpty() ? nullptr : (*SelectedNodes.begin()).Get()); }
	void DeleteSelectedNode();

	bool HasSelection() const;
	bool HasUniqueName(UStateTreeBaseNode* InNode, bool bIsSameType = true);
	void TraversingAllNodes(TFunction<void(UStateTreeBaseNode*)> InProcessor);
	void TraversingAllNodesRecursive(UStateTreeBaseNode* ParentNode, TFunction<void(UStateTreeBaseNode*)> InProcessor);

	// bIsDeferredInitialize，延迟初始化，允许你在外部获取到Node后再手动执行 Init()
	UStateTreeBaseNode* CreateNewNode(UStateTreeBaseNode* SlotNode, EScriptStateTreeNodeType NodeType, bool bIsDeferredInitialize = false);
	void RenameNode(UStateTreeBaseNode* Node, FName NewName);
	void MoveSelectedNodesBefore(UStateTreeBaseNode* TargetNode);
	void MoveSelectedNodesAfter(UStateTreeBaseNode* TargetNode);
	void MoveSelectedNodesInto(UStateTreeBaseNode* TargetNode);


	TArray<TObjectPtr<UStateTreeBaseNode>>& GetTreeRoots();
	UStateAbilityScriptArchetype* GetScriptArchetype();
	UStateAbilityScriptEditorData* GetScriptEditorData();


	void OnNodeAdded(UStateTreeBaseNode* ParentNode, UStateTreeBaseNode* NewNode);
	void OnNodesMoved(const TSet<UStateTreeBaseNode*>& AffectedParents, const TSet<UStateTreeBaseNode*>& MovedNodes);
	void OnNodeRelease(UStateTreeBaseNode* OldNode);

	void OnSpawnNodeGraph(UEdGraph* NewGraph);
	void OnDetailsViewChangingProperties(UObject* SelectedObject);

	void UpdateStateTree();
	void UpdateStateNode();
	void UpdateStateNodePin();

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnNodeAdded, UStateTreeBaseNode* /*ParentNode*/, UStateTreeBaseNode* /*NewNode*/);
	FOnNodeAdded OnNodeAdded_Delegate;
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnNodesMoved, const TSet<UStateTreeBaseNode*>& /*AffectedParents*/, const TSet<UStateTreeBaseNode*>& /*MovedNodes*/);
	FOnNodesMoved OnNodesMoved_Delegate;
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnNodeRelease, UStateTreeBaseNode*);
	FOnNodeRelease OnNodeRelease_Delegate;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSpawnNodeGraph, UEdGraph*);
	FOnSpawnNodeGraph OnSpawnNodeGraph_Delegate;
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDetailsViewChangingProperties, UObject* /*SelectedObject*/);
	FOnDetailsViewChangingProperties OnDetailsViewChangingProperties_Delegate;
	DECLARE_MULTICAST_DELEGATE(FOnUpdateStateTree);
	FOnUpdateStateTree OnUpdateStateTree_Delegate;
	DECLARE_MULTICAST_DELEGATE(FOnUpdateStateTree);
	FOnUpdateStateTree OnUpdateStateNode_Delegate;
	DECLARE_MULTICAST_DELEGATE(FOnUpdateStateTree);
	FOnUpdateStateTree OnUpdateStateNodePin_Delegate;

	DECLARE_MULTICAST_DELEGATE(FOnEditorClose);
	FOnEditorClose OnEditorClose_Delegate;
private:
	void MoveSelectedNodes_Internal(UStateTreeBaseNode* TargetNode, const EScriptStateTreeViewModelInsert RelativeLocation);

private:
	TWeakObjectPtr<UStateAbilityScriptEditorData> EditorDataWeak;
	TSet<TWeakObjectPtr<UStateTreeBaseNode>> SelectedNodes;

	// 目前仅StateNode可以作为RootNode
	UStateTreeStateNode* RootStateNode = nullptr;
};

class SScriptStateTreeView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SScriptStateTreeView) {}
	SLATE_END_ARGS()

	SScriptStateTreeView();
	virtual ~SScriptStateTreeView() override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	void Construct(const FArguments& InArgs, TSharedPtr<FScriptStateTreeViewModel> InScriptStateTreeViewModel);

private:
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	void ReGenerateStateTree();
	void UpdateStateTree();

	TSharedRef<ITableRow> OnGenerateRow(TWeakObjectPtr<UStateTreeBaseNode> InState, const TSharedRef<STableViewBase>& InOwnerTableView);
	void OnGetChildren(TWeakObjectPtr<UStateTreeBaseNode> InParent, TArray<TWeakObjectPtr<UStateTreeBaseNode>>& OutChildren);
	void OnTreeExpansionChanged(TWeakObjectPtr<UStateTreeBaseNode> InSelectedItem, bool bIsExpanded);
	bool HasSelectedNode() const;
	TSharedPtr<SWidget> OnContextMenuOpening();

	// Action handlers
	FReply HandleAddActionNodeButton() { return HandleAddNodeButton(EScriptStateTreeNodeType::Action); }
	FReply HandleAddNodeButton(EScriptStateTreeNodeType NodeType);
	void HandleModelNodeAdded(UStateTreeBaseNode* ParentNode, UStateTreeBaseNode* NewNode);
	void HandleModelNodesMoved(const TSet<UStateTreeBaseNode*>& AffectedParents, const TSet<UStateTreeBaseNode*>& MovedNodes);
	void HandleDetailsViewChangingProperties(UObject* SelectedObject);

	TSharedRef<SWidget> CreateStatesSchemaMenu();
	TSharedRef<SWidget> CreateSharedsSchemaMenu();

	TSharedPtr<FScriptStateTreeViewModel> GetViewModel() { return ScriptStateTreeViewModel; }

private:
	// ViewModel
	TSharedPtr<FScriptStateTreeViewModel> ScriptStateTreeViewModel;
	TSharedPtr<STreeView<TWeakObjectPtr<UStateTreeBaseNode>>> TreeViewWidget;
	TSharedPtr<FUICommandList> CommandList;

	TSharedPtr<SScrollBox> TreeViewScrollBox;
	TArray<TWeakObjectPtr<UStateTreeBaseNode>> TreeRoots;

	bool bItemsDirty;
};


class SScriptStateTreeViewRow : public STableRow<TWeakObjectPtr<UStateTreeBaseNode>>
{
public:

	SLATE_BEGIN_ARGS(SScriptStateTreeViewRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<SScrollBox>& TreeViewScrollBox, 
		TWeakObjectPtr<UStateTreeBaseNode> InNode, TSharedRef<FScriptStateTreeViewModel> InScriptStateTreeViewModel);
	void RequestRename() const;

private:
	FSlateColor GetSelectionNodeColor() const;
	FSlateColor GetActiveNodeColor() const;
	FSlateColor GetTitleColor() const;
	FSlateColor GetErrorColor() const;

	// Slot
	FSlateColor GetSlotColor() const;
	const FSlateBrush* GetSlotIcon() const;
	FText GetSlotName() const;
	
	FText GetNodeTypeTooltip() const;

	// Linked Node
	FText GetLinkedNodeName() const;
	FSlateColor GetLinkedNodeColor() const;
	EVisibility GetLinkedNodeVisibility() const;
	FReply HandleLinkedNodeMouseDoubleClick(const FGeometry&, const FPointerEvent&) const;
	TOptional<EMouseCursor::Type> GetLinkedNodeCursor() const;
	void HandleLinkedNodeLabelTextCommitted(const FText& NewLabel, ETextCommit::Type CommitType) const;
	bool IsLinkedNodeNameReadOnly() const;

	FReply HandleOnSelect(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const;
	FReply HandleDragDetected(const FGeometry&, const FPointerEvent&) const;
	TOptional<EItemDropZone> HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TWeakObjectPtr<UStateTreeBaseNode> TargetNode) const;
	FReply HandleAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TWeakObjectPtr<UStateTreeBaseNode> TargetNode) const;

	bool IsRootNode() const;
private:
	TSharedPtr<FScriptStateTreeViewModel> ScriptStateTreeViewModel;
	TWeakObjectPtr<UStateTreeBaseNode> WeakNode;

	TSharedPtr<SInlineEditableTextBlock> NameTextBlock;
};

class FActionTreeViewDragDrop : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FActionTreeViewDragDrop, FDragDropOperation);

	static TSharedRef<FActionTreeViewDragDrop> New(const UStateTreeBaseNode* InNode)
	{
		return MakeShareable(new FActionTreeViewDragDrop(InNode));
	}

	const UStateTreeBaseNode* GetDraggedNode() const { return Node; }

private:
	FActionTreeViewDragDrop(const UStateTreeBaseNode* InNode)
		: Node(InNode)
	{
	}

	const UStateTreeBaseNode* Node;
};

UENUM()
enum class EStateEventSlotType : uint8
{
	ActionEvent,
	StateEvent,
};

USTRUCT()
struct FStateEventSlotInfo
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere)
	FName EventName = NAME_None;
	UPROPERTY()
	FGuid UID = FGuid::NewGuid();
	UPROPERTY()
	EStateEventSlotType SlotType = EStateEventSlotType::ActionEvent;
};

// Base
UCLASS()
class UStateTreeBaseNode : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="一般")
	FName Name = NAME_None;
	UPROPERTY(VisibleAnywhere, Category = "一般")
	EScriptStateTreeNodeType NodeType = EScriptStateTreeNodeType::None;

	UPROPERTY();
	UStateTreeBaseNode* Parent = nullptr;
	UPROPERTY()
	TArray<TObjectPtr<UStateTreeBaseNode>> Children;
	UPROPERTY()
	bool bIsExpanded = false;
	UPROPERTY()
	FStateEventSlotInfo EventSlotInfo;

public:
	virtual bool CanReleaseNode() {
		return true;
	}
	virtual void GenerateSlotNodes() {
		RemoveChildren();
	}
	virtual void SortSlotNodes() {}
	virtual void Init(TSharedPtr<FScriptStateTreeViewModel> InScriptStateTreeViewModel) {
		GenerateSlotNodes();
		ScriptStateTreeViewModel = InScriptStateTreeViewModel;
	}
	virtual void Release() {
		if (ScriptStateTreeViewModel.IsValid())
		{
			ScriptStateTreeViewModel->OnNodeRelease(this);
		}
		ScriptStateTreeViewModel.Reset();
		
		Parent = nullptr;
		SetFlags(RF_Transient);
		Rename(nullptr, GetTransientPackage());
		MarkAsGarbage();

		RemoveChildren();
	}
	virtual void OnPropertyChanged() {}
	virtual void OnUpdateNode(TSharedRef<FScriptStateTreeViewModel> ScriptStateTreeViewModelRef) {
		// @TODO: 这种方式性能开销可能有点高
		ScriptStateTreeViewModel = ScriptStateTreeViewModelRef;
		for (UStateTreeBaseNode* Child : Children)
		{
			Child->OnUpdateNode(ScriptStateTreeViewModelRef);
		}
	}

	void CopySlotNodeFrom(UStateTreeBaseNode* FromNode)
	{
		Parent = FromNode->Parent;
		Children = FromNode->Children;
		EventSlotInfo = FromNode->EventSlotInfo;
		bIsExpanded = FromNode->bIsExpanded;
	}

	TSharedPtr<FScriptStateTreeViewModel> GetStateTreeViewModel() {
		return ScriptStateTreeViewModel;
	}
private:
	void RemoveChildren()
	{
		for (UStateTreeBaseNode* Child : Children)
		{
			Child->Release();
		}
		Children.Empty();
	}
protected:
	TSharedPtr<FScriptStateTreeViewModel> ScriptStateTreeViewModel;
};

UCLASS()
class UStateTreeActionNode : public UStateTreeBaseNode
{
	GENERATED_BODY()
public:
	virtual void GenerateSlotNodes() override;
	virtual void Init(TSharedPtr<FScriptStateTreeViewModel> InScriptStateTreeViewModel) override;
	virtual void Release() override;
	virtual void OnUpdateNode(TSharedRef<FScriptStateTreeViewModel> ScriptStateTreeViewModelRef) override;
	UPROPERTY()
	TObjectPtr<UEdGraph> EdGraph;					// 存储Action类型的Node
};

UCLASS()
class UStateTreeStateNode : public UStateTreeBaseNode
{
	GENERATED_BODY()
public:
	virtual void GenerateSlotNodes() override;
	virtual void Init(TSharedPtr<FScriptStateTreeViewModel> InScriptStateTreeViewModel) override;
	virtual void OnPropertyChanged() override;
	virtual void Release() override;
	UPROPERTY()
	TObjectPtr<UStateAbilityState> StateInstance;	// 存储State类型的Node
};

UCLASS()
class UStateTreeSharedNode : public UStateTreeBaseNode
{
	GENERATED_BODY()
public:
	virtual void Init(TSharedPtr<FScriptStateTreeViewModel> InScriptStateTreeViewModel) override;
	virtual void OnPropertyChanged() override;
	UPROPERTY()
	TWeakObjectPtr<UStateTreeBaseNode> SharedNode;
};