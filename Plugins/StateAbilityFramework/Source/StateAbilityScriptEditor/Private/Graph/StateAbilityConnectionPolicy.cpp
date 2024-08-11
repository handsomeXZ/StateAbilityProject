#include "Graph/StateAbilityConnectionPolicy.h"

#include "Containers/EnumAsByte.h"
#include "Containers/Set.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/ArrangedWidget.h"
#include "Layout/PaintGeometry.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/Optional.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingCommon.h"
#include "SGraphNode.h"
#include "Styling/SlateBrush.h"
#include "SGraphPin.h"

#include "SNode/SGraphAbilityPin.h"

TMap<UEdGraphPin*, TWeakPtr<SGraphPin>> FStateAbilityConnectionPolicy::CachePinMap;

FStateAbilityConnectionPolicy::FStateAbilityConnectionPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
	: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements)
{
}


void FStateAbilityConnectionPolicy::DrawSplineWithArrow(const FVector2D& StartPoint, const FVector2D& EndPoint, const FConnectionParams& Params)
{
	FVector2D RealStartPoint = StartPoint;
	FVector2D RealEndPoint = EndPoint;

	FixupOverlayPinPosition(RealStartPoint, RealEndPoint, Params);

	FConnectionDrawingPolicy::DrawSplineWithArrow(RealStartPoint, RealEndPoint, Params);
}

void FStateAbilityConnectionPolicy::SetIncompatiblePinDrawState(const TSharedPtr<SGraphPin>& StartPin, const TSet<TSharedRef<SWidget>>& VisiblePins)
{
	FConnectionDrawingPolicy::SetIncompatiblePinDrawState(StartPin, VisiblePins);
	CachePinMap.Add(StartPin->GetPinObj(), StartPin.ToWeakPtr());
}

void FStateAbilityConnectionPolicy::ResetIncompatiblePinDrawState(const TSet<TSharedRef<SWidget>>& VisiblePins)
{
	FConnectionDrawingPolicy::ResetIncompatiblePinDrawState(VisiblePins);
	CachePinMap.Empty();
}

void FStateAbilityConnectionPolicy::FixupOverlayPinPosition(FVector2D& StartPoint, FVector2D& EndPoint, const FConnectionParams& Params)
{

	if (Params.AssociatedPin1 && !Params.AssociatedPin1->IsPendingKill())
	{
		TSharedPtr<SGraphPin>* Pin1_Widget = PinToPinWidgetMap.Find(Params.AssociatedPin1);
		TWeakPtr<SGraphPin>* Pin1_WidgetCache = CachePinMap.Find(Params.AssociatedPin1);

		TSharedPtr<SGraphAbilityPin> GraphAbilityPin;
		if (Pin1_Widget)
		{
			GraphAbilityPin = StaticCastSharedPtr<SGraphAbilityPin>(*Pin1_Widget);
		}
		else if (Pin1_WidgetCache && Pin1_WidgetCache->IsValid())
		{
			GraphAbilityPin = StaticCastSharedPtr<SGraphAbilityPin>(Pin1_WidgetCache->Pin());
		}


		if (GraphAbilityPin.IsValid())
		{
			if (GraphAbilityPin->IsOverlayPin())
			{
				FVector2D Offset = FVector2D(GraphAbilityPin.Get()->GetDesiredSize().X, GraphAbilityPin.Get()->GetDesiredSize().Y * 0.5) * GraphAbilityPin->GetOverlayScale();
				StartPoint = GraphAbilityPin->GetOverlayPosition() + Offset;
			}
		}
	}

	if (Params.AssociatedPin2 && !Params.AssociatedPin2->IsPendingKill())
	{
		TSharedPtr<SGraphPin>* Pin2_Widget = PinToPinWidgetMap.Find(Params.AssociatedPin2);
		TWeakPtr<SGraphPin>* Pin2_WidgetCache = CachePinMap.Find(Params.AssociatedPin2);

		TSharedPtr<SGraphAbilityPin> GraphAbilityPin;
		if (Pin2_Widget)
		{
			GraphAbilityPin = StaticCastSharedPtr<SGraphAbilityPin>(*Pin2_Widget);
		}
		else if (Pin2_WidgetCache && Pin2_WidgetCache->IsValid())
		{
			GraphAbilityPin = StaticCastSharedPtr<SGraphAbilityPin>(Pin2_WidgetCache->Pin());
		}

		if (GraphAbilityPin.IsValid())
		{
			if (GraphAbilityPin->IsOverlayPin())
			{
				FVector2D Offset = FVector2D(0, GraphAbilityPin.Get()->GetDesiredSize().Y * 0.5) * GraphAbilityPin->GetOverlayScale();
				EndPoint = GraphAbilityPin->GetOverlayPosition() + Offset;
			}
		}
	}

}