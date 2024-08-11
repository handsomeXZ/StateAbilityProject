#pragma once
#include "SGraphPin.h"
#include "Containers/Array.h"
#include "Internationalization/Text.h"
#include "Styling/SlateColor.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SGraphNode;

/////////////////////////////////////////////////////
// SGraphAbilityPin
class SGraphAbilityPin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphAbilityPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<SGraphNode> InOwnerNode, UEdGraphPin* InPin, bool InbIsOverlayPin = true);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/**
	 * 仅使用于通过SNodePanel::SNode::GetOverlayWidgets() 所添加的SGraphPin。
	 * 因为通过GetOverlayWidgets()创建的Pin，其使用的是在SGraphNode中ArrangeChildren时分配的Geometry，
	 * 而Connetion用的是SGraphPanel分配的Geometry，这会导致默认的Connetion规则得到错误的位置信息，目前已在 FStateAbilityConnectionPolicy进行修复处理。
	 */
	virtual bool IsOverlayPin() { return bIsOverlayPin; }
	virtual FVector2D GetOverlayPosition() const;
	virtual float GetOverlayScale() const;
protected:
	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	TOptional<EMouseCursor::Type> GetAbilityPinCursor() const;
	virtual FSlateColor GetTargetPinColor(TSharedPtr<SWidget> Widget) const;
	virtual const FSlateBrush* GetPinBorder() const;
protected:
	TWeakPtr<SGraphNode> OwnerNode;

	bool bIsOverlayPin = false;
	mutable FVector2f CacheAllottedGeometry_Position = FVector2f::ZeroVector;
	mutable float CacheAllottedGeometry_Scale = 1.0f;
};

/////////////////////////////////////////////////////
// SGraphSDTPinEntry

class SGraphAbilityPinEntry : public SGraphAbilityPin
{
public:
	SLATE_BEGIN_ARGS(SGraphAbilityPinEntry) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);
	virtual bool IsOverlayPin() override { return false; }
protected:
	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	virtual const FSlateBrush* GetPinIcon() const override;
	//~ End SGraphPin Interface

	void CachePinIcons();

	const FSlateBrush* CachedImg_Pin_ConnectedHovered;
	const FSlateBrush* CachedImg_Pin_DisconnectedHovered;
};

/////////////////////////////////////////////////////
// GraphAbilityStackedPin
class SGraphAbilityStackedPin : public SGraphAbilityPin
{
public:
	SLATE_BEGIN_ARGS(SGraphAbilityStackedPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<SGraphNode> InOwnerNode, UEdGraphPin* InPin, struct FStackedPinInfo& StackedPinInfo);

protected:
	FReply OnStackedPinMouseDown(const FGeometry& SenderGeometry, const FPointerEvent& MouseEvent);
	FSlateColor GetStackedPinColor(TSharedPtr<SWidget> Widget) const;
protected:
	TSharedPtr<SHorizontalBox> PinPanel;
};