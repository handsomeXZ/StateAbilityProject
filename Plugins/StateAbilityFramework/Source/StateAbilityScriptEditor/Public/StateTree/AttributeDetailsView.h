#pragma once

#include "CoreMinimal.h"

#include "AttributeDetailsView.generated.h"

struct FStateAbilityAttributeBag;
struct FAttributeBagPropertyDesc;
class SInlineEditableTextBlock;
class SPinTypeSelector;

enum class EAttributeDetailsNodeType : uint8
{
	None,
	CategoryNode,
	AttributeNode,
};

struct FAttributeDetailsNode : TSharedFromThis<FAttributeDetailsNode>
{
	FAttributeDetailsNode(FText& InName);
	FAttributeDetailsNode(const FAttributeBagPropertyDesc* InAttributeDesc);
	static TSharedPtr<FAttributeDetailsNode> CreateCategoryNode(FText& InCategory);
	static TSharedPtr<FAttributeDetailsNode> CreateAttributeNode(const FAttributeBagPropertyDesc* InAttributeDesc);

	EAttributeDetailsNodeType NodeType;
	int32 Index;
	UObject* Owner;
	FText Name;
	const FAttributeBagPropertyDesc* AttributeDesc;
};

class FAttributeDetailsViewModel : public TSharedFromThis<FAttributeDetailsViewModel>
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnPinInfoChanged);
	FOnPinInfoChanged OnPinInfoChanged;
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
	void GenerateAddAttributeWidget();
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FAttributeDetailsNode> InNode, const TSharedRef<STableViewBase>& InOwnerTableView);
	void OnGetChildren(TSharedPtr<FAttributeDetailsNode> InParent, TArray<TSharedPtr<FAttributeDetailsNode>>& OutChildren);
	void UpdateDetails();
private:
	TWeakObjectPtr<UObject> AttributeBagOwner;
	FStateAbilityAttributeBag* AttributeBag;
	const FProperty* BagStructProperty;

	TArray<TSharedPtr<FAttributeDetailsNode>> TreeRoots;
	TSharedPtr<FAttributeDetailsViewModel> ViewModel;

	TSharedPtr<SHorizontalBox> AddAttributeWidget;
	TSharedPtr<STreeView<TSharedPtr<FAttributeDetailsNode>>> TreeViewWidget;
};

class SAttributeDetailsViewRow : public STableRow<TSharedPtr<FAttributeDetailsNode>>
{
public:
	SLATE_BEGIN_ARGS(SAttributeDetailsViewRow)
		{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<FAttributeDetailsNode> InNode, const FProperty* BagStructProperty, TSharedPtr<FAttributeDetailsViewModel> InViewModel);
private:
	FText GetAttributeName() const;

private:
	TSharedPtr<FAttributeDetailsNode> Node;
	TSharedPtr<SInlineEditableTextBlock> NameTextBlock;
	TSharedPtr<SPinTypeSelector> PinTypeSelector;

	TSharedPtr<FAttributeDetailsViewModel> ViewModel;
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