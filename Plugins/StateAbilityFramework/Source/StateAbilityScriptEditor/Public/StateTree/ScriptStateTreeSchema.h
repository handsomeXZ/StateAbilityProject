#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

class FScriptStateTreeViewModel;
class UStateTreeBaseNode;

DECLARE_DELEGATE(FStateMenuClosed);

//////////////////////////////////////////////////////////////////////////
//	State Schema
//////////////////////////////////////////////////////////////////////////
struct FScriptStateTreeSchemaStateItem : public TSharedFromThis<FScriptStateTreeSchemaStateItem>
{
	enum class ESchemaItemType : uint8
	{
		None,
		Category,
		Action,
	};
	FText CategoryName;
	FString ActionName;
	FText ActionToolTip;
	UClass* StateClass;
	TArray<TSharedPtr<FScriptStateTreeSchemaStateItem>> Children;
	TWeakPtr<FScriptStateTreeSchemaStateItem> Parent;

	FScriptStateTreeSchemaStateItem();
	~FScriptStateTreeSchemaStateItem() {}

	ESchemaItemType GetItemType() { return ItemType; }

	static TSharedPtr<FScriptStateTreeSchemaStateItem> CreateCategory(const FText& InCategory) {
		TSharedPtr<FScriptStateTreeSchemaStateItem> SchemaCategory = MakeShared<FScriptStateTreeSchemaStateItem>();
		SchemaCategory->ItemType = ESchemaItemType::Category;
		SchemaCategory->CategoryName = InCategory;
		return SchemaCategory;
	}

	static TSharedPtr<FScriptStateTreeSchemaStateItem> CreateAction(const FString& InActionName, const FText& InActionToolTip, UClass* InStateClass) {
		TSharedPtr<FScriptStateTreeSchemaStateItem> SchemaAction = MakeShared<FScriptStateTreeSchemaStateItem>();
		SchemaAction->ItemType = ESchemaItemType::Action;
		SchemaAction->ActionName = InActionName;
		SchemaAction->ActionToolTip = InActionToolTip;
		SchemaAction->StateClass = InStateClass;
		return SchemaAction;
	}

	void AddChild(TSharedPtr<FScriptStateTreeSchemaStateItem> Child) { Children.Add(Child); }
	void SortChildren() { Children.Sort(FScriptStateTreeSchemaStateItem::SortCompare); }
	static bool SortCompare(const TSharedPtr<FScriptStateTreeSchemaStateItem>& LeftAction, const TSharedPtr<FScriptStateTreeSchemaStateItem>& RightAction) {
		switch (LeftAction->ItemType)
		{
		case ESchemaItemType::Action:
		{
			return LeftAction->ActionName <= RightAction->ActionName;
		}
		case ESchemaItemType::Category:
		{
			return LeftAction->CategoryName.ToString() <= RightAction->CategoryName.ToString();
		}
		}
		return true;
	};

private:
	ESchemaItemType ItemType;
};

struct FScriptStateTreeMenuSchemaState : public TSharedFromThis<FScriptStateTreeMenuSchemaState>
{
public:
	static const FText DefaultCategoryName;

	FScriptStateTreeMenuSchemaState(UStateTreeBaseNode* InParentNode, TSharedPtr<FScriptStateTreeViewModel> InScriptStateTreeViewModel);
	void AddAction(const FText& InCategory, TSharedPtr<FScriptStateTreeSchemaStateItem> SchemaAction);
	void Sort();
	void Reset();
	bool PerformAction(TSharedPtr<FScriptStateTreeSchemaStateItem> SchemaItem);
	TArray<TSharedPtr<FScriptStateTreeSchemaStateItem>>& GetCategorys() { return Categorys; }
	TArray<TSharedPtr<FScriptStateTreeSchemaStateItem>>& GetActions() { return AllActions; }
private:
	TWeakObjectPtr<UStateTreeBaseNode> WeakParentNode;
	TSharedPtr<FScriptStateTreeViewModel> ScriptStateTreeViewModel;
	TArray<TSharedPtr<FScriptStateTreeSchemaStateItem>> AllActions;
	TArray<TSharedPtr<FScriptStateTreeSchemaStateItem>> Categorys;
};

class SScriptStateTreeSchemaStateMenu : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SScriptStateTreeSchemaStateMenu)
		{}
		SLATE_ARGUMENT(FStateMenuClosed, OnClosedCallback)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UStateTreeBaseNode* InParentNode, TSharedPtr<FScriptStateTreeViewModel> InScriptStateTreeViewModel);

	~SScriptStateTreeSchemaStateMenu() {}

	TSharedRef<SEditableTextBox> GetFilterTextBox();

private:
	void RefreshAllActions(bool bPreserveExpansion, bool bHandleOnSelectionEvent = true);
	void GenerateFilteredItems(bool bPreserveExpansion);

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FScriptStateTreeSchemaStateItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	void OnFilterTextChanged(const FText& InFilterText);
	void OnFilterTextCommitted(const FText& InText, ETextCommit::Type CommitInfo);
	void OnItemSelected(TSharedPtr<FScriptStateTreeSchemaStateItem> InSelectedItem, ESelectInfo::Type SelectInfo);
	void OnItemDoubleClicked(TSharedPtr<FScriptStateTreeSchemaStateItem> InClickedItem);
	void OnGetChildren(TSharedPtr<FScriptStateTreeSchemaStateItem> InItem, TArray<TSharedPtr<FScriptStateTreeSchemaStateItem>>& OutChildren);
	void OnSetExpansionRecursive(TSharedPtr<FScriptStateTreeSchemaStateItem> InTreeNode, bool bInIsItemExpanded);

protected:
	FText GetFilterText() const;

protected:
	FStateMenuClosed OnClosedCallback;

private:
	TSharedPtr<FScriptStateTreeMenuSchemaState> MenuSchema;

	TSharedPtr<SSearchBox> FilterTextBox;
	TSharedPtr<STreeView<TSharedPtr<FScriptStateTreeSchemaStateItem>>> TreeView;
};

class SScriptStateTreeSchemaState : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SScriptStateTreeSchemaState) {}
		SLATE_ATTRIBUTE(FText, FilterText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FScriptStateTreeSchemaStateItem> InSchemaItem);
	/*virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override*/
public:
	TSharedPtr<FScriptStateTreeSchemaStateItem> SchemaItem;
};


//////////////////////////////////////////////////////////////////////////
//	Shared Schema
//////////////////////////////////////////////////////////////////////////
struct FScriptStateTreeSchemaSharedItem : public TSharedFromThis<FScriptStateTreeSchemaSharedItem>
{
	enum class ESchemaItemType : uint8
	{
		None,
		Category,
		Action,
	};
	FText ActionName;
	FText ActionToolTip;
	TWeakObjectPtr<UStateTreeBaseNode> WeakSharedNode;

	TArray<TSharedPtr<FScriptStateTreeSchemaSharedItem>> Children;
	TWeakPtr<FScriptStateTreeSchemaSharedItem> Parent;

	FScriptStateTreeSchemaSharedItem();
	~FScriptStateTreeSchemaSharedItem() {}

	ESchemaItemType GetItemType() { return ItemType; }

	static TSharedPtr<FScriptStateTreeSchemaSharedItem> CreateCategory(const FText& InCategory) {
		TSharedPtr<FScriptStateTreeSchemaSharedItem> SchemaCategory = MakeShared<FScriptStateTreeSchemaSharedItem>();
		SchemaCategory->ItemType = ESchemaItemType::Category;
		SchemaCategory->ActionName = InCategory;
		return SchemaCategory;
	}

	static TSharedPtr<FScriptStateTreeSchemaSharedItem> CreateAction(const FText& InActionName, UStateTreeBaseNode* InSharedNode) {
		TSharedPtr<FScriptStateTreeSchemaSharedItem> SchemaAction = MakeShared<FScriptStateTreeSchemaSharedItem>();
		SchemaAction->ItemType = ESchemaItemType::Action;
		SchemaAction->ActionName = InActionName;
		SchemaAction->WeakSharedNode = InSharedNode;
		return SchemaAction;
	}

	void AddChild(TSharedPtr<FScriptStateTreeSchemaSharedItem> Child) { Children.Add(Child); }
	void SortChildren() { Children.Sort(FScriptStateTreeSchemaSharedItem::SortCompare); }
	static bool SortCompare(const TSharedPtr<FScriptStateTreeSchemaSharedItem>& LeftAction, const TSharedPtr<FScriptStateTreeSchemaSharedItem>& RightAction) {
		return LeftAction->ActionName.ToString() <= RightAction->ActionName.ToString();
	};
private:
	ESchemaItemType ItemType;
};
struct FScriptStateTreeMenuSchemaShared : public TSharedFromThis<FScriptStateTreeMenuSchemaShared>
{
public:
	static const FText DefaultCategoryName;

	FScriptStateTreeMenuSchemaShared(UStateTreeBaseNode* InParentNode, TSharedPtr<FScriptStateTreeViewModel> InScriptStateTreeViewModel);

	TSharedPtr<FScriptStateTreeViewModel> GetViewModel() { return ScriptStateTreeViewModel; }
	void GetAllStateTreeCanSharedNode(UStateTreeBaseNode* RootNode, TArray<TObjectPtr<UStateTreeBaseNode>>& Nodes);
	void AddAction(const FText& InCategory, TSharedPtr<FScriptStateTreeSchemaSharedItem> SchemaAction);
	void Sort();
	void Reset();
	bool PerformAction(TSharedPtr<FScriptStateTreeSchemaSharedItem> SchemaItem);
	TArray<TSharedPtr<FScriptStateTreeSchemaSharedItem>>& GetActions() { return AllActions; }
	TArray<TSharedPtr<FScriptStateTreeSchemaSharedItem>>& GetCategorys() { return Categorys; }
private:
	TWeakObjectPtr<UStateTreeBaseNode> WeakParentNode;
	TSharedPtr<FScriptStateTreeViewModel> ScriptStateTreeViewModel;
	TArray<TSharedPtr<FScriptStateTreeSchemaSharedItem>> AllActions;
	TArray<TSharedPtr<FScriptStateTreeSchemaSharedItem>> Categorys;
};
class SScriptStateTreeSchemaSharedMenu : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SScriptStateTreeSchemaSharedMenu)
		{}
		SLATE_ARGUMENT(FStateMenuClosed, OnClosedCallback)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UStateTreeBaseNode* InParentNode, TSharedPtr<FScriptStateTreeViewModel> InScriptStateTreeViewModel);

	~SScriptStateTreeSchemaSharedMenu() {}

	TSharedRef<SEditableTextBox> GetFilterTextBox();

private:
	void RefreshAllActions(bool bPreserveExpansion, bool bHandleOnSelectionEvent = true);
	void GenerateFilteredItems(bool bPreserveExpansion);

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FScriptStateTreeSchemaSharedItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	void OnFilterTextChanged(const FText& InFilterText);
	void OnFilterTextCommitted(const FText& InText, ETextCommit::Type CommitInfo);
	void OnItemSelected(TSharedPtr<FScriptStateTreeSchemaSharedItem> InSelectedItem, ESelectInfo::Type SelectInfo);
	void OnItemDoubleClicked(TSharedPtr<FScriptStateTreeSchemaSharedItem> InClickedItem);
	void OnGetChildren(TSharedPtr<FScriptStateTreeSchemaSharedItem> InItem, TArray<TSharedPtr<FScriptStateTreeSchemaSharedItem>>& OutChildren);
	void OnSetExpansionRecursive(TSharedPtr<FScriptStateTreeSchemaSharedItem> InTreeNode, bool bInIsItemExpanded);

protected:
	FText GetFilterText() const;

protected:
	FStateMenuClosed OnClosedCallback;

private:
	TSharedPtr<FScriptStateTreeMenuSchemaShared> MenuSchema;

	TSharedPtr<SSearchBox> FilterTextBox;
	TSharedPtr<STreeView<TSharedPtr<FScriptStateTreeSchemaSharedItem>>> TreeView;
};

class SScriptStateTreeSchemaShared : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SScriptStateTreeSchemaShared) {}
		SLATE_ATTRIBUTE(FText, FilterText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FScriptStateTreeSchemaSharedItem> InSchemaItem);
public:
	TSharedPtr<FScriptStateTreeSchemaSharedItem> SchemaItem;
};