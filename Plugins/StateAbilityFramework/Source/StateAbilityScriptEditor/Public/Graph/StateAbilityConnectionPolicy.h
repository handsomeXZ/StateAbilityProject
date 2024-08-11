#pragma once

#include "ConnectionDrawingPolicy.h"
#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Layout/ArrangedWidget.h"
#include "Math/Vector2D.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SWidget.h"

class FArrangedChildren;
class FArrangedWidget;
class FSlateRect;
class FSlateWindowElementList;
class SWidget;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
struct FGeometry;

class FStateAbilityConnectionPolicy : public FConnectionDrawingPolicy
{
public:
	FStateAbilityConnectionPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj);

	// FConnectionDrawingPolicy interface 
	virtual void DrawSplineWithArrow(const FVector2D& StartPoint, const FVector2D& EndPoint, const FConnectionParams& Params) override;
	virtual void SetIncompatiblePinDrawState(const TSharedPtr<SGraphPin>& StartPin, const TSet< TSharedRef<SWidget> >& VisiblePins);
	virtual void ResetIncompatiblePinDrawState(const TSet< TSharedRef<SWidget> >& VisiblePins);
	// End of FConnectionDrawingPolicy interface

protected:
	void FixupOverlayPinPosition(FVector2D& StartPoint, FVector2D& EndPoint, const FConnectionParams& Params);

	// 
	static TMap<UEdGraphPin*, TWeakPtr<SGraphPin>> CachePinMap;
};
