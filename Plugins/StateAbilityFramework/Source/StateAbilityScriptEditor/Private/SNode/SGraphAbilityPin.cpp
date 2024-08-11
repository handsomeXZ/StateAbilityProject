#include "SNode/SGraphAbilityPin.h"

#include "SGraphPanel.h"
#include "SGraphNode.h"
#include "Layout/Geometry.h"

#include "Node/GraphAbilityNode_State.h"
#include "Editor/StateAbilityEditorStyle.h"

void SGraphAbilityPin::Construct(const FArguments& InArgs, TSharedPtr<SGraphNode> InOwnerNode, UEdGraphPin* InPin, bool InbIsOverlayPin /* = true */)
{
	OwnerNode = InOwnerNode;
	bIsOverlayPin = InbIsOverlayPin;

	bShowLabel = true;

	GraphPinObj = InPin;
	check(GraphPinObj != NULL);

	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
	check(Schema);

	TSharedPtr<SGraphAbilityPin> SelfWidget = SharedThis(this);

	SBorder::Construct(SBorder::FArguments()
		.BorderImage(this, &SGraphAbilityPin::GetPinBorder)
		.BorderBackgroundColor_Lambda([SelfWidget, this]() {
			if (SelfWidget.IsValid())
			{
				return GetTargetPinColor(SelfWidget);
			}

			return FSlateColor(FLinearColor::Transparent);
		})
		.OnMouseButtonDown(this, &SGraphAbilityPin::OnPinMouseDown)
		.Cursor(this, &SGraphAbilityPin::GetAbilityPinCursor)
		.Padding(FMargin(10.0f))
	);
}

int32 SGraphAbilityPin::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	CacheAllottedGeometry_Position = AllottedGeometry.AbsolutePosition;
	CacheAllottedGeometry_Scale = AllottedGeometry.Scale;

	return SGraphPin::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

TSharedRef<SWidget>	SGraphAbilityPin::GetDefaultValueWidget()
{
	return SNew(STextBlock);
}

const FSlateBrush* SGraphAbilityPin::GetPinBorder() const
{
	return FStateAbilityEditorStyle::Get().GetBrush("Pin.Point");
}

FSlateColor SGraphAbilityPin::GetTargetPinColor(TSharedPtr<SWidget> Widget) const
{
	if (!Widget.IsValid())
	{
		return FSlateColor(COLOR("218c74AF"));
	}

	bool bHightLight = Widget->IsHovered() || !GraphPinObj->LinkedTo.IsEmpty();

	return FSlateColor(bHightLight ? COLOR("33d9b2FF") : COLOR("218c74AF"));
}

TOptional<EMouseCursor::Type> SGraphAbilityPin::GetAbilityPinCursor() const
{
	if (IsHovered())
	{
		if (bIsMovingLinks)
		{
			return EMouseCursor::GrabHandClosed;
		}
		else
		{
			return EMouseCursor::Crosshairs;
		}
	}
	else
	{
		return EMouseCursor::Default;
	}
}

FVector2D SGraphAbilityPin::GetOverlayPosition() const
{
	return FVector2D(CacheAllottedGeometry_Position);
}

float SGraphAbilityPin::GetOverlayScale() const
{
	return CacheAllottedGeometry_Scale;
}

/////////////////////////////////////////////////////
// SGraphAbilityPinEntry

void SGraphAbilityPinEntry::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InPin);

	// Call utility function so inheritors can also call it since arguments can't be passed through
	CachePinIcons();
}

void SGraphAbilityPinEntry::CachePinIcons()
{
	CachedImg_Pin_ConnectedHovered = FAppStyle::GetBrush(TEXT("Graph.ExecPin.ConnectedHovered"));
	CachedImg_Pin_Connected = FAppStyle::GetBrush(TEXT("Graph.ExecPin.Connected"));
	CachedImg_Pin_DisconnectedHovered = FAppStyle::GetBrush(TEXT("Graph.ExecPin.DisconnectedHovered"));
	CachedImg_Pin_Disconnected = FAppStyle::GetBrush(TEXT("Graph.ExecPin.Disconnected"));
}

TSharedRef<SWidget>	SGraphAbilityPinEntry::GetDefaultValueWidget()
{
	return SNew(SSpacer);
}

const FSlateBrush* SGraphAbilityPinEntry::GetPinIcon() const
{
	const FSlateBrush* Brush = nullptr;

	if (IsConnected())
	{
		Brush = IsHovered() ? CachedImg_Pin_ConnectedHovered : CachedImg_Pin_Connected;
	}
	else
	{
		Brush = IsHovered() ? CachedImg_Pin_DisconnectedHovered : CachedImg_Pin_Disconnected;
	}

	return Brush;
}

//////////////////////////////////////////////////////////////////////////
// SGraphAbilityStackedPin
void SGraphAbilityStackedPin::Construct(const FArguments& InArgs, TSharedPtr<SGraphNode> InOwnerNode, UEdGraphPin* InPin, struct FStackedPinInfo& StackedPinInfo)
{
	bIsOverlayPin = true;
	OwnerNode = InOwnerNode;

	SBorder::Construct(SBorder::FArguments()
		.Padding(0)
		.BorderBackgroundColor(FLinearColor::Transparent)
		[
			SAssignNew(PinPanel, SHorizontalBox)
		]
	);

	TSharedPtr<SGraphAbilityStackedPin> SelfWidget = SharedThis(this);

	for (auto& PinInfo : StackedPinInfo.StackedEventNode)
	{
		TSharedPtr<SHorizontalBox> StackedPinWidget = SNew(SHorizontalBox);

		TSharedPtr<STextBlock> PinNameWidget = SNew(STextBlock)
			.Visibility(EVisibility::Collapsed)
			.Text(FText::FromString(PinInfo.Value.ToString()));

		StackedPinWidget->AddSlot()
		.AutoWidth()
		[
			SNew(SBorder)
			.Padding(FMargin(10.0f))
			.BorderImage(this, &SGraphAbilityStackedPin::GetPinBorder)
			.BorderBackgroundColor_Lambda([StackedPinWidget, SelfWidget, this]() {
				if (SelfWidget.IsValid())
				{
					return GetStackedPinColor(StackedPinWidget);
				}
				return FSlateColor(FLinearColor::Transparent);
			})
		];
		StackedPinWidget->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			PinNameWidget.ToSharedRef()
		];

		PinPanel->AddSlot()
		.AutoWidth()
		[
			SNew(SBorder)
			.Padding(0)
			.BorderBackgroundColor(FLinearColor::Transparent)
			.OnMouseButtonDown_Lambda([this, PinNameWidget](const FGeometry& SenderGeometry, const FPointerEvent& MouseEvent) {
				if (!PinNameWidget.IsValid())
				{
					return FReply::Unhandled();
				}
				
				if (PinNameWidget->GetVisibility() == EVisibility::Collapsed)
				{
					PinNameWidget->SetVisibility(EVisibility::HitTestInvisible);
				}
				else
				{
					PinNameWidget->SetVisibility(EVisibility::Collapsed);
				}

				OnStackedPinMouseDown(SenderGeometry, MouseEvent);
				return FReply::Unhandled();
			})
			[
				StackedPinWidget.ToSharedRef()
			]
		];

		// @TODO：缺少显示当前Pin连接的StateTreeNode
	}

	bShowLabel = true;

	GraphPinObj = InPin;
	check(GraphPinObj != NULL);

	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
	check(Schema);

	TSharedPtr<SHorizontalBox> PinWidget = SNew(SHorizontalBox);

	PinWidget->AddSlot()
	.AutoWidth()
	[
		SNew(SBorder)
			.BorderImage(this, &SGraphAbilityStackedPin::GetPinBorder)
			.BorderBackgroundColor_Lambda([PinWidget, SelfWidget, this]() {
				if (SelfWidget.IsValid())
				{
					return GetTargetPinColor(PinWidget);
				}
				return FSlateColor(FLinearColor::Transparent);
			})
			.OnMouseButtonDown(this, &SGraphAbilityStackedPin::OnPinMouseDown)
			.Cursor(this, &SGraphAbilityStackedPin::GetAbilityPinCursor)
			.Padding(FMargin(10.0f))
	];
	

	PinPanel->AddSlot()
	.AutoWidth()
	[
		PinWidget.ToSharedRef()
	];
}

FSlateColor SGraphAbilityStackedPin::GetStackedPinColor(TSharedPtr<SWidget> Widget) const
{
	if (!Widget.IsValid())
	{
		return FSlateColor(COLOR("84817aFF"));
	}

	return FSlateColor(Widget->IsHovered() ? COLOR("d1ccc0FF") : COLOR("84817aFF"));
}

FReply SGraphAbilityStackedPin::OnStackedPinMouseDown(const FGeometry& SenderGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Unhandled();
}