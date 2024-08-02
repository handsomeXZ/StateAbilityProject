


#include "SNode/SGraphAbilityNode_Action.h"

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


/////////////////////////////////////////////////////
// SGraphAbilityNode_Action
void SGraphAbilityNode_Action::Construct(const FArguments& InArgs, UGraphAbilityNode* InNode)
{
	this->GraphNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();

	UGraphAbilityNode_Action* ActionNode = CastChecked<UGraphAbilityNode_Action>(GraphNode);

	TWeakPtr<SGraphAbilityNode_Action> WeakGraphNode = SharedThis(this);
	OnUpdateGraphNodeHandle = ActionNode->OnUpdateGraphNode.AddLambda([WeakGraphNode]() {
		if (WeakGraphNode.IsValid())
		{
			WeakGraphNode.Pin()->UpdateGraphNode();
		}
	});
}

SGraphAbilityNode_Action::~SGraphAbilityNode_Action()
{
	UGraphAbilityNode_Action* ActionNode = CastChecked<UGraphAbilityNode_Action>(GraphNode);
	if (IsValid(ActionNode))
	{
		ActionNode->OnUpdateGraphNode.Remove(OnUpdateGraphNodeHandle);
	}
}

void SGraphAbilityNode_Action::GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const
{

}

void SGraphAbilityNode_Action::UpdateGraphNode()
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

	this->ContentScale.Bind(this, &SGraphNode::GetContentScale);
	this->GetOrAddSlot( ENodeZone::Center )
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage( FAppStyle::GetBrush( "Graph.StateNode.Body" ) )
			.Padding(0.0f)
			.BorderBackgroundColor( this, &SGraphAbilityNode_Action::GetBorderBackgroundColor )
			[
				SNew(SOverlay)

				// Pins and node details
				+SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SHorizontalBox)

					// INPUT PIN AREA
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.MinDesiredHeight(NodePadding.Top)
						[
							SAssignNew(LeftNodeBox, SVerticalBox)
						]
					]

					// ACTION NAME AREA
					+SHorizontalBox::Slot()
					.Padding(NodePadding)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Fill)
						[
							SAssignNew(NodeBody, SBorder)
							.BorderImage( FAppStyle::GetBrush("BTEditor.Graph.BTNode.Body") )
							.BorderBackgroundColor(FLinearColor(0.5f, 0.5f, 0.5f, 0.1f))
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							.Visibility(EVisibility::SelfHitTestInvisible)
							[
								SNew(SVerticalBox)
									+SVerticalBox::Slot()
									.HAlign(HAlign_Fill)
									.VAlign(VAlign_Fill)
									[
										SNew(SHorizontalBox)
										+SHorizontalBox::Slot()
										.AutoWidth()
										[
											SNew(SLevelOfDetailBranchNode)
											.UseLowDetailSlot(this, &SGraphAbilityNode_Action::UseLowDetailNodeTitles)
											.LowDetail()
											[
												SNew(SBox)
												.WidthOverride_Lambda(GetNodeTitlePlaceholderWidth)
												.HeightOverride_Lambda(GetNodeTitlePlaceholderHeight)
											]
											.HighDetail()
											[
												SNew(SHorizontalBox)
												+SHorizontalBox::Slot()
												.AutoWidth()
												.VAlign(VAlign_Center)
												[
													SNew(SImage)
													.Visibility(EVisibility::Collapsed)
													.Image(FAppStyle::GetBrush(TEXT("BTEditor.Graph.BTNode.Icon")))
												]
												+SHorizontalBox::Slot()
												.Padding(FMargin(4.0f, 0.0f, 4.0f, 0.0f))
												.VAlign(VAlign_Center)
												[
													SNew(SVerticalBox)
													+SVerticalBox::Slot()
													.AutoHeight()
													[
														SAssignNew(InlineEditableText, SInlineEditableTextBlock)
														.Style( FAppStyle::Get(), "Graph.StateNode.NodeTitleInlineEditableText" )
														.Text( NodeTitle.Get(), &SNodeTitle::GetHeadTitle )
														.OnVerifyTextChanged(this, &SGraphAbilityNode_Action::OnVerifyNameTextChanged)
														.OnTextCommitted(this, &SGraphAbilityNode_Action::OnNameTextCommited)
														.IsReadOnly( this, &SGraphAbilityNode_Action::IsNameReadOnly )
														.IsSelected(this, &SGraphAbilityNode_Action::IsSelectedExclusively)
													]
													+SVerticalBox::Slot()
													.AutoHeight()
													[
														NodeTitle.ToSharedRef()
													]
												]
											]
										]
									]
							]
						]
					]

					// OUTPUT PIN AREA
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.MinDesiredHeight(NodePadding.Bottom)
						[
							SAssignNew(RightNodeBox, SVerticalBox)
						]
					]
				]
			]
		];

	CreatePinWidgets();
}

void SGraphAbilityNode_Action::CreatePinWidgets()
{
	UGraphAbilityNode* MyNode = CastChecked<UGraphAbilityNode>(GraphNode);

	for (int32 PinIdx = 0; PinIdx < MyNode->Pins.Num(); PinIdx++)
	{
		UEdGraphPin* MyPin = MyNode->Pins[PinIdx];
		if (!MyPin->bHidden)
		{
			TSharedPtr<SGraphPin> NewPin = SNew(SGraphAbilityPin, MyPin)
				.ToolTipText(this, &SGraphAbilityNode_Action::GetPinTooltip, MyPin);

			AddPin(NewPin.ToSharedRef());
		}
	}
}

FText SGraphAbilityNode_Action::GetPinTooltip(UEdGraphPin* GraphPinObj) const
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

FText SGraphAbilityNode_Action::GetPinName(const TSharedRef<SGraphPin>& PinToAdd) const
{
	if (PinToAdd->GetPinObj()->PinName != NAME_None)
	{
		return FText::FromName(PinToAdd->GetPinObj()->PinName);
	}

	return FText::FromString(TEXT(""));
}

void SGraphAbilityNode_Action::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
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
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.FillHeight(1.0f)
			.Padding(0.0f, 20.0f)
			[
				PinToAdd
			];
		InputPins.Add(PinToAdd);
	}
	else // Direction == EEdGraphPinDirection::EGPD_Output
	{
		RightNodeBox->AddSlot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.FillHeight(1.0f)
			.Padding(0, 5.0f)
			[
				SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.Padding(FMargin(4.0f, 0.0f))
					[
						SNew(STextBlock)
							.Text_Lambda([this, PinToAdd]() {
								return GetPinName(PinToAdd);
							})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						PinToAdd
					]
			];
		OutputPins.Add(PinToAdd);
	}
}

FSlateColor SGraphAbilityNode_Action::GetBorderBackgroundColor() const
{
	FLinearColor InactiveStateColor(0.08f, 0.08f, 0.08f);
	FLinearColor ActiveStateColorDim(0.4f, 0.3f, 0.15f);
	FLinearColor ActiveStateColorBright(1.f, 0.6f, 0.35f);
	FLinearColor InstancedSelectorColor(0.08f, 0.08f, 0.08f);
	FLinearColor UnInstancedSelectorColor(0.f, 0.f, 0.f);
	/*return GetBorderBackgroundColor_Internal(InactiveStateColor, ActiveStateColorDim, ActiveStateColorBright);*/
	
	UGraphAbilityNode_Action* MyNode = CastChecked<UGraphAbilityNode_Action>(GraphNode);


	return MyNode->NodeInstance ? InstancedSelectorColor : UnInstancedSelectorColor;

}