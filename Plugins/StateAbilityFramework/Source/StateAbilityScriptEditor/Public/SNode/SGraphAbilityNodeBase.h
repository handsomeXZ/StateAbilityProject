#pragma once

#include "Containers/Array.h"
#include "Internationalization/Text.h"
#include "SGraphNode.h"
#include "SGraphPin.h"
#include "Styling/SlateColor.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SGraphPin;
struct FGraphInformationPopupInfo;
struct FNodeInfoContext;

class SGraphAbilityNodeBase : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphAbilityNodeBase) {}
	SLATE_END_ARGS()

	static const float DefaultMinNodeWidth;
	static const float DefaultMinNodeHeight;

	void Construct(const FArguments& InArgs, class UGraphAbilityNode* InNode);
	virtual ~SGraphAbilityNodeBase();

	// SNodePanel::SNode interface
	virtual void GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const override;
	virtual TArray<FOverlayWidgetInfo> GetOverlayWidgets(bool bSelected, const FVector2D& WidgetSize) const override;
	// End of SNodePanel::SNode interface

	// SGraphNode interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual void UpdateGraphNode() override;
	virtual void CreatePinWidgets() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	// End of SGraphNode interface

	virtual FText GetPinTooltip(UEdGraphPin* GraphPinObj) const;
	virtual FText GetPinName(const TSharedRef<SGraphPin>& PinToAdd) const;
	virtual FSlateColor GetBorderBackgroundColor() const;

protected:
	virtual void ReGeneratePinWidgets();

protected:
	bool bIsDirty;

	TSharedPtr<SHorizontalBox> OutputPinBox;
	/** The node body widget, cached here so we can determine its size when we want ot position our overlays */
	TSharedPtr<SBorder> NodeName;
	TSharedPtr<SHorizontalBox> NodeBottomPanel;
	TSharedPtr<SWidget> LeftOverrideWidget;
	TSharedPtr<SWidget> RightOverrideWidget;
};
