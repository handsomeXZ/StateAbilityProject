#include "StateTree/ScriptStateTreeSchema.h"

#include "StateTree/ScriptStateTreeView.h"
#include "Component/StateAbility/StateAbilityState.h"
#include "Component/StateAbility/StateAbilityAction.h"
#include "Component/StateAbility/Script/StateAbilityScript.h"
#include "StateAbilityScriptEditor.h"

#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBorder.h"

// AIGraph
#include "AIGraphTypes.h"

#define LOCTEXT_NAMESPACE "ScriptStateTreeSchema"



//////////////////////////////////////////////////////////////////////////
//	State Schema
//////////////////////////////////////////////////////////////////////////

// FScriptStateTreeSchemaStateItem
FScriptStateTreeSchemaStateItem::FScriptStateTreeSchemaStateItem()
	: StateClass(nullptr)
	, Parent(nullptr)
	, ItemType(FScriptStateTreeSchemaStateItem::ESchemaItemType::None)
{}

// FScriptStateTreeMenuSchemaState
FScriptStateTreeMenuSchemaState::FScriptStateTreeMenuSchemaState(UStateTreeBaseNode* InParentNode, TSharedPtr<FScriptStateTreeViewModel> InScriptStateTreeViewModel)
{
	WeakParentNode = InParentNode;
	ScriptStateTreeViewModel = InScriptStateTreeViewModel;
}

const FText FScriptStateTreeMenuSchemaState::DefaultCategoryName(LOCTEXT("ScriptStateTreeMenuSchema", "一般"));

void FScriptStateTreeMenuSchemaState::AddAction(const FText& InCategory, TSharedPtr<FScriptStateTreeSchemaStateItem> SchemaAction)
{
	AllActions.Add(SchemaAction);

	FText CategoryName = InCategory.IsEmpty() ? DefaultCategoryName : InCategory;

	for (TSharedPtr<FScriptStateTreeSchemaStateItem> SchemaCategory : Categorys)
	{
		if (SchemaCategory->CategoryName.EqualTo(CategoryName))
		{
			SchemaCategory->AddChild(SchemaAction);
			SchemaAction->Parent = SchemaCategory.ToWeakPtr();
			return;
		}
	}

	TSharedPtr<FScriptStateTreeSchemaStateItem> SchemaCategory = FScriptStateTreeSchemaStateItem::CreateCategory(CategoryName);
	SchemaCategory->AddChild(SchemaAction);
	SchemaAction->Parent = SchemaCategory.ToWeakPtr();

	Categorys.Add(SchemaCategory);
}

void FScriptStateTreeMenuSchemaState::Sort()
{
	Categorys.Sort(FScriptStateTreeSchemaStateItem::SortCompare);

	for (TSharedPtr<FScriptStateTreeSchemaStateItem> SchemaCategory : Categorys)
	{
		SchemaCategory->SortChildren();
	}
}

void FScriptStateTreeMenuSchemaState::Reset()
{
	AllActions.Empty();
	Categorys.Empty();
}

bool FScriptStateTreeMenuSchemaState::PerformAction(TSharedPtr<FScriptStateTreeSchemaStateItem> SchemaItem)
{
	UStateAbilityScriptArchetype* ScriptArchetype = ScriptStateTreeViewModel->GetScriptArchetype();
	UStateTreeBaseNode* ParentNode = WeakParentNode.Get();
	if (!ScriptArchetype || !ParentNode)
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("AddNode", "Add Node"));

	// 延迟初始化
	UStateTreeStateNode* NewStateNode = Cast<UStateTreeStateNode>(ScriptStateTreeViewModel->CreateNewNode(ParentNode, EScriptStateTreeNodeType::State, true));

	NewStateNode->StateInstance = UStateAbilityNodeBase::CreateInstance<UStateAbilityState>(ScriptArchetype, SchemaItem->StateClass);

	NewStateNode->Init(ScriptStateTreeViewModel);
	ScriptStateTreeViewModel->OnNodeAdded(ParentNode, NewStateNode);

	return true;
}

// SScriptStateTreeSchemaStateMenu
void SScriptStateTreeSchemaStateMenu::Construct(const FArguments& InArgs, UStateTreeBaseNode* InParentNode, TSharedPtr<FScriptStateTreeViewModel> InScriptStateTreeViewModel)
{
	MenuSchema = MakeShared<FScriptStateTreeMenuSchemaState>(InParentNode, InScriptStateTreeViewModel);

	TreeView = SNew(STreeView<TSharedPtr<FScriptStateTreeSchemaStateItem>>)
		.ItemHeight(24)
		.TreeItemsSource(&(this->MenuSchema->GetCategorys()))
		.OnGenerateRow(this, &SScriptStateTreeSchemaStateMenu::OnGenerateRow)
		.OnSelectionChanged(this, &SScriptStateTreeSchemaStateMenu::OnItemSelected)
		.OnMouseButtonDoubleClick(this, &SScriptStateTreeSchemaStateMenu::OnItemDoubleClicked)
		.OnGetChildren(this, &SScriptStateTreeSchemaStateMenu::OnGetChildren)
		.SelectionMode(ESelectionMode::Single)
		.OnSetExpansionRecursive(this, &SScriptStateTreeSchemaStateMenu::OnSetExpansionRecursive)
		.HighlightParentNodesForSelection(true);

	// Build the widget layout
	SBorder::Construct(SBorder::FArguments()
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		.Padding(5.f)
		[
			// Achieving fixed width by nesting items within a fixed width box.
			SNew(SBox)
				.WidthOverride(400.f)
				[
					SNew(SVerticalBox)
						// FILTER BOX
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SAssignNew(FilterTextBox, SSearchBox)
								.Visibility(EVisibility::Visible)
								.OnTextChanged(this, &SScriptStateTreeSchemaStateMenu::OnFilterTextChanged)
								.OnTextCommitted(this, &SScriptStateTreeSchemaStateMenu::OnFilterTextCommitted)
								.DelayChangeNotificationsWhileTyping(false)
						]
						// ACTION LIST
						+ SVerticalBox::Slot()
						.Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
						.FillHeight(1.f)
						[
							SNew(SScrollBorder, TreeView.ToSharedRef())
								[
									TreeView.ToSharedRef()
								]
						]
				]
		]
	);

	// Get all actions.
	RefreshAllActions(false);
}

void SScriptStateTreeSchemaStateMenu::RefreshAllActions(bool bPreserveExpansion, bool bHandleOnSelectionEvent /* = true */)
{
	MenuSchema->Reset();

	FStateAbilityScriptEditorModule& EditorModule = FModuleManager::GetModuleChecked<FStateAbilityScriptEditorModule>(TEXT("StateAbilityScriptEditor"));
	FGraphNodeClassHelper* ClassCache = EditorModule.GetClassCache().Get();

	TArray<FGraphNodeClassData> StateClasses;
	ClassCache->GatherClasses(UStateAbilityState::StaticClass(), StateClasses);
	for (auto& StateClass : StateClasses)
	{
		TSharedPtr<FScriptStateTreeSchemaStateItem> SchemaAction = FScriptStateTreeSchemaStateItem::CreateAction(StateClass.ToString(), StateClass.GetTooltip(), StateClass.GetClass());

		MenuSchema->AddAction(StateClass.GetCategory(), SchemaAction);
	}



	GenerateFilteredItems(bPreserveExpansion);

}

void SScriptStateTreeSchemaStateMenu::GenerateFilteredItems(bool bPreserveExpansion)
{
	MenuSchema->Sort();


	TreeView->RequestTreeRefresh();
}

TSharedRef<ITableRow> SScriptStateTreeSchemaStateMenu::OnGenerateRow(TSharedPtr<FScriptStateTreeSchemaStateItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedPtr<STableRow<TSharedPtr<FScriptStateTreeSchemaStateItem>>> TableRow = SNew(STableRow<TSharedPtr<FScriptStateTreeSchemaStateItem>>, OwnerTable)
		.bAllowPreselectedItemActivation(true);

	TSharedPtr<SHorizontalBox> RowContainer;
	TSharedPtr<SWidget> RowContent;
	FMargin RowPadding = FMargin(0, 2);

	switch (InItem->GetItemType())
	{
	case FScriptStateTreeSchemaStateItem::ESchemaItemType::Category: {
		RowContent = SNew(STextBlock)
			.Text(InItem->CategoryName);
		break;
	}
	case FScriptStateTreeSchemaStateItem::ESchemaItemType::Action: {
		RowContent = SNew(SScriptStateTreeSchemaState, InItem)
			.FilterText(this, &SScriptStateTreeSchemaStateMenu::GetFilterText);
		break;
	}
	default: {
		return TableRow.ToSharedRef();
	}
	}

	TableRow->SetRowContent
	(
		SAssignNew(RowContainer, SHorizontalBox)
	);

	TSharedPtr<SExpanderArrow> ExpanderWidget = SNew(SExpanderArrow, TableRow)
		.ShouldDrawWires(false)
		.IndentAmount(16)
		.BaseIndentLevel(0);

	RowContainer->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Right)
		[
			ExpanderWidget.ToSharedRef()
		];

	RowContainer->AddSlot()
		.FillWidth(1.0)
		.Padding(RowPadding)
		[
			RowContent.ToSharedRef()
		];

	return TableRow.ToSharedRef();
}

void SScriptStateTreeSchemaStateMenu::OnFilterTextChanged(const FText& InFilterText)
{
	GenerateFilteredItems(false);
}

void SScriptStateTreeSchemaStateMenu::OnFilterTextCommitted(const FText& InText, ETextCommit::Type CommitInfo)
{

}

void SScriptStateTreeSchemaStateMenu::OnItemSelected(TSharedPtr<FScriptStateTreeSchemaStateItem> InSelectedItem, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::OnMouseClick || SelectInfo == ESelectInfo::OnKeyPress)
	{
		if (InSelectedItem->GetItemType() != FScriptStateTreeSchemaStateItem::ESchemaItemType::Action)
		{
			return;
		}

		bool bDoDismissMenus = false;

		bDoDismissMenus = MenuSchema->PerformAction(InSelectedItem);

		if (bDoDismissMenus)
		{
			FSlateApplication::Get().DismissAllMenus();
		}
	}
}

void SScriptStateTreeSchemaStateMenu::OnItemDoubleClicked(TSharedPtr<FScriptStateTreeSchemaStateItem> InClickedItem)
{
	if (InClickedItem.IsValid())
	{
		if (InClickedItem->Children.Num())
		{
			TreeView->SetItemExpansion(InClickedItem, !TreeView->IsItemExpanded(InClickedItem));
		}
	}
}

void SScriptStateTreeSchemaStateMenu::OnGetChildren(TSharedPtr<FScriptStateTreeSchemaStateItem> InItem, TArray<TSharedPtr<FScriptStateTreeSchemaStateItem>>& OutChildren)
{
	if (InItem->Children.Num())
	{
		OutChildren = InItem->Children;
	}
}

void SScriptStateTreeSchemaStateMenu::OnSetExpansionRecursive(TSharedPtr<FScriptStateTreeSchemaStateItem> InItem, bool bInIsItemExpanded)
{
	if (InItem.IsValid() && InItem->Children.Num())
	{
		TreeView->SetItemExpansion(InItem, bInIsItemExpanded);

		for (TSharedPtr<FScriptStateTreeSchemaStateItem> Child : InItem->Children)
		{
			OnSetExpansionRecursive(Child, bInIsItemExpanded);
		}
	}
}

FText SScriptStateTreeSchemaStateMenu::GetFilterText() const
{
	return FilterTextBox->GetText();
}

// SScriptStateTreeSchemaState
void SScriptStateTreeSchemaState::Construct(const FArguments& InArgs, TSharedPtr<FScriptStateTreeSchemaStateItem> InSchemaItem)
{
	SchemaItem = InSchemaItem;

	this->ChildSlot
		[
			SNew(SHorizontalBox)
				.ToolTipText(SchemaItem->ActionToolTip)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						.Text(FText::FromString(SchemaItem->ActionName))
						.HighlightText(InArgs._FilterText)
				]
		];
}

//////////////////////////////////////////////////////////////////////////
//	Shared Schema
//////////////////////////////////////////////////////////////////////////

// FScriptStateTreeSchemaSharedItem
FScriptStateTreeSchemaSharedItem::FScriptStateTreeSchemaSharedItem()
	: WeakSharedNode(nullptr)
	, Parent(nullptr)
	, ItemType(FScriptStateTreeSchemaSharedItem::ESchemaItemType::None)
{}

// FScriptStateTreeMenuSchemaShared
FScriptStateTreeMenuSchemaShared::FScriptStateTreeMenuSchemaShared(UStateTreeBaseNode* InParentNode, TSharedPtr<FScriptStateTreeViewModel> InScriptStateTreeViewModel)
{
	WeakParentNode = InParentNode;
	ScriptStateTreeViewModel = InScriptStateTreeViewModel;
}

const FText FScriptStateTreeMenuSchemaShared::DefaultCategoryName(LOCTEXT("ScriptStateTreeMenuSchema", "其他"));

void FScriptStateTreeMenuSchemaShared::GetAllStateTreeCanSharedNode(UStateTreeBaseNode* RootNode, TArray<TObjectPtr<UStateTreeBaseNode>>& Nodes)
{
	if (!IsValid(RootNode) || RootNode->NodeType != EScriptStateTreeNodeType::Action && RootNode->NodeType != EScriptStateTreeNodeType::State)
	{
		return;
	}

	Nodes.Add(RootNode);

	for (auto child : RootNode->Children)
	{
		GetAllStateTreeCanSharedNode(child, Nodes);
	}
}

void FScriptStateTreeMenuSchemaShared::AddAction(const FText& InCategory, TSharedPtr<FScriptStateTreeSchemaSharedItem> SchemaAction)
{
	AllActions.Add(SchemaAction);

	FText CategoryName = InCategory.IsEmpty() ? DefaultCategoryName : InCategory;

	for (TSharedPtr<FScriptStateTreeSchemaSharedItem> SchemaCategory : Categorys)
	{
		if (SchemaCategory->ActionName.EqualTo(CategoryName))
		{
			SchemaCategory->AddChild(SchemaAction);
			SchemaAction->Parent = SchemaCategory.ToWeakPtr();
			return;
		}
	}

	TSharedPtr<FScriptStateTreeSchemaSharedItem> SchemaCategory = FScriptStateTreeSchemaSharedItem::CreateCategory(CategoryName);
	SchemaCategory->AddChild(SchemaAction);
	SchemaAction->Parent = SchemaCategory.ToWeakPtr();

	Categorys.Add(SchemaCategory);
}

void FScriptStateTreeMenuSchemaShared::Sort()
{
	Categorys.Sort(FScriptStateTreeSchemaSharedItem::SortCompare);

	for (TSharedPtr<FScriptStateTreeSchemaSharedItem> SchemaCategory : Categorys)
	{
		SchemaCategory->SortChildren();
	}
}

void FScriptStateTreeMenuSchemaShared::Reset()
{
	AllActions.Empty();
	Categorys.Empty();
}

bool FScriptStateTreeMenuSchemaShared::PerformAction(TSharedPtr<FScriptStateTreeSchemaSharedItem> SchemaItem)
{
	UStateTreeBaseNode* ParentNode = WeakParentNode.Get();
	if (!ParentNode)
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("AddNode", "Add Node"));

	// 延迟初始化
	UStateTreeSharedNode* NewSharedNode = Cast<UStateTreeSharedNode>(ScriptStateTreeViewModel->CreateNewNode(ParentNode, EScriptStateTreeNodeType::Shared, true));

	NewSharedNode->SharedNode = SchemaItem->WeakSharedNode.Get();
	NewSharedNode->Init(ScriptStateTreeViewModel);

	ScriptStateTreeViewModel->OnNodeAdded(ParentNode, NewSharedNode);

	return true;
}


// SScriptStateTreeSchemaSharedMenu
void SScriptStateTreeSchemaSharedMenu::Construct(const FArguments& InArgs, UStateTreeBaseNode* InParentNode, TSharedPtr<FScriptStateTreeViewModel> InScriptStateTreeViewModel)
{
	MenuSchema = MakeShared<FScriptStateTreeMenuSchemaShared>(InParentNode, InScriptStateTreeViewModel);

	TreeView = SNew(STreeView<TSharedPtr<FScriptStateTreeSchemaSharedItem>>)
		.ItemHeight(24)
		.TreeItemsSource(&(this->MenuSchema->GetCategorys()))
		.OnGenerateRow(this, &SScriptStateTreeSchemaSharedMenu::OnGenerateRow)
		.OnSelectionChanged(this, &SScriptStateTreeSchemaSharedMenu::OnItemSelected)
		.OnMouseButtonDoubleClick(this, &SScriptStateTreeSchemaSharedMenu::OnItemDoubleClicked)
		.OnGetChildren(this, &SScriptStateTreeSchemaSharedMenu::OnGetChildren)
		.SelectionMode(ESelectionMode::Single)
		.OnSetExpansionRecursive(this, &SScriptStateTreeSchemaSharedMenu::OnSetExpansionRecursive)
		.HighlightParentNodesForSelection(true);

	// Build the widget layout
	SBorder::Construct(SBorder::FArguments()
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		.Padding(5.f)
		[
			// Achieving fixed width by nesting items within a fixed width box.
			SNew(SBox)
				.WidthOverride(400.f)
				[
					SNew(SVerticalBox)
						// FILTER BOX
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SAssignNew(FilterTextBox, SSearchBox)
								.Visibility(EVisibility::Visible)
								.OnTextChanged(this, &SScriptStateTreeSchemaSharedMenu::OnFilterTextChanged)
								.OnTextCommitted(this, &SScriptStateTreeSchemaSharedMenu::OnFilterTextCommitted)
								.DelayChangeNotificationsWhileTyping(false)
						]
						// ACTION LIST
						+ SVerticalBox::Slot()
						.Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
						.FillHeight(1.f)
						[
							SNew(SScrollBorder, TreeView.ToSharedRef())
								[
									TreeView.ToSharedRef()
								]
						]
				]
		]
	);

	// Get all actions.
	RefreshAllActions(false);
}

void SScriptStateTreeSchemaSharedMenu::RefreshAllActions(bool bPreserveExpansion, bool bHandleOnSelectionEvent /* = true */)
{
	MenuSchema->Reset();

	TSharedPtr<FScriptStateTreeViewModel> ViewModel = MenuSchema->GetViewModel();
	TArray<TObjectPtr<UStateTreeBaseNode>> AllNodes;

	for (UStateTreeBaseNode* Node : ViewModel->GetTreeRoots())
	{
		MenuSchema->GetAllStateTreeCanSharedNode(Node, AllNodes);
	}


	for (UStateTreeBaseNode* Node : AllNodes)
	{
		TSharedPtr<FScriptStateTreeSchemaSharedItem> SchemaAction = FScriptStateTreeSchemaSharedItem::CreateAction(FText::FromName(Node->Name), Node);

		switch (Node->NodeType)
		{
		case EScriptStateTreeNodeType::Action: {
			static const FText CategoryName(LOCTEXT("ScriptStateTreeMenuSchema", "Action"));
			MenuSchema->AddAction(CategoryName, SchemaAction);
			break;
		}
		case EScriptStateTreeNodeType::State: {
			static const FText CategoryName(LOCTEXT("ScriptStateTreeMenuSchema", "State"));
			MenuSchema->AddAction(CategoryName, SchemaAction);
			break;
		}
		}
		
	}



	GenerateFilteredItems(bPreserveExpansion);

}

void SScriptStateTreeSchemaSharedMenu::GenerateFilteredItems(bool bPreserveExpansion)
{
	MenuSchema->Sort();


	TreeView->RequestTreeRefresh();
}

TSharedRef<ITableRow> SScriptStateTreeSchemaSharedMenu::OnGenerateRow(TSharedPtr<FScriptStateTreeSchemaSharedItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedPtr<STableRow<TSharedPtr<FScriptStateTreeSchemaSharedItem>>> TableRow = SNew(STableRow<TSharedPtr<FScriptStateTreeSchemaSharedItem>>, OwnerTable)
		.bAllowPreselectedItemActivation(true);

	TSharedPtr<SHorizontalBox> RowContainer;
	TSharedPtr<SWidget> RowContent;
	FMargin RowPadding = FMargin(0, 2);

	switch (InItem->GetItemType())
	{
	case FScriptStateTreeSchemaSharedItem::ESchemaItemType::Category: {
		RowContent = SNew(STextBlock)
			.Text(InItem->ActionName);
		break;
	}
	case FScriptStateTreeSchemaSharedItem::ESchemaItemType::Action: {
		RowContent = SNew(SScriptStateTreeSchemaShared, InItem)
			.FilterText(this, &SScriptStateTreeSchemaSharedMenu::GetFilterText);
		break;
	}
	default: {
		return TableRow.ToSharedRef();
	}
	}

	TableRow->SetRowContent
	(
		SAssignNew(RowContainer, SHorizontalBox)
	);

	TSharedPtr<SExpanderArrow> ExpanderWidget = SNew(SExpanderArrow, TableRow)
		.ShouldDrawWires(false)
		.IndentAmount(16)
		.BaseIndentLevel(0);

	RowContainer->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Right)
		[
			ExpanderWidget.ToSharedRef()
		];

	RowContainer->AddSlot()
		.FillWidth(1.0)
		.Padding(RowPadding)
		[
			RowContent.ToSharedRef()
		];

	return TableRow.ToSharedRef();
}

void SScriptStateTreeSchemaSharedMenu::OnFilterTextChanged(const FText& InFilterText)
{
	GenerateFilteredItems(false);
}

void SScriptStateTreeSchemaSharedMenu::OnFilterTextCommitted(const FText& InText, ETextCommit::Type CommitInfo)
{

}

void SScriptStateTreeSchemaSharedMenu::OnItemSelected(TSharedPtr<FScriptStateTreeSchemaSharedItem> InSelectedItem, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::OnMouseClick || SelectInfo == ESelectInfo::OnKeyPress)
	{
		if (InSelectedItem->GetItemType() != FScriptStateTreeSchemaSharedItem::ESchemaItemType::Action)
		{
			return;
		}

		bool bDoDismissMenus = false;

		bDoDismissMenus = MenuSchema->PerformAction(InSelectedItem);

		if (bDoDismissMenus)
		{
			FSlateApplication::Get().DismissAllMenus();
		}
	}
}

void SScriptStateTreeSchemaSharedMenu::OnItemDoubleClicked(TSharedPtr<FScriptStateTreeSchemaSharedItem> InClickedItem)
{
	if (InClickedItem.IsValid())
	{
		if (InClickedItem->Children.Num())
		{
			TreeView->SetItemExpansion(InClickedItem, !TreeView->IsItemExpanded(InClickedItem));
		}
	}
}

void SScriptStateTreeSchemaSharedMenu::OnGetChildren(TSharedPtr<FScriptStateTreeSchemaSharedItem> InItem, TArray<TSharedPtr<FScriptStateTreeSchemaSharedItem>>& OutChildren)
{
	if (InItem->Children.Num())
	{
		OutChildren = InItem->Children;
	}
}

void SScriptStateTreeSchemaSharedMenu::OnSetExpansionRecursive(TSharedPtr<FScriptStateTreeSchemaSharedItem> InItem, bool bInIsItemExpanded)
{
	if (InItem.IsValid() && InItem->Children.Num())
	{
		TreeView->SetItemExpansion(InItem, bInIsItemExpanded);

		for (TSharedPtr<FScriptStateTreeSchemaSharedItem> Child : InItem->Children)
		{
			OnSetExpansionRecursive(Child, bInIsItemExpanded);
		}
	}
}

FText SScriptStateTreeSchemaSharedMenu::GetFilterText() const
{
	return FilterTextBox->GetText();
}

// SScriptStateTreeSchemaShared
void SScriptStateTreeSchemaShared::Construct(const FArguments& InArgs, TSharedPtr<FScriptStateTreeSchemaSharedItem> InSchemaItem)
{
	SchemaItem = InSchemaItem;

	this->ChildSlot
		[
			SNew(SHorizontalBox)
				.ToolTipText(SchemaItem->ActionToolTip)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						.Text(SchemaItem->ActionName)
						.HighlightText(InArgs._FilterText)
				]
		];
}

#undef LOCTEXT_NAMESPACE