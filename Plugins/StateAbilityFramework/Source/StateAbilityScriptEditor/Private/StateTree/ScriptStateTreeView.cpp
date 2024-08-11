#include "StateTree/ScriptStateTreeView.h"

#include "Component/StateAbility/StateAbilityState.h"
#include "Component/StateAbility/StateAbilityAction.h"
#include "Component/StateAbility/Script/StateAbilityScript.h"
#include "StateAbilityScriptEditor.h"
#include "StateAbilityScriptEditorData.h"
#include "Editor/StateAbilityEditorStyle.h"
#include "StateTree/ScriptStateTreeSchema.h"

#include "SPositiveActionButton.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Framework/Docking/TabManager.h"

// Graph
#include "Graph/StateAbilityScriptGraph.h"
#include "StateAbilityScriptEdGraphSchema.h"


#define LOCTEXT_NAMESPACE "ScriptStateTreeView"


namespace UE::ScriptStateTree::Editor
{
	// 如果节点是父节点的子节点则返回true。
	bool IsChildNodeOf(const UStateTreeBaseNode* ParentNode, const UStateTreeBaseNode* Node)
	{
		for (const UStateTreeBaseNode* Child : ParentNode->Children)
		{
			if (Child == Node)
			{
				return true;
			}
			if (IsChildNodeOf(Child, Node))
			{
				return true;
			}
		}
		return false;
	}


	// 从传入的Nodes中移除Nodes中的父节点的子节点。
	void RemoveContainedChildren(TArray<UStateTreeBaseNode*>& Nodes)
	{
		TSet<UStateTreeBaseNode*> UniqueNodes;
		for (UStateTreeBaseNode* Node : Nodes)
		{
			UniqueNodes.Add(Node);
		}

		for (int32 i = 0; i < Nodes.Num(); )
		{
			UStateTreeBaseNode* Node = Nodes[i];

			// 遍历父节点，如果当前节点属于任何一个新的父节点，则将其删除。
			UStateTreeBaseNode* NodeParent = Node->Parent;
			bool bShouldRemove = false;
			while (NodeParent)
			{
				if (UniqueNodes.Contains(NodeParent))
				{
					bShouldRemove = true;
					break;
				}
				NodeParent = NodeParent->Parent;
			}

			if (bShouldRemove)
			{
				Nodes.RemoveAt(i);
			}
			else
			{
				i++;
			}
		}
	}

	void ExpandAllNodes(TArray<TObjectPtr<UStateTreeBaseNode>>& Nodes, TSharedPtr<STreeView<TWeakObjectPtr<UStateTreeBaseNode>>> TreeView)
	{
		if (Nodes.IsEmpty())
		{
			return;
		}

		for (TObjectPtr<UStateTreeBaseNode>& Node : Nodes)
		{
			TreeView->SetItemExpansion(Node, true);
			ExpandAllNodes(Node->Children, TreeView);
		}
	}

};

//////////////////////////////////////////////////////////////////////////
// FScriptStateTreeViewModel
FScriptStateTreeViewModel::FScriptStateTreeViewModel(UStateAbilityScriptEditorData* InEditorData)
	: EditorDataWeak(InEditorData)
{

}

FScriptStateTreeViewModel::~FScriptStateTreeViewModel()
{

}

void FScriptStateTreeViewModel::Init(UStateTreeStateNode* InRootStateNode)
{
	RootStateNode = InRootStateNode;
}

void FScriptStateTreeViewModel::PostUndo(bool bSuccess)
{

}

void FScriptStateTreeViewModel::PostRedo(bool bSuccess)
{

}

void FScriptStateTreeViewModel::ClearSelection()
{
	SetSelection(nullptr);
}

void FScriptStateTreeViewModel::ClearSelection(UObject* ClearSelected)
{
	for (auto It = SelectedNodes.CreateIterator(); It; ++It)
	{
		TWeakObjectPtr<UObject> SelectItem = *It;
		if (SelectItem.IsValid() && SelectItem.Get() == ClearSelected)
		{
			It.RemoveCurrent();
		}
	}
}

void FScriptStateTreeViewModel::SetSelection(UStateTreeBaseNode* Selected)
{
	SelectedNodes.Reset();
	TSet<UObject*> SelectedNodesSet;

	if (IsValid(Selected))
	{
		SelectedNodes.Add(Selected);
		SelectedNodesSet.Add(Selected);
	}

	if (!SelectedNodesSet.IsEmpty())
	{
		EditorDataWeak->GetScriptViewModel()->OnStateTreeNodeSelected.Broadcast(SelectedNodesSet);
	}
}

void FScriptStateTreeViewModel::SetSelection(const TArray<TWeakObjectPtr<UStateTreeBaseNode>>& InSelectedNodes)
{
	SelectedNodes.Reset();
	TSet<UObject*> SelectedNodesSet;

	for (const TWeakObjectPtr<UStateTreeBaseNode>& Node : InSelectedNodes)
	{
		if (Node.Get())
		{
			SelectedNodes.Add(Node);
			SelectedNodesSet.Add(Node.Get());
		}
	}

	//TArray<FGuid> SelectedTaskIDArr;
	if (!SelectedNodesSet.IsEmpty())
	{
		EditorDataWeak->GetScriptViewModel()->OnStateTreeNodeSelected.Broadcast(SelectedNodesSet);
	}
}

bool FScriptStateTreeViewModel::IsSelected(const UStateTreeBaseNode* Node) const
{
	const TWeakObjectPtr<UStateTreeBaseNode> WeakNode = const_cast<UStateTreeBaseNode*>(Node);
	return SelectedNodes.Contains(WeakNode);
}

bool FScriptStateTreeViewModel::IsChildOfSelection(const UStateTreeBaseNode* Node) const
{
	for (const TWeakObjectPtr<UStateTreeBaseNode>& WeakSelectedNode : SelectedNodes)
	{
		if (const UStateTreeBaseNode* SelectedNode = Cast<UStateTreeBaseNode>(WeakSelectedNode.Get()))
		{
			if (SelectedNode == Node)
			{
				return true;
			}

			if (UE::ScriptStateTree::Editor::IsChildNodeOf(SelectedNode, Node))
			{
				return true;
			}
		}
	}
	return false;
}

bool FScriptStateTreeViewModel::HasSelection() const
{
	return SelectedNodes.Num() > 0;
}

bool FScriptStateTreeViewModel::HasUniqueName(UStateTreeBaseNode* InNode, bool bIsSameType /* = true */)
{
	bool bIsUnique = true;
	EScriptStateTreeNodeType NodeType = InNode->NodeType;
	FName NodeName = InNode->Name;

	TraversingAllNodes([NodeType, NodeName, InNode = InNode, bIsSameType = bIsSameType, &bIsUnique](UStateTreeBaseNode* Node) {
		if ((Node->NodeType == NodeType || !bIsSameType) && Node != InNode && Node->Name == NodeName)
		{
			bIsUnique = false;
		}
	});

	return bIsUnique;
}

void FScriptStateTreeViewModel::GetSelectedNodes(TArray<UStateTreeBaseNode*>& OutSelectedNodes)
{
	OutSelectedNodes.Reset();
	for (TWeakObjectPtr<UStateTreeBaseNode>& WeakNode : SelectedNodes)
	{
		if (UStateTreeBaseNode* Node = WeakNode.Get())
		{
			OutSelectedNodes.Add(Node);
		}
	}
}

void FScriptStateTreeViewModel::GetSelectedNodes(TArray<TWeakObjectPtr<UStateTreeBaseNode>>& OutSelectedNodes)
{
	OutSelectedNodes.Reset();
	for (TWeakObjectPtr<UStateTreeBaseNode>& WeakNode : SelectedNodes)
	{
		if (WeakNode.Get())
		{
			OutSelectedNodes.Add(WeakNode);
		}
	}
}

void FScriptStateTreeViewModel::DeleteSelectedNode()
{
	// 仅支持单独Selection的删除
	if (SelectedNodes.Num() == 1)
	{
		UStateTreeBaseNode* Selection = GetFirstSelectedNode();

		// 移除所有可能有关的SharedNode
		TraversingAllNodes([Selection, this](UStateTreeBaseNode* Node) {
			if (Node->NodeType == EScriptStateTreeNodeType::Shared)
			{
				UStateTreeSharedNode* Shared = Cast<UStateTreeSharedNode>(Node);
				if (!Shared->SharedNode.IsValid() || Shared->SharedNode.Get() == Selection)
				{
					CreateNewNode(Node, EScriptStateTreeNodeType::Slot, false);
				}
			}
		});

		// 最后移除Selection
		CreateNewNode(Selection, EScriptStateTreeNodeType::Slot, false);
	}

	ClearSelection();
}

UStateTreeBaseNode* FScriptStateTreeViewModel::CreateNewNode(UStateTreeBaseNode* SlotNode, EScriptStateTreeNodeType NodeType, bool bIsDeferredInitialize)
{
	UStateAbilityScriptEditorData* EditorData = EditorDataWeak.Get();
	if (EditorData == nullptr || SlotNode == nullptr)
	{
		return nullptr;
	}

	//const FScopedTransaction Transaction(LOCTEXT("AddStateTransaction", "Add State"));

	UStateTreeBaseNode* NewNode = nullptr;
	switch (NodeType)
	{
	case EScriptStateTreeNodeType::Action: {
		NewNode = NewObject<UStateTreeActionNode>(SlotNode->GetOuter(), FName(), RF_Transactional);
		break;
	}
	case EScriptStateTreeNodeType::State: {
		NewNode = NewObject<UStateTreeStateNode>(SlotNode->GetOuter(), FName(), RF_Transactional);
		break;
	}
	case EScriptStateTreeNodeType::Slot: {
		NewNode = NewObject<UStateTreeBaseNode>(SlotNode->GetOuter(), FName(), RF_Transactional);
		break;
	}
	case EScriptStateTreeNodeType::Shared: {
		NewNode = NewObject<UStateTreeSharedNode>(SlotNode->GetOuter(), FName(), RF_Transactional);
		break;
	}
	}
	NewNode->NodeType = NodeType;
	NewNode->CopySlotNodeFrom(SlotNode);	// 将SlotNode的必要信息继承下来

	UStateTreeBaseNode* ParentNode = SlotNode->Parent;
	if (ParentNode != nullptr)
	{
		ParentNode->Modify();

		for (int32 index = 0; index < ParentNode->Children.Num(); ++index)
		{
			if (ParentNode->Children[index] == SlotNode)
			{
				ParentNode->Children[index] = NewNode;
				break;
			}
		}
	}
	else
	{
		NewNode->Parent = nullptr;
	}

	// 旧的SlotNode已经没用了，开始执行释放回收流程
	SlotNode->Release();

	if (!bIsDeferredInitialize)
	{
		// 非延迟初始化，立即执行
		NewNode->Init(SharedThis(this));
		// 如果是延迟的，我们不应该广播，因为它还未处理完成
		OnNodeAdded(ParentNode, NewNode);
	}

	return NewNode;
}

UStateAbilityScriptArchetype* FScriptStateTreeViewModel::GetScriptArchetype()
{
	if (EditorDataWeak.IsValid())
	{
		return EditorDataWeak.Get()->GetTypedOuter<UStateAbilityScriptArchetype>();
	}

	return nullptr;
}

UStateAbilityScriptEditorData* FScriptStateTreeViewModel::GetScriptEditorData()
{
	if (EditorDataWeak.IsValid())
	{
		return EditorDataWeak.Get();
	}

	return nullptr;
}

TArray<TObjectPtr<UStateTreeBaseNode>>& FScriptStateTreeViewModel::GetTreeRoots()
{
	if (IsValid(RootStateNode))
	{
		return RootStateNode->Children;
	}
	else
	{
		static TArray<TObjectPtr<UStateTreeBaseNode>> EmptyArray;
		EmptyArray.Empty();
		return EmptyArray;
	}
}

void FScriptStateTreeViewModel::RenameNode(UStateTreeBaseNode* Node, FName NewName)
{
	if (Node == nullptr)
	{
		return;
	}

	//const FScopedTransaction Transaction(LOCTEXT("RenameTransaction", "Rename"));
	//State->Modify();
	Node->Name = NewName;

	//TSet<UStateTreeBaseNode*> AffectedStates;
	//AffectedStates.Add(State);

	//FProperty* NameProperty = FindFProperty<FProperty>(UStateTreeBaseNode::StaticClass(), GET_MEMBER_NAME_CHECKED(UStateTreeBaseNode, Name));
	//FPropertyChangedEvent PropertyChangedEvent(NameProperty, EPropertyChangeType::ValueSet);
	//OnStatesChanged.Broadcast(AffectedStates, PropertyChangedEvent);
}

void FScriptStateTreeViewModel::MoveSelectedNodesBefore(UStateTreeBaseNode* TargetNode)
{
	MoveSelectedNodes_Internal(TargetNode, EScriptStateTreeViewModelInsert::Before);
}

void FScriptStateTreeViewModel::MoveSelectedNodesAfter(UStateTreeBaseNode* TargetNode)
{
	MoveSelectedNodes_Internal(TargetNode, EScriptStateTreeViewModelInsert::After);
}

void FScriptStateTreeViewModel::MoveSelectedNodesInto(UStateTreeBaseNode* TargetNode)
{
	MoveSelectedNodes_Internal(TargetNode, EScriptStateTreeViewModelInsert::Into);
}

void FScriptStateTreeViewModel::MoveSelectedNodes_Internal(UStateTreeBaseNode* TargetNode, const EScriptStateTreeViewModelInsert RelativeLocation)
{
	//UStateAbilityScriptEditorData* EditorData = EditorDataWeak.Get();
	//if (EditorData == nullptr || TargetNode == nullptr)
	//{
	//	return;
	//}

	//TArray<UStateTreeBaseNode*> Nodes;
	//GetSelectedNodes(Nodes);
	//
	//// 仅保留选择中的相对父节点，因为选择的子节点会跟随父节点。
	//UE::ScriptStateTree::Editor::RemoveContainedChildren(Nodes);
	//
	//// 删除包含目标节点的子节点。
	//Nodes.RemoveAll([TargetNode](const UStateTreeBaseNode* Node)
	//	{
	//		return UE::ScriptStateTree::Editor::IsChildNodeOf(Node, TargetNode);
	//	});
	//
	//if (Nodes.Num() > 0 && TargetNode != nullptr)
	//{
	//	//const FScopedTransaction Transaction(LOCTEXT("MoveTransaction", "Move"));
	//
	//	TSet<UStateTreeBaseNode*> AffectedParents;
	//	TSet<UStateTreeBaseNode*> AffectedNodes;
	//
	//	UStateTreeBaseNode* TargetParent = TargetNode->Parent;
	//	if (RelativeLocation == EScriptStateTreeViewModelInsert::Into)
	//	{
	//		AffectedParents.Add(TargetNode);
	//	}
	//	else
	//	{
	//		AffectedParents.Add(TargetParent);
	//	}
	//
	//	for (int32 i = Nodes.Num() - 1; i >= 0; i--)
	//	{
	//		if (UStateTreeBaseNode* Node = Nodes[i])
	//		{
	//			Node->Modify();
	//			if (Node->Parent)
	//			{
	//				AffectedParents.Add(Node->Parent);
	//			}
	//		}
	//	}
	//
	//	if (RelativeLocation == EScriptStateTreeViewModelInsert::Into)
	//	{
	//		// Move into
	//		TargetNode->Modify();
	//	}
	//
	//	for (UStateTreeBaseNode* Parent : AffectedParents)
	//	{
	//		if (Parent)
	//		{
	//			Parent->Modify();
	//		}
	//		else
	//		{
	//			EditorData->Modify();
	//		}
	//	}
	//
	//	// 以相反的顺序添加，以保持原来的顺序。
	//	for (int32 i = Nodes.Num() - 1; i >= 0; i--)
	//	{
	//		if (UStateTreeBaseNode* SelectedNode = Nodes[i])
	//		{
	//			AffectedNodes.Add(SelectedNode);
	//
	//			UStateTreeBaseNode* SelectedParent = SelectedNode->Parent;
	//
	//			// Remove from current parent
	//			TArray<TObjectPtr<UStateTreeBaseNode>>& ArrayToRemoveFrom = SelectedParent ? SelectedParent->Children : EditorData->SubTrees;
	//			const int32 ItemIndex = ArrayToRemoveFrom.Find(SelectedNode);
	//			if (ItemIndex != INDEX_NONE)
	//			{
	//				ArrayToRemoveFrom.RemoveAt(ItemIndex);
	//				SelectedNode->Parent = nullptr;
	//			}
	//
	//			// Insert to new parent
	//			if (RelativeLocation == EScriptStateTreeViewModelInsert::Into)
	//			{
	//				// Into
	//				// TargetNode->Children.Insert(SelectedNode, /*Index*/0);
	//				TargetNode->Children.Add(SelectedNode);
	//				SelectedNode->Parent = TargetNode;
	//			}
	//			else
	//			{
	//				TArray<TObjectPtr<UStateTreeBaseNode>>& ArrayToMoveTo = TargetParent ? TargetParent->Children : EditorData->SubTrees;
	//				const int32 TargetIndex = ArrayToMoveTo.Find(TargetNode);
	//				if (TargetIndex != INDEX_NONE)
	//				{
	//					if (RelativeLocation == EScriptStateTreeViewModelInsert::Before)
	//					{
	//						// Before
	//						ArrayToMoveTo.Insert(SelectedNode, TargetIndex);
	//						SelectedNode->Parent = TargetParent;
	//					}
	//					else if (RelativeLocation == EScriptStateTreeViewModelInsert::After)
	//					{
	//						// After
	//						ArrayToMoveTo.Insert(SelectedNode, TargetIndex + 1);
	//						SelectedNode->Parent = TargetParent;
	//					}
	//				}
	//				else
	//				{
	//					// Fallback, should never happen.
	//					ensureMsgf(false, TEXT("%s: Failed to find specified target state %s on state %s while moving a state."), *GetNameSafe(EditorData->GetOuter()), *GetNameSafe(TargetNode), *GetNameSafe(SelectedParent));
	//					ArrayToMoveTo.Add(SelectedNode);
	//					SelectedNode->Parent = TargetParent;
	//				}
	//			}
	//		}
	//	}
	//
	//	OnNodesMoved.Broadcast(AffectedParents, AffectedNodes);
	//
	//	TArray<TWeakObjectPtr<UStateTreeBaseNode>> WeakNodes;
	//	for (UStateTreeBaseNode* Node : Nodes)
	//	{
	//		WeakNodes.Add(Node);
	//	}
	//
	//	SetSelection(WeakNodes);
	//}
}

void FScriptStateTreeViewModel::TraversingAllNodes(TFunction<void(UStateTreeBaseNode*)> InProcessor)
{
	for (TObjectPtr<UStateTreeBaseNode> RootNode : GetTreeRoots())
	{
		TraversingAllNodesRecursive(RootNode, InProcessor);
	}
}

void FScriptStateTreeViewModel::TraversingAllNodesRecursive(UStateTreeBaseNode* ParentNode, TFunction<void(UStateTreeBaseNode*)> InProcessor)
{
	if (ParentNode == nullptr)
	{
		return;
	}
	
	InProcessor(ParentNode);

	for (auto Child : ParentNode->Children)
	{
		TraversingAllNodesRecursive(Child, InProcessor);
	}
}

void FScriptStateTreeViewModel::OnNodeAdded(UStateTreeBaseNode* ParentNode, UStateTreeBaseNode* NewNode)
{
	OnNodeAdded_Delegate.Broadcast(ParentNode, NewNode);
}

void FScriptStateTreeViewModel::OnNodesMoved(const TSet<UStateTreeBaseNode*>& AffectedParents, const TSet<UStateTreeBaseNode*>& MovedNodes)
{
	OnNodesMoved_Delegate.Broadcast(AffectedParents, MovedNodes);
}

void FScriptStateTreeViewModel::OnNodeRelease(UStateTreeBaseNode* OldNode)
{
	OnNodeRelease_Delegate.Broadcast(OldNode);
}

void FScriptStateTreeViewModel::OnSpawnNodeGraph(UEdGraph* NewGraph)
{
	OnSpawnNodeGraph_Delegate.Broadcast(NewGraph);
}

void FScriptStateTreeViewModel::OnDetailsViewChangingProperties(UObject* SelectedObject)
{
	OnDetailsViewChangingProperties_Delegate.Broadcast(SelectedObject);
}

void FScriptStateTreeViewModel::UpdateStateTree()
{
	OnUpdateStateTree_Delegate.Broadcast();
}

void FScriptStateTreeViewModel::UpdateStateNode()
{
	OnUpdateStateNode_Delegate.Broadcast();
}

void FScriptStateTreeViewModel::UpdateStateNodePin()
{
	OnUpdateStateNodePin_Delegate.Broadcast();
	OnUpdateStateTree_Delegate.Broadcast();
}

//////////////////////////////////////////////////////////////////////////
// SScriptStateTreeView

SScriptStateTreeView::SScriptStateTreeView()
	: bItemsDirty(false)
{

}

SScriptStateTreeView::~SScriptStateTreeView()
{
	if (ScriptStateTreeViewModel.IsValid())
	{
		ScriptStateTreeViewModel->OnNodeAdded_Delegate.RemoveAll(this);
		ScriptStateTreeViewModel->OnNodesMoved_Delegate.RemoveAll(this);
		ScriptStateTreeViewModel->OnDetailsViewChangingProperties_Delegate.RemoveAll(this);
		ScriptStateTreeViewModel->OnUpdateStateTree_Delegate.RemoveAll(this);
	}
}

FReply SScriptStateTreeView::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		ScriptStateTreeViewModel->ClearSelection();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SScriptStateTreeView::Construct(const FArguments& InArgs, TSharedPtr<FScriptStateTreeViewModel> InScriptStateTreeViewModel)
{
	ScriptStateTreeViewModel = InScriptStateTreeViewModel;
	CommandList = ScriptStateTreeViewModel->GetScriptEditorData()->GetScriptViewModel()->ToolkitCommands;

	ScriptStateTreeViewModel->OnNodeAdded_Delegate.AddSP(this, &SScriptStateTreeView::HandleModelNodeAdded);
	ScriptStateTreeViewModel->OnNodesMoved_Delegate.AddSP(this, &SScriptStateTreeView::HandleModelNodesMoved);
	ScriptStateTreeViewModel->OnDetailsViewChangingProperties_Delegate.AddSP(this, &SScriptStateTreeView::HandleDetailsViewChangingProperties);
	ScriptStateTreeViewModel->OnUpdateStateTree_Delegate.AddSP(this, &SScriptStateTreeView::UpdateStateTree);

	TreeRoots.Empty();
	TreeRoots.Append(ScriptStateTreeViewModel->GetTreeRoots());


	TSharedRef<SScrollBar> HorizontalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Horizontal)
		.Thickness(FVector2D(12.0f, 12.0f));

	TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.Thickness(FVector2D(12.0f, 12.0f));

	TreeViewWidget = SNew(STreeView<TWeakObjectPtr<UStateTreeBaseNode>>)
		.OnGenerateRow(this, &SScriptStateTreeView::OnGenerateRow)
		.OnGetChildren(this, &SScriptStateTreeView::OnGetChildren)
		.TreeItemsSource(&TreeRoots)
		.ItemHeight(32)
		.OnExpansionChanged(this, &SScriptStateTreeView::OnTreeExpansionChanged)
		.OnContextMenuOpening(this, &SScriptStateTreeView::OnContextMenuOpening)
		.AllowOverscroll(EAllowOverscroll::No)
		.SelectionMode(ESelectionMode::None)
		.ExternalScrollbar(VerticalScrollBar);

		ChildSlot
		[
			SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoHeight()
				[
					SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.Padding(2.0f)
						[
							SNew(SHorizontalBox)

								// New Action Node
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.Padding(4.0f, 2.0f)
								.AutoWidth()
								[
									SNew(SPositiveActionButton)
									.ToolTipText(LOCTEXT("AddNodeToolTip", "Add New Action Node"))
									.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
									.Text(LOCTEXT("AddNode", "Add Action"))
									.OnClicked(this, &SScriptStateTreeView::HandleAddActionNodeButton)
									.IsEnabled(this, &SScriptStateTreeView::HasSelectedNode)
								]
								// New State Node
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.Padding(4.0f, 2.0f)
								.AutoWidth()
								[
									SNew(SPositiveActionButton)
									.ToolTipText(LOCTEXT("AddNodeToolTip", "Add New State Node"))
									.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
									.Text(LOCTEXT("AddNode", "Add State"))
									.OnGetMenuContent(this, &SScriptStateTreeView::CreateStatesSchemaMenu)
									.IsEnabled(this, &SScriptStateTreeView::HasSelectedNode)
								]
								// New Shared Node
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.Padding(4.0f, 2.0f)
								.AutoWidth()
								[
									SNew(SPositiveActionButton)
									.ToolTipText(LOCTEXT("AddNodeToolTip", "Add New Shared Node"))
									.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
									.Text(LOCTEXT("AddShared", "Add Shared"))
									.OnGetMenuContent(this, &SScriptStateTreeView::CreateSharedsSchemaMenu)
									.IsEnabled(this, &SScriptStateTreeView::HasSelectedNode)
								]
						]
				]

				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.FillHeight(1.0f)
				.Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					.FillHeight(1.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.Padding(0.0f)
						[
							SAssignNew(TreeViewScrollBox, SScrollBox)
							.Orientation(Orient_Horizontal)
							.ExternalScrollbar(HorizontalScrollBar)
							+ SScrollBox::Slot()
							.FillSize(1.0f)
							[
								TreeViewWidget.ToSharedRef()
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							VerticalScrollBar
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						HorizontalScrollBar
					]
				]
		];

	ReGenerateStateTree();

	UpdateStateTree();
	//CommandList = InCommandList;
	//BindCommands();
}

FReply SScriptStateTreeView::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	else
	{
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}
}

void SScriptStateTreeView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (bItemsDirty)
	{
		ReGenerateStateTree();
	}

	//if (RequestedRenameState && !TreeViewWidget->IsPendingRefresh())
	//{
	//	if (TSharedPtr<SScriptStateTreeViewRow> Row = StaticCastSharedPtr<SScriptStateTreeViewRow>(TreeViewWidget->WidgetFromItem(RequestedRenameState)))
	//	{
	//		Row->RequestRename();
	//	}
	//	RequestedRenameState = nullptr;
	//}
}

void SScriptStateTreeView::ReGenerateStateTree()
{
	if (!ScriptStateTreeViewModel)
	{
		return;
	}

	//TSet<TWeakObjectPtr<UStateTreeBaseNode>> ExpandedNodes;
	//if (bExpandPersistent)
	//{
	//	// Get expanded Node from the tree data.
	//	ScriptStateTreeViewModel->GetPersistentExpandedStates(ExpandedNodes);
	//}
	//else
	//{
	//	// Restore current expanded state.
	//	TreeViewWidget->GetExpandedItems(ExpandedNodes);
	//}

	// Remember selection
	TArray<TWeakObjectPtr<UStateTreeBaseNode>> SelectedNodes;
	ScriptStateTreeViewModel->GetSelectedNodes(SelectedNodes);

	// Regenerate items
	TreeRoots.Empty();
	TreeRoots.Append(ScriptStateTreeViewModel->GetTreeRoots());
	TreeViewWidget->SetTreeItemsSource(&TreeRoots);

	if (!TreeRoots.IsEmpty())
	{
		// Expanded All Node
		TSharedPtr<STreeView<TWeakObjectPtr<UStateTreeBaseNode>>> TreeView = TreeViewWidget;

		TreeViewWidget->SetItemExpansion(TreeRoots[0], true);
		UE::ScriptStateTree::Editor::ExpandAllNodes(TreeRoots[0]->Children, TreeView);
	}

	// Restore selected Node
	TreeViewWidget->ClearSelection();
	TreeViewWidget->SetItemSelection(SelectedNodes, true);

	TreeViewWidget->RebuildList();

	bItemsDirty = false;
}

void SScriptStateTreeView::UpdateStateTree()
{
	for (TWeakObjectPtr<UStateTreeBaseNode>& WeakRoot : TreeRoots)
	{
		if (WeakRoot.IsValid())
		{
			WeakRoot->OnUpdateNode(ScriptStateTreeViewModel.ToSharedRef());
		}
	}

	bItemsDirty = true;
}

void SScriptStateTreeView::OnGetChildren(TWeakObjectPtr<UStateTreeBaseNode> InParent, TArray<TWeakObjectPtr<UStateTreeBaseNode>>& OutChildren)
{
	if (const UStateTreeBaseNode* Parent = InParent.Get())
	{
		OutChildren.Append(Parent->Children);
	}
}

TSharedRef<ITableRow> SScriptStateTreeView::OnGenerateRow(TWeakObjectPtr<UStateTreeBaseNode> InNode, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	return SNew(SScriptStateTreeViewRow, InOwnerTableView, TreeViewScrollBox, InNode, ScriptStateTreeViewModel.ToSharedRef());
}

void SScriptStateTreeView::OnTreeExpansionChanged(TWeakObjectPtr<UStateTreeBaseNode> InSelectedItem, bool bIsExpanded)
{
	// 不对Node调用Modify()，因为我们不想让扩展弄脏资产。
	if (UStateTreeBaseNode* Node = InSelectedItem.Get())
	{
		Node->bIsExpanded = bIsExpanded;
	}
}

TSharedPtr<SWidget> SScriptStateTreeView::OnContextMenuOpening()
{
	if (!ScriptStateTreeViewModel.IsValid())
	{
		return nullptr;
	}

	FMenuBuilder MenuBuilder(true, CommandList);

	TArray<TWeakObjectPtr<UStateTreeBaseNode>> SelectNodes;
	ScriptStateTreeViewModel->GetSelectedNodes(SelectNodes);


	// 切换State类型
	//if (SelectNodes.Num() == 1 && SelectNodes[0]->NodeType == EScriptStateTreeNodeType::State)
	//{
	//	UStateTreeBaseNode* StateNode = SelectNodes[0].Get();
	//	UStateTreeBaseNode* ParentNode = StateNode->Parent;

	//}

	//...

	return MenuBuilder.MakeWidget();
}

FReply SScriptStateTreeView::HandleAddNodeButton(EScriptStateTreeNodeType NodeType)
{
	if (ScriptStateTreeViewModel == nullptr)
	{
		return FReply::Handled();
	}

	TArray<UStateTreeBaseNode*> SelectedNodes;
	ScriptStateTreeViewModel->GetSelectedNodes(SelectedNodes);
	UStateTreeBaseNode* FirstSelectedNode = SelectedNodes.Num() > 0 ? SelectedNodes[0] : nullptr;

	if (FirstSelectedNode != nullptr)
	{
		switch (NodeType)
		{
		case EScriptStateTreeNodeType::Action:
		{
			ScriptStateTreeViewModel->CreateNewNode(FirstSelectedNode, NodeType);
			TreeViewWidget->SetItemExpansion(FirstSelectedNode, true);
			break;
		}
		}
	}
	else
	{
		ScriptStateTreeViewModel->CreateNewNode(nullptr, NodeType);
	}

	return FReply::Handled();
}

void SScriptStateTreeView::HandleModelNodeAdded(UStateTreeBaseNode* ParentNode, UStateTreeBaseNode* NewNode)
{
	bItemsDirty = true;

	// Request to rename the state immediately.
	// RequestedRenameState = NewState;

	if (ScriptStateTreeViewModel.IsValid())
	{
		ScriptStateTreeViewModel->SetSelection(NewNode);
	}
}

void SScriptStateTreeView::HandleModelNodesMoved(const TSet<UStateTreeBaseNode*>& AffectedParents, const TSet<UStateTreeBaseNode*>& MovedNodes)
{
	bItemsDirty = true;
}

void SScriptStateTreeView::HandleDetailsViewChangingProperties(UObject* SelectedObject)
{
	bItemsDirty = true;
}

bool SScriptStateTreeView::HasSelectedNode() const
{
	if (ScriptStateTreeViewModel.IsValid())
	{
		return ScriptStateTreeViewModel->HasSelection();
	}
	return false;
}

TSharedRef<SWidget> SScriptStateTreeView::CreateStatesSchemaMenu()
{
	/*FMenuBuilder MenuBuilder(true, CommandList);

	MenuBuilder.AddSubMenu(
		"Select State",
		LOCTEXT("SelectState", "Select State..."),
		LOCTEXT("SelectStateTooltip", "Select new State as a LinkedStateNode"),
		FNewMenuDelegate::CreateUObject(this, &SScriptStateTreeView::CreateSelectStateSubMenu)
	);


	return MenuBuilder.MakeWidget();*/
	if (!HasSelectedNode())
	{
		return SNullWidget::NullWidget;
	}

	TArray<UStateTreeBaseNode*> SelectedNodes;
	ScriptStateTreeViewModel->GetSelectedNodes(SelectedNodes);
	UStateTreeBaseNode* FirstSelectedNode = SelectedNodes.Num() > 0 ? SelectedNodes[0] : nullptr;

	return SNew(SScriptStateTreeSchemaStateMenu, FirstSelectedNode, ScriptStateTreeViewModel);
}

TSharedRef<SWidget> SScriptStateTreeView::CreateSharedsSchemaMenu()
{
	/*FMenuBuilder MenuBuilder(true, CommandList);

	MenuBuilder.AddSubMenu(
		"Select State",
		LOCTEXT("SelectState", "Select State..."),
		LOCTEXT("SelectStateTooltip", "Select new State as a LinkedStateNode"),
		FNewMenuDelegate::CreateUObject(this, &SScriptStateTreeView::CreateSelectStateSubMenu)
	);


	return MenuBuilder.MakeWidget();*/
	if (!HasSelectedNode())
	{
		return SNullWidget::NullWidget;
	}

	TArray<UStateTreeBaseNode*> SelectedNodes;
	ScriptStateTreeViewModel->GetSelectedNodes(SelectedNodes);
	UStateTreeBaseNode* FirstSelectedNode = SelectedNodes.Num() > 0 ? SelectedNodes[0] : nullptr;

	return SNew(SScriptStateTreeSchemaSharedMenu, FirstSelectedNode, ScriptStateTreeViewModel);
}

//void SScriptStateTreeView::CreateSelectStateSubMenu()
//{
//	//TSharedRef<SGraphEditorActionMenuAI> Widget =
//	//	SNew(SGraphEditorActionMenuAI)
//	//	.GraphObj(Graph)
//	//	.GraphNode((UBehaviorTreeGraphNode*)this)
//	//	.SubNodeFlags(ESubNode::Decorator)
//	//	.AutoExpandActionMenu(true);
//
//	//FToolMenuSection& Section = Menu->FindOrAddSection("Section");
//	//Section.AddEntry(FToolMenuEntry::InitWidget("DecoratorWidget", Widget, FText(), true));
//
//	//////////////////////////////////////////////////////////////////////////
//
//
//	TArray<UStateTreeBaseNode*> SelectedNodes;
//	ScriptStateTreeViewModel->GetSelectedNodes(SelectedNodes);
//	UStateTreeBaseNode* FirstSelectedNode = SelectedNodes.Num() > 0 ? SelectedNodes[0] : nullptr;
//
//	if (!FirstSelectedNode)
//	{
//		return;
//	}
//
//	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ScriptStateTreeView", "State"));
//	MenuBuilder.AddWidget(SNew(SScriptStateTreeSchemaMenu, FirstSelectedNode, ScriptStateTreeViewModel));
//	MenuBuilder.EndSection();
//}

//////////////////////////////////////////////////////////////////////////
// SScriptStateTreeViewRow

void SScriptStateTreeViewRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<SScrollBox>& TreeViewScrollBox, TWeakObjectPtr<UStateTreeBaseNode> InNode, TSharedRef<FScriptStateTreeViewModel> InScriptStateTreeViewModel)
{
	ScriptStateTreeViewModel = InScriptStateTreeViewModel;
	WeakNode = InNode;
	InNode->OnPropertyChanged();

	const UStateTreeBaseNode* Node = InNode.Get();

	ConstructInternal(STableRow::FArguments()
		.Padding(5.0f)
		.OnDragDetected(this, &SScriptStateTreeViewRow::HandleDragDetected)
		.OnCanAcceptDrop(this, &SScriptStateTreeViewRow::HandleCanAcceptDrop)
		.OnAcceptDrop(this, &SScriptStateTreeViewRow::HandleAcceptDrop)
		//.Style(&FStateAbilityEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("StateTree.Selection"))
		, InOwnerTableView);

	static const FLinearColor LinkBackground = FLinearColor(FColor(84, 84, 84));
	static constexpr FLinearColor IconTint = FLinearColor(1, 1, 1, 0.5f);


	this->ChildSlot
	.HAlign(HAlign_Fill)
	[
		SNew(SBorder)
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(this, &SScriptStateTreeViewRow::GetActiveNodeColor)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SExpanderArrow, SharedThis(this))
				.ShouldDrawWires(true)
				.IndentAmount(32)
				.BaseIndentLevel(0)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Left)
			.Padding(FMargin(0.0f, 4.0f))
			.AutoWidth()
			[
				SNew(SBox)
				.HeightOverride(28.0f)
				.VAlign(VAlign_Fill)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor(this, &SScriptStateTreeViewRow::GetSelectionNodeColor)
					.OnMouseButtonDown(this, &SScriptStateTreeViewRow::HandleOnSelect)
					.Padding(FMargin(4.0f, 0.0f))
					.Cursor(EMouseCursor::Hand)
					[
						SNew(SHorizontalBox)
						// slot maker
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SBox)
							.MinDesiredWidth(4.0f)
							.MinDesiredHeight(28.0f)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
								.BorderBackgroundColor(this, &SScriptStateTreeViewRow::GetSlotColor)
								.Padding(FMargin(4.0f, 0.0f))
								[
									SNew(SHorizontalBox)
									// Slot
									+ SHorizontalBox::Slot()
									.VAlign(VAlign_Center)
									.AutoWidth()
									[
										SNew(SImage)
										.Image(this, &SScriptStateTreeViewRow::GetSlotIcon)
									]
									+ SHorizontalBox::Slot()
									.VAlign(VAlign_Center)
									.AutoWidth()
									.Padding(FMargin(4.0f, 0.0f))
									[
										SNew(STextBlock)
										.Text(this, &SScriptStateTreeViewRow::GetSlotName)
									]
								]
							]
						]
						// Linked Box
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SBox)
							.MinDesiredHeight(28.0f)
							.HAlign(HAlign_Fill)
							.Visibility(this, &SScriptStateTreeViewRow::GetLinkedNodeVisibility)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
								.BorderBackgroundColor(this, &SScriptStateTreeViewRow::GetLinkedNodeColor)
								.Padding(FMargin(4.0f, 0.0f))
								.OnMouseDoubleClick(this, &SScriptStateTreeViewRow::HandleLinkedNodeMouseDoubleClick)
								.Cursor(this, &SScriptStateTreeViewRow::GetLinkedNodeCursor)
								[
									SNew(SHorizontalBox)
									// Linked Node Name
									+ SHorizontalBox::Slot()
									.VAlign(VAlign_Center)
									.AutoWidth()
									[
										SAssignNew(NameTextBlock, SInlineEditableTextBlock)
										.Style(FStateAbilityEditorStyle::Get(), "StateAbilityEditor.State.TitleInlineEditableText")
										.OnVerifyTextChanged_Lambda([](const FText& NewLabel, FText& OutErrorMessage)
										{
											return !NewLabel.IsEmptyOrWhitespace();
										})
										.OnTextCommitted(this, &SScriptStateTreeViewRow::HandleLinkedNodeLabelTextCommitted)
										.Text(this, &SScriptStateTreeViewRow::GetLinkedNodeName)
										.ToolTipText(this, &SScriptStateTreeViewRow::GetNodeTypeTooltip)
										.Clipping(EWidgetClipping::ClipToBounds)
										.IsReadOnly(this, &SScriptStateTreeViewRow::IsLinkedNodeNameReadOnly)
									]
								]
							]
						]
					// State/Action Name
					//+ SHorizontalBox::Slot()
					//.VAlign(VAlign_Fill)
					//.AutoWidth()
					//[
					//	SNew(SBox)
					//	.VAlign(VAlign_Fill)
					//	.Visibility(this, &SScriptStateTreeViewRow::GetNodeVisibility)
					//	[
					//		CreateTasksWidget()
					//	]
					//]
					]
				]
			]
		]
	];
}

void SScriptStateTreeViewRow::RequestRename() const
{
	if (NameTextBlock)
	{
		NameTextBlock->EnterEditingMode();
	}
}

FSlateColor SScriptStateTreeViewRow::GetSelectionNodeColor() const
{
	if (!ScriptStateTreeViewModel.IsValid())
	{
		return FLinearColor::Transparent;
	}

	UStateTreeBaseNode* SelectedNode = ScriptStateTreeViewModel->GetFirstSelectedNode();

	if (!SelectedNode)
	{
		return FLinearColor::Transparent;
	}

	if (UStateTreeBaseNode* Node = WeakNode.Get())
	{
		/*if (ScriptStateTreeViewModel && ScriptStateTreeViewModel->IsStateActiveInDebugger(*State))
		{
			return FLinearColor::Yellow;
		}*/
		if (SelectedNode->NodeType == EScriptStateTreeNodeType::Shared)
		{
			UStateTreeSharedNode* Shared = Cast<UStateTreeSharedNode>(SelectedNode);
			if (!Shared->SharedNode.IsValid())
			{
				return FLinearColor::Transparent;
			}
			// 选中了的是共享节点，且自身就是被共享的目标
			if (Shared->SharedNode.Get() == Node)
			{
				return FLinearColor(FColor(5, 196, 107));
			}
			// 选中了的是共享节点，且自身就是共享节点
			else if (SelectedNode == Node)
			{
				return FLinearColor(FColor(243, 104, 244));
			}
		}
		else if (Node->NodeType == EScriptStateTreeNodeType::Shared)
		{
			UStateTreeSharedNode* Shared = Cast<UStateTreeSharedNode>(Node);
			if (!Shared->SharedNode.IsValid())
			{
				return FLinearColor::Transparent;
			}
			// 本身是共享节点，且共享目标被选中
			if (Shared->SharedNode.Get() == SelectedNode)
			{
				return FLinearColor(FColor(243, 104, 244));
			}
		}
		else if (Node == SelectedNode)
		{
			return FLinearColor(FColor(5, 196, 107));
		}
	}

	return FLinearColor::Transparent;
}

FSlateColor SScriptStateTreeViewRow::GetActiveNodeColor() const
{
	auto CheckStateNode = [ScriptStateTreeViewModel = ScriptStateTreeViewModel](UStateTreeBaseNode* Node) {
		UStateTreeStateNode* StateNode = Cast<UStateTreeStateNode>(Node);
		//if (ScriptStateTreeViewModel->IsActiveState(StateNode->StateInstance->UniqueID))
		//{
		//	return FLinearColor(FColor(204, 174, 98));
		//}
		return FLinearColor::Transparent;
	};


	if (ScriptStateTreeViewModel.IsValid() && WeakNode.IsValid())
	{
		UStateTreeBaseNode* Node = WeakNode.Get();

		if (Node->NodeType == EScriptStateTreeNodeType::State)
		{
			return CheckStateNode(Node);
		}
		else if (Node->NodeType == EScriptStateTreeNodeType::Shared)
		{
			UStateTreeSharedNode* Shared = Cast<UStateTreeSharedNode>(Node);
			UStateTreeBaseNode* SharedNode = Shared->SharedNode.Get();
			if (!SharedNode)
			{
				return FLinearColor::Transparent;
			}
			if (SharedNode->NodeType == EScriptStateTreeNodeType::State)
			{
				return CheckStateNode(SharedNode);
			}
		}
	}

	return FLinearColor::Transparent;
}

FSlateColor SScriptStateTreeViewRow::GetTitleColor() const
{
	if (const UStateTreeBaseNode* Node = WeakNode.Get())
	{
		if (IsRootNode())
		{
			return FLinearColor(FColor(17, 117, 131));
		}
	}

	return FLinearColor(FColor(31, 151, 167));
}

FSlateColor SScriptStateTreeViewRow::GetSlotColor() const
{
	return FLinearColor(FColor(75, 75, 75));
}

FSlateColor SScriptStateTreeViewRow::GetErrorColor() const
{
	return FLinearColor(FColor(255, 56, 56));
}

const FSlateBrush* SScriptStateTreeViewRow::GetSlotIcon() const
{
	if (const UStateTreeBaseNode* Node = WeakNode.Get())
	{
		if (IsRootNode())
		{
			return FStateAbilityEditorStyle::Get().GetBrush("StateAbilityEditor.SlotRoot");
		}
		if (Node->NodeType == EScriptStateTreeNodeType::Slot)
		{
			return FStateAbilityEditorStyle::Get().GetBrush("StateAbilityEditor.SlotNormal");
		}
		else if (Node->NodeType == EScriptStateTreeNodeType::Shared)
		{
			return FStateAbilityEditorStyle::Get().GetBrush("StateAbilityEditor.SharedNode");
		}
		else if (Node->NodeType == EScriptStateTreeNodeType::State && Cast<UStateTreeStateNode>(Node)->StateInstance->bIsPersistent)
		{
			return FStateAbilityEditorStyle::Get().GetBrush("StateAbilityEditor.PersistentState");
		}
		else
		{
			return FStateAbilityEditorStyle::Get().GetBrush("StateAbilityEditor.SlotLinked");
		}
	}

	return FStateAbilityEditorStyle::Get().GetBrush("StateAbilityEditor.Error");
}

FText SScriptStateTreeViewRow::GetSlotName() const
{
	if (const UStateTreeBaseNode* Node = WeakNode.Get())
	{
		return FText::FromName(Node->EventSlotInfo.EventName);
	}

	return FText();
}

FText SScriptStateTreeViewRow::GetNodeTypeTooltip() const
{
	if (const UStateTreeBaseNode* Node = WeakNode.Get())
	{
		const UEnum* Enum = StaticEnum<EScriptStateTreeNodeType>();
		check(Enum);
		const int32 Index = Enum->GetIndexByValue((int64)Node->NodeType);
		return Enum->GetToolTipTextByIndex(Index);
	}

	return FText::GetEmpty();
}

bool SScriptStateTreeViewRow::IsRootNode() const
{
	// Routines can be identified by not having parent state.
	const UStateTreeBaseNode* Node = WeakNode.Get();
	return Node ? Node->Parent == nullptr : false;
}

FReply SScriptStateTreeViewRow::HandleOnSelect(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		ScriptStateTreeViewModel->SetSelection(WeakNode.Get());

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SScriptStateTreeViewRow::HandleDragDetected(const FGeometry&, const FPointerEvent&) const
{
	return FReply::Handled().BeginDragDrop(FActionTreeViewDragDrop::New(WeakNode.Get()));
}

TOptional<EItemDropZone> SScriptStateTreeViewRow::HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TWeakObjectPtr<UStateTreeBaseNode> TargetNode) const
{
	const TSharedPtr<FActionTreeViewDragDrop> DragDropOperation = DragDropEvent.GetOperationAs<FActionTreeViewDragDrop>();
	if (DragDropOperation.IsValid())
	{
		// Cannot drop on selection or child of selection.
		if (ScriptStateTreeViewModel && ScriptStateTreeViewModel->IsChildOfSelection(TargetNode.Get()))
		{
			return TOptional<EItemDropZone>();
		}

		return DropZone;
	}

	return TOptional<EItemDropZone>();
}

FReply SScriptStateTreeViewRow::HandleAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TWeakObjectPtr<UStateTreeBaseNode> TargetNode) const
{
	const TSharedPtr<FActionTreeViewDragDrop> DragDropOperation = DragDropEvent.GetOperationAs<FActionTreeViewDragDrop>();
	if (DragDropOperation.IsValid())
	{
		if (ScriptStateTreeViewModel.IsValid())
		{
			if (DropZone == EItemDropZone::AboveItem)
			{
				ScriptStateTreeViewModel->MoveSelectedNodesBefore(TargetNode.Get());
			}
			else if (DropZone == EItemDropZone::BelowItem)
			{
				ScriptStateTreeViewModel->MoveSelectedNodesAfter(TargetNode.Get());
			}
			else
			{
				ScriptStateTreeViewModel->MoveSelectedNodesInto(TargetNode.Get());
			}

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

/************************************************************************/
/* Linked Node															*/
/************************************************************************/
FText SScriptStateTreeViewRow::GetLinkedNodeName() const
{
	if (const UStateTreeBaseNode* Node = WeakNode.Get())
	{
		return FText::FromName(Node->Name);
	}
	return FText::FromName(FName());
}

FSlateColor SScriptStateTreeViewRow::GetLinkedNodeColor() const
{
	if (const UStateTreeBaseNode* Node = WeakNode.Get())
	{
		static const FLinearColor StateColor(FColor(0, 98, 102));
		static const FLinearColor ActionColor(FColor(61, 61, 61));

		if (Node->NodeType == EScriptStateTreeNodeType::State)
		{
			return StateColor;
		}
		if (Node->NodeType == EScriptStateTreeNodeType::Action)
		{
			return ActionColor;
		}
		if (Node->NodeType == EScriptStateTreeNodeType::Shared)
		{
			UStateTreeSharedNode* Shared = Cast<UStateTreeSharedNode>(WeakNode.Get());
			if (!Shared->SharedNode.IsValid())
			{
				return GetErrorColor();
			}
			if (Shared && Shared->SharedNode->NodeType == EScriptStateTreeNodeType::State)
			{
				return StateColor;
			}
			if (Shared && Shared->SharedNode->NodeType == EScriptStateTreeNodeType::Action)
			{
				return ActionColor;
			}
		}
	}

	return GetErrorColor();
}

EVisibility SScriptStateTreeViewRow::GetLinkedNodeVisibility() const
{
	if (const UStateTreeBaseNode* Node = WeakNode.Get())
	{
		if (Node->NodeType == EScriptStateTreeNodeType::Slot)
		{
			return EVisibility::Collapsed;
		}
	}

	return EVisibility::Visible;
}

FReply SScriptStateTreeViewRow::HandleLinkedNodeMouseDoubleClick(const FGeometry&, const FPointerEvent&) const
{
	if (!ScriptStateTreeViewModel.IsValid() || !WeakNode.Get())
	{
		return FReply::Handled();
	}

	if (WeakNode->NodeType == EScriptStateTreeNodeType::Action)
	{
		if (UStateTreeActionNode* ActionNode = Cast<UStateTreeActionNode>(WeakNode.Get()))
		{
			ScriptStateTreeViewModel->OnSpawnNodeGraph(ActionNode->EdGraph);
		}
	}
	else if(WeakNode->NodeType == EScriptStateTreeNodeType::Shared)
	{
		UStateTreeSharedNode* Shared = Cast<UStateTreeSharedNode>(WeakNode.Get());
		if (!Shared->SharedNode.IsValid())
		{
			return FReply::Handled();
		}
		if (Shared && Shared->SharedNode->NodeType == EScriptStateTreeNodeType::Action)
		{
			UStateTreeActionNode* ActionNode = Cast<UStateTreeActionNode>(Shared->SharedNode.Get());
			ScriptStateTreeViewModel->OnSpawnNodeGraph(ActionNode->EdGraph);
		}
	}
	return FReply::Handled();
}

TOptional<EMouseCursor::Type> SScriptStateTreeViewRow::GetLinkedNodeCursor() const
{
	//if (const UStateTreeBaseNode* Node = WeakNode.Get())
	//{
	//	switch (Node->NodeType)
	//	{
	//	case EScriptStateTreeNodeType::Action: {
	//		return EMouseCursor::Hand;
	//	}
	//	default: {
	//		return EMouseCursor::Default;
	//	}
	//	}
	//}
	//
	//return EMouseCursor::Default;

	return EMouseCursor::Hand;
}

void SScriptStateTreeViewRow::HandleLinkedNodeLabelTextCommitted(const FText& NewLabel, ETextCommit::Type CommitType) const
{
	if (ScriptStateTreeViewModel)
	{
		if (UStateTreeBaseNode* Node = WeakNode.Get())
		{
			ScriptStateTreeViewModel->RenameNode(Node, FName(*FText::TrimPrecedingAndTrailing(NewLabel).ToString()));
		}
	}
}

bool SScriptStateTreeViewRow::IsLinkedNodeNameReadOnly() const
{
	if (const UStateTreeBaseNode* Node = WeakNode.Get())
	{
		if (Node->NodeType == EScriptStateTreeNodeType::Action)
		{
			return true;
		}
		else if (Node->NodeType == EScriptStateTreeNodeType::Shared)
		{
			const UStateTreeSharedNode* Shared = Cast<const UStateTreeSharedNode>(Node);
			if (!Shared->SharedNode.IsValid())
			{
				return true;
			}
			if (Shared->SharedNode->NodeType == EScriptStateTreeNodeType::Action)
			{
				return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// UStateTreeActionNode

void UStateTreeActionNode::GenerateSlotNodes()
{
	Super::GenerateSlotNodes();

	UStateAbilityScriptGraph* ScriptGraph =Cast<UStateAbilityScriptGraph>(EdGraph);
	const TMap<FGuid, TObjectPtr<UStateAbilityEventSlot>>& EventSlotsMap = ScriptGraph->CollectAllEventSlot();
	for (auto EventSlotPair : EventSlotsMap)
	{
		UStateAbilityEventSlot* EventSlot = EventSlotPair.Value;

		FStateEventSlotInfo SlotInfo;
		SlotInfo.EventName = EventSlot->DisplayName;
		SlotInfo.UID = EventSlot->EditorGetUID(ScriptStateTreeViewModel->GetScriptArchetype()->GetDefaultScript());
		SlotInfo.SlotType = EStateEventSlotType::ActionEvent;

		UStateTreeBaseNode* SlotNode = NewObject<UStateTreeBaseNode>(this, FName(), RF_Transactional);
		SlotNode->Name = NAME_None;
		SlotNode->NodeType = EScriptStateTreeNodeType::Slot;
		SlotNode->Parent = this;
		SlotNode->bIsExpanded = true;
		SlotNode->EventSlotInfo = SlotInfo;

		Children.Add(SlotNode);
	}

	SortSlotNodes();
}

void UStateTreeActionNode::OnUpdateNode(TSharedRef<FScriptStateTreeViewModel> ScriptStateTreeViewModelRef)
{
	//////////////////////////////////////////////////////////////////////////
	// 刷新 Event Slot
	UStateAbilityScriptGraph* ScriptGraph = Cast<UStateAbilityScriptGraph>(EdGraph);
	TMap<FGuid, TObjectPtr<UStateAbilityEventSlot>> EventSlotsMap = ScriptGraph->CollectAllEventSlot();
	for (auto It = Children.CreateIterator(); It; ++It)
	{
		UStateTreeBaseNode* Node = It->Get();
		if (Node->NodeType != EScriptStateTreeNodeType::Slot && EventSlotsMap.Find(Node->EventSlotInfo.UID))
		{
			Node->EventSlotInfo.EventName = EventSlotsMap.FindOrAdd(Node->EventSlotInfo.UID)->DisplayName;
			EventSlotsMap.Remove(Node->EventSlotInfo.UID);
			continue;
		}
		Node->Release();

		It.RemoveCurrent();
	}

	for (auto EventSlotPair : EventSlotsMap)
	{
		UStateAbilityEventSlot* EventSlot = EventSlotPair.Value;

		FStateEventSlotInfo SlotInfo;
		SlotInfo.EventName = EventSlot->DisplayName;
		SlotInfo.UID = EventSlot->EditorGetUID(ScriptStateTreeViewModelRef->GetScriptArchetype()->GetDefaultScript());
		SlotInfo.SlotType = EStateEventSlotType::ActionEvent;

		UStateTreeBaseNode* SlotNode = NewObject<UStateTreeBaseNode>(this, FName(), RF_Transactional);
		SlotNode->Name = NAME_None;
		SlotNode->NodeType = EScriptStateTreeNodeType::Slot;
		SlotNode->Parent = this;
		SlotNode->bIsExpanded = true;
		SlotNode->EventSlotInfo = SlotInfo;

		Children.Add(SlotNode);
	}

	SortSlotNodes();
	//////////////////////////////////////////////////////////////////////////

	Super::OnUpdateNode(ScriptStateTreeViewModelRef);
}

void UStateTreeActionNode::Init(TSharedPtr<FScriptStateTreeViewModel> InScriptStateTreeViewModel)
{
	Name = FName("ActionGraph");

	constexpr int32 MaxIterations = 100;
	int32 Number = 1;
	while (!InScriptStateTreeViewModel->HasUniqueName(this) && Number < MaxIterations)
	{
		Number++;
		Name.SetNumber(Number);
	}

	check(Number != MaxIterations);

	// 创建一个新的EdGraph
	EdGraph = UStateAbilityScriptGraph::CreateNewGraph(InScriptStateTreeViewModel->GetScriptArchetype(), NAME_None, UStateAbilityScriptGraph::StaticClass(), UStateAbilityScriptEdGraphSchema::StaticClass());
	
	// @TODO：是否有些旧的EdGraph数据需要继承？

	// 最后才构建EventSlot，注意在此之前ScriptStateTreeViewModel都还未被初始化
	Super::Init(InScriptStateTreeViewModel);
}

void UStateTreeActionNode::Release()
{
	if (EdGraph)
	{
		if (UStateAbilityScriptGraph* StateAbilityScriptGraph = Cast<UStateAbilityScriptGraph>(EdGraph))
		{
			StateAbilityScriptGraph->Release();
		}

		EdGraph->SetFlags(RF_Transient);
		EdGraph->Rename(nullptr, GetTransientPackage());
		EdGraph->MarkAsGarbage();
	}

	Super::Release();
}

//////////////////////////////////////////////////////////////////////////
// UStateTreeStateNode

void UStateTreeStateNode::GenerateSlotNodes()
{
	Super::GenerateSlotNodes();

	if (!StateInstance)
	{
		return;
	}

	TMap<FName, FConfigVars_EventSlot> EventSlots = StateInstance->GetEventSlots();

	for (auto& EventSlotPair : EventSlots)
	{
		FStateEventSlotInfo SlotInfo;
		SlotInfo.EventName = EventSlotPair.Key;
		SlotInfo.UID = EventSlotPair.Value.UID;
		SlotInfo.SlotType = EStateEventSlotType::StateEvent;

		UStateTreeBaseNode* SlotNode = NewObject<UStateTreeBaseNode>(this, FName(), RF_Transactional);
		SlotNode->Name = NAME_None;
		SlotNode->NodeType = EScriptStateTreeNodeType::Slot;
		SlotNode->Parent = this;
		SlotNode->bIsExpanded = true;
		SlotNode->EventSlotInfo = SlotInfo;

		Children.Add(SlotNode);
	}

}

void UStateTreeStateNode::Init(TSharedPtr<FScriptStateTreeViewModel> InScriptStateTreeViewModel)
{
	Super::Init(InScriptStateTreeViewModel);
	if (!StateInstance)
	{
		return;
	}
	
	if (StateInstance->DisplayName != NAME_None)
	{
		Name = StateInstance->DisplayName;
	}
	else
	{
		FString DisplayName = StateInstance->GetClass()->GetMetaData(TEXT("DisplayName"));

		Name = DisplayName.IsEmpty() ? FName(StateInstance->GetClass()->GetDisplayNameText().ToString()) : FName(DisplayName);
	}

	constexpr int32 MaxIterations = 100;
	int32 Number = 1;
	while (!ScriptStateTreeViewModel->HasUniqueName(this) && Number < MaxIterations)
	{
		Number++;
		Name.SetNumber(Number);
	}

	check(Number != MaxIterations);

	StateInstance->DisplayName = Name;
}

void UStateTreeStateNode::OnPropertyChanged()
{
	Super::OnPropertyChanged();
	Name = StateInstance->DisplayName;

	constexpr int32 MaxIterations = 100;
	int32 Number = 1;
	while (!ScriptStateTreeViewModel->HasUniqueName(this) && Number < MaxIterations)
	{
		Number++;
		Name.SetNumber(Number);
	}

	check(Number != MaxIterations);

	StateInstance->DisplayName = Name;
}

void UStateTreeStateNode::Release()
{
	if (StateInstance)
	{
		StateInstance->SetFlags(RF_Transient);
		StateInstance->Rename(nullptr, GetTransientPackage());
		StateInstance->MarkAsGarbage();

	}

	Super::Release();
}

//////////////////////////////////////////////////////////////////////////
// UStateTreeSharedNode
void UStateTreeSharedNode::Init(TSharedPtr<FScriptStateTreeViewModel> InScriptStateTreeViewModel)
{
	Super::Init(InScriptStateTreeViewModel);
	if (!SharedNode.IsValid())
	{
		return;
	}

	Name = SharedNode->Name;
}

void UStateTreeSharedNode::OnPropertyChanged()
{
	Super::OnPropertyChanged();
	if (!SharedNode.IsValid())
	{
		return;
	}
	SharedNode->OnPropertyChanged();
	Name = SharedNode->Name;
}

#undef LOCTEXT_NAMESPACE