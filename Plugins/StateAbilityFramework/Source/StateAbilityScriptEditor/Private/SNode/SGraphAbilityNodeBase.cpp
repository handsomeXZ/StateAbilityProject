#include "SNode/SGraphAbilityNodeBase.h"

#include "Node/GraphAbilityNode.h"
#include "Node/GraphAbilityNode_Action.h"
#include "SNode/SGraphAbilityPin.h"

#include "Delegates/Delegate.h"
#include "GenericPlatform/ICursor.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Margin.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "Misc/Attribute.h"
#include "Misc/Optional.h"
#include "SGraphPin.h"
#include "SNodePanel.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Types/SlateEnums.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "SLevelOfDetailBranchNode.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"


const float SGraphAbilityNodeBase::DefaultMinNodeWidth = 120.f;
const float SGraphAbilityNodeBase::DefaultMinNodeHeight = 60.f;

/////////////////////////////////////////////////////
// SGraphAbilityNodeBase
void SGraphAbilityNodeBase::Construct(const FArguments& InArgs, UGraphAbilityNode* InNode)
{
	this->GraphNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();

	
}

SGraphAbilityNodeBase::~SGraphAbilityNodeBase()
{

}

void SGraphAbilityNodeBase::GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const
{

}

void SGraphAbilityNodeBase::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SGraphNode::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bIsDirty)
	{
		ReGeneratePinWidgets();
	}

	bIsDirty = false;
}

void SGraphAbilityNodeBase::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();

	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();


	FLinearColor TitleShadowColor(0.6f, 0.6f, 0.6f);

	const FMargin NodePadding = FMargin(2.0f);

	TSharedPtr<STextBlock> DescriptionText;
	TSharedPtr<SNodeTitle> NodeTitle = SNew(SNodeTitle, GraphNode);
	TWeakPtr<SNodeTitle> WeakNodeTitle = NodeTitle;
	auto GetNodeTitlePlaceholderWidth = [WeakNodeTitle]() -> FOptionalSize
		{
			TSharedPtr<SNodeTitle> NodeTitlePin = WeakNodeTitle.Pin();
			const float DesiredWidth = (NodeTitlePin.IsValid()) ? NodeTitlePin->GetTitleSize().X : 0.0f;
			return FMath::Max(75.0f, DesiredWidth);
		};
	auto GetNodeTitlePlaceholderHeight = [WeakNodeTitle]() -> FOptionalSize
		{
			TSharedPtr<SNodeTitle> NodeTitlePin = WeakNodeTitle.Pin();
			const float DesiredHeight = (NodeTitlePin.IsValid()) ? NodeTitlePin->GetTitleSize().Y : 0.0f;
			return FMath::Max(22.0f, DesiredHeight);
		};

	LeftOverrideWidget = SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderBackgroundColor(FLinearColor::Transparent)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			SAssignNew(LeftNodeBox, SVerticalBox)
		];

	RightOverrideWidget = SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderBackgroundColor(FLinearColor::Transparent)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			SAssignNew(RightNodeBox, SVerticalBox)
		];

	NodeName = SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("BTEditor.Graph.BTNode.Body"))
		.BorderBackgroundColor(FLinearColor(0.5f, 0.5f, 0.5f, 0.1f))
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Visibility(EVisibility::SelfHitTestInvisible)
		[
			SNew(SLevelOfDetailBranchNode)
			.UseLowDetailSlot(this, &SGraphAbilityNodeBase::UseLowDetailNodeTitles)
			.LowDetail()
			[
				SNew(SBox)
				.WidthOverride_Lambda(GetNodeTitlePlaceholderWidth)
				.HeightOverride_Lambda(GetNodeTitlePlaceholderHeight)
			]
			.HighDetail()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Visibility(EVisibility::Collapsed)
					.Image(FAppStyle::GetBrush(TEXT("BTEditor.Graph.BTNode.Icon")))
				]
				+ SHorizontalBox::Slot()
				.Padding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(InlineEditableText, SInlineEditableTextBlock)
						.Style(FAppStyle::Get(), "Graph.StateNode.NodeTitleInlineEditableText")
						.Text(NodeTitle.Get(), &SNodeTitle::GetHeadTitle)
						.OnVerifyTextChanged(this, &SGraphAbilityNodeBase::OnVerifyNameTextChanged)
						.OnTextCommitted(this, &SGraphAbilityNodeBase::OnNameTextCommited)
						.IsReadOnly(this, &SGraphAbilityNodeBase::IsNameReadOnly)
						.IsSelected(this, &SGraphAbilityNodeBase::IsSelectedExclusively)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						NodeTitle.ToSharedRef()
					]
				]
			]
		];


	this->ContentScale.Bind(this, &SGraphNode::GetContentScale);
	this->GetOrAddSlot(ENodeZone::Center)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SOverlay)
			// Pins and node details
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SVerticalBox)
				// TOP BODY
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.AutoHeight()
				[
					SNew(SBox)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.MinDesiredWidth(DefaultMinNodeWidth)
					.MinDesiredHeight(DefaultMinNodeHeight)
					[
						SNew(SBorder)
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Fill)
						.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
						.Padding(0.0f)
						.BorderBackgroundColor(this, &SGraphAbilityNodeBase::GetBorderBackgroundColor)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.Padding(NodePadding)
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Top)
								[
									NodeName.ToSharedRef()
								]
							]
						]
					]
				]
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.AutoHeight()
				[
					SAssignNew(NodeBottomPanel, SHorizontalBox)
				]
			]
			
		];

	CreatePinWidgets();
}

void SGraphAbilityNodeBase::ReGeneratePinWidgets()
{
	LeftNodeBox->ClearChildren();
	RightNodeBox->ClearChildren();
	OutputPins.Empty();

	CreatePinWidgets();
}

void SGraphAbilityNodeBase::CreatePinWidgets()
{
	UGraphAbilityNode* MyNode = CastChecked<UGraphAbilityNode>(GraphNode);

	for (int32 PinIdx = 0; PinIdx < MyNode->Pins.Num(); PinIdx++)
	{
		UEdGraphPin* MyPin = MyNode->Pins[PinIdx];
		if (!MyPin->bHidden)
		{
			TSharedPtr<SGraphPin> NewPin = SNew(SGraphAbilityPin, SharedThis(this), MyPin)
				.ToolTipText(this, &SGraphAbilityNodeBase::GetPinTooltip, MyPin);

			AddPin(NewPin.ToSharedRef());
		}
	}
}

FText SGraphAbilityNodeBase::GetPinTooltip(UEdGraphPin* GraphPinObj) const
{
	FText HoverText = FText::GetEmpty();

	check(GraphPinObj != nullptr);
	UEdGraphNode* OwningGraphNode = GraphPinObj->GetOwningNode();
	if (OwningGraphNode != nullptr)
	{
		FString HoverStr;
		OwningGraphNode->GetPinHoverText(*GraphPinObj, /*out*/HoverStr);
		if (!HoverStr.IsEmpty())
		{
			HoverText = FText::FromString(HoverStr);
		}
	}

	return HoverText;
}

FText SGraphAbilityNodeBase::GetPinName(const TSharedRef<SGraphPin>& PinToAdd) const
{
	if (PinToAdd->GetPinObj() != nullptr && !(PinToAdd->GetPinObj()->IsPendingKill()) && PinToAdd->GetPinObj()->PinName != NAME_None)
	{
		return FText::FromName(PinToAdd->GetPinObj()->PinName);
	}

	return FText::FromString(TEXT(""));
}

void SGraphAbilityNodeBase::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	PinToAdd->SetOwner(SharedThis(this));

	const UEdGraphPin* PinObj = PinToAdd->GetPinObj();
	const bool bAdvancedParameter = PinObj && PinObj->bAdvancedView;
	if (bAdvancedParameter)
	{
		PinToAdd->SetVisibility(TAttribute<EVisibility>(PinToAdd, &SGraphPin::IsPinVisibleAsAdvanced));
	}

	if (PinToAdd->GetDirection() == EEdGraphPinDirection::EGPD_Input)
	{
		LeftNodeBox->AddSlot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 5.0f)
			[
				PinToAdd
			];
		InputPins.Add(PinToAdd);
	}
	else // Direction == EEdGraphPinDirection::EGPD_Output
	{
		RightNodeBox->AddSlot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(0, 5.0f)
			[
				SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						PinToAdd
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Center)
					.Padding(FMargin(4.0f, 0.0f))
					[
						SNew(STextBlock)
						.Text_Lambda([this, PinToAdd]() {
							return GetPinName(PinToAdd);
						})
					]
			];
		OutputPins.Add(PinToAdd);
	}
}

FSlateColor SGraphAbilityNodeBase::GetBorderBackgroundColor() const
{
	FLinearColor InactiveStateColor(0.08f, 0.08f, 0.08f);
	FLinearColor ActiveStateColorDim(0.4f, 0.3f, 0.15f);
	FLinearColor ActiveStateColorBright(1.f, 0.6f, 0.35f);
	FLinearColor InstancedSelectorColor(0.08f, 0.08f, 0.08f);
	FLinearColor UnInstancedSelectorColor(0.f, 0.f, 0.f);
	/*return GetBorderBackgroundColor_Internal(InactiveStateColor, ActiveStateColorDim, ActiveStateColorBright);*/

	return UnInstancedSelectorColor;
}

TArray<FOverlayWidgetInfo> SGraphAbilityNodeBase::GetOverlayWidgets(bool bSelected, const FVector2D& WidgetSize) const
{
	TArray<FOverlayWidgetInfo> Widgets;

	FVector2D Origin(0.0f, 0.0f);

	{
		FOverlayWidgetInfo Overlay(LeftOverrideWidget);
		Overlay.OverlayOffset = FVector2D(-3.0f - LeftOverrideWidget->GetDesiredSize().X, Origin.Y);
		Widgets.Add(Overlay);
	}

	{
		FOverlayWidgetInfo Overlay(RightOverrideWidget);
		Overlay.OverlayOffset = FVector2D(3.0f + WidgetSize.X, Origin.Y);
		Widgets.Add(Overlay);
	}

	return Widgets;
}