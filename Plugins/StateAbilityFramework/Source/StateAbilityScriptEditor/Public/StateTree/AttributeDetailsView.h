#pragma once

#include "CoreMinimal.h"

#include "AttributeDetailsView.generated.h"

struct FStateAbilityAttributeBag;
struct FAttributeBagPropertyDesc;
struct FAttributeBag;
class SInlineEditableTextBlock;
class SPinTypeSelector;
class FAttributeDetailsViewModel;

/************************************************************************/
/* 不允许AttributeBag之间嵌套。											*/
/* 编辑器层会做限制，参照 FAttributeDetailsNode::GenerateAttributeChildren	*/
/************************************************************************/

struct FAttributeDetailsNode : TSharedFromThis<FAttributeDetailsNode>
{
	FAttributeDetailsNode(FAttributeBag& AttributeBag, int32 InPropIndex, const FText& InDisplayName, UObject* InPropertyOuter, FProperty* InProperty, TSharedRef<FAttributeDetailsViewModel> InViewModel);

	void GenerateAttributeChildren(FAttributeBag& AttributeBag);
	bool IsDataValid() const;
	TSharedPtr<FAttributeDetailsNode> GetParentNode();
	TSharedPtr<FAttributeDetailsNode> GetOutermostNode();

	FGuid UID;				// Bag的UID，可以是自身(自身是Bag)的UID也可以是指向OwnerBag的UID
	int32 PropIndex;
	FText DisplayName;
	UObject* PropertyOuter;	// 不一定是Owner
	FProperty* Property;

	TSharedPtr<SWidget> ValueOverride;
	TSharedPtr<FAttributeDetailsNode> Parent;
	TArray<TSharedPtr<FAttributeDetailsNode>> Children;
	TWeakPtr<FAttributeDetailsViewModel> ViewModel;
};

class FAttributeDetailsViewModel : public TSharedFromThis<FAttributeDetailsViewModel>
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnPinInfoChanged);
	FOnPinInfoChanged OnPinInfoChanged;

	DECLARE_MULTICAST_DELEGATE(FOnUpdateDetails);
	FOnUpdateDetails OnUpdateDetails;
};

class SAttributeDetailsView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAttributeDetailsView)
		{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetObject(UObject* InSelection);
private:
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FAttributeDetailsNode> InNode, const TSharedRef<STableViewBase>& InOwnerTableView);
	void OnGetChildren(TSharedPtr<FAttributeDetailsNode> InParent, TArray<TSharedPtr<FAttributeDetailsNode>>& OutChildren);
	void UpdateDetails();
private:
	TWeakObjectPtr<UObject> AttributeBagOwner;

	TArray<TSharedPtr<FAttributeDetailsNode>> TreeRoots;
	TSharedPtr<FAttributeDetailsViewModel> ViewModel;

	TSharedPtr<STreeView<TSharedPtr<FAttributeDetailsNode>>> TreeViewWidget;
};

class SAttributeDetailsViewRow : public STableRow<TSharedPtr<FAttributeDetailsNode>>
{
public:
	SLATE_BEGIN_ARGS(SAttributeDetailsViewRow)
		{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedRef<FAttributeDetailsNode> InNode, TWeakPtr<FAttributeDetailsViewModel> InViewModel);
private:
	FText GetAttributeName() const;

	void GetFilteredVariableTypeTree(TArray<TSharedPtr<class UEdGraphSchema_K2::FPinTypeTreeInfo>>& TypeTree, ETypeTreeFilter TypeTreeFilter);
	FEdGraphPinType GetPinInfo();
	void PinInfoChanged(const FEdGraphPinType& PinType);
private:
	TSharedPtr<FAttributeDetailsNode> Node;
	TSharedPtr<SInlineEditableTextBlock> NameTextBlock;
	TSharedPtr<SPinTypeSelector> PinTypeSelector;

	TWeakPtr<FAttributeDetailsViewModel> ViewModel;
};

/**
 * Specific property bag schema to allow customizing the requirements (e.g. supported containers).
 */
UCLASS()
class UAttributeBagSchema : public UEdGraphSchema_K2
{
	GENERATED_BODY()
public:
	virtual bool SupportsPinTypeContainer(TWeakPtr<const FEdGraphSchemaAction> SchemaAction, const FEdGraphPinType& PinType, const EPinContainerType& ContainerType) const override
	{
		return ContainerType == EPinContainerType::None || ContainerType == EPinContainerType::Array;
	}
};