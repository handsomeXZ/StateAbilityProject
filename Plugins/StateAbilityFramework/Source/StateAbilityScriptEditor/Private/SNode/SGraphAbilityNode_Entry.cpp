


#include "SNode/SGraphAbilityNode_Entry.h"

#include "Node/GraphAbilityNode.h"
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



/////////////////////////////////////////////////////
// SGraphAbilityNode_Entry

void SGraphAbilityNode_Entry::Construct(const FArguments& InArgs, UGraphAbilityNode* InNode)
{
	this->GraphNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();
}

void SGraphAbilityNode_Entry::GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const
{

}

FSlateColor SGraphAbilityNode_Entry::GetBorderBackgroundColor() const
{
	FLinearColor InactiveStateColor(0.08f, 0.08f, 0.08f);
	FLinearColor ActiveStateColorDim(0.4f, 0.3f, 0.15f);
	FLinearColor ActiveStateColorBright(1.f, 0.6f, 0.35f);

	return InactiveStateColor;
}

void SGraphAbilityNode_Entry::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();

	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();


	FLinearColor TitleShadowColor(0.6f, 0.6f, 0.6f);

	this->ContentScale.Bind(this, &SGraphNode::GetContentScale);
	this->GetOrAddSlot(ENodeZone::Center)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
		.Padding(0)
		.BorderBackgroundColor(this, &SGraphAbilityNode_Entry::GetBorderBackgroundColor)
		[
			SNew(SOverlay)

			// PIN AREA
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(10.0f)
		[
			SAssignNew(RightNodeBox, SVerticalBox)
		]
		]
		];

	CreatePinWidgets();
}

void SGraphAbilityNode_Entry::CreatePinWidgets()
{
	UGraphAbilityNode* SAGraphNode = CastChecked<UGraphAbilityNode>(GraphNode);

	UEdGraphPin* CurPin = SAGraphNode->GetOutputPin();
	if (!CurPin->bHidden)
	{
		TSharedPtr<SGraphPin> NewPin = SNew(SGraphAbilityPinEntry, CurPin);

		this->AddPin(NewPin.ToSharedRef());
	}
}

void SGraphAbilityNode_Entry::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	PinToAdd->SetOwner(SharedThis(this));
	RightNodeBox->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.FillHeight(1.0f)
		[
			PinToAdd
		];
	OutputPins.Add(PinToAdd);
}

FText SGraphAbilityNode_Entry::GetPreviewCornerText() const
{
	return NSLOCTEXT("SGraphAbilityNode_Entry", "CornerTextDescription", "Entry point for StateAbility");
}
