
#include "SNode/SGraphAbilityNode_State.h"

#include "Node/GraphAbilityNode.h"
#include "Node/GraphAbilityNode_State.h"
#include "SNode/SGraphAbilityPin.h"
#include "StateTree/ScriptStateTreeView.h"

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
// SGraphAbilityNode_State
void SGraphAbilityNode_State::Construct(const FArguments& InArgs, UGraphAbilityNode* InNode)
{
	GraphNode = InNode;

	UGraphAbilityNode_State* StateGraphNode = CastChecked<UGraphAbilityNode_State>(GraphNode);

	StateTreeViewModel = StateGraphNode->GetOrCreateStateTreeViewModel();

	StateTreeViewModel->OnNodeAdded_Delegate.AddSP(this, &SGraphAbilityNode_State::HandleModelNodeAdded);
	StateTreeViewModel->OnNodesMoved_Delegate.AddSP(this, &SGraphAbilityNode_State::HandleModelNodesMoved);
	StateTreeViewModel->OnDetailsViewChangingProperties_Delegate.AddSP(this, &SGraphAbilityNode_State::HandleDetailsViewChangingProperties);
	StateTreeViewModel->OnUpdateStateTree_Delegate.AddSP(this, &SGraphAbilityNode_State::HandleUpdateStateTree);
	StateTreeViewModel->OnUpdateStateNode_Delegate.AddSP(this, &SGraphAbilityNode_State::UpdateGraphNode);
	StateTreeViewModel->OnUpdateStateNodePin_Delegate.AddSP(this, &SGraphAbilityNode_State::ReGeneratePinWidgets);


	SGraphAbilityNodeBase::Construct(SGraphAbilityNodeBase::FArguments(), InNode);
}

SGraphAbilityNode_State::~SGraphAbilityNode_State()
{
	if (StateTreeViewModel.IsValid())
	{
		StateTreeViewModel->OnNodeAdded_Delegate.RemoveAll(this);
		StateTreeViewModel->OnNodesMoved_Delegate.RemoveAll(this);
		StateTreeViewModel->OnDetailsViewChangingProperties_Delegate.RemoveAll(this);
		StateTreeViewModel->OnUpdateStateTree_Delegate.RemoveAll(this);
		StateTreeViewModel->OnUpdateStateNode_Delegate.RemoveAll(this);
		StateTreeViewModel->OnUpdateStateNodePin_Delegate.RemoveAll(this);
	}
}

void SGraphAbilityNode_State::UpdateGraphNode()
{
	SGraphAbilityNodeBase::UpdateGraphNode();

	NodeBottomPanel->ClearChildren();
	NodeBottomPanel->AddSlot()
	[
		SAssignNew(StateTreeEditor, SScriptStateTreeView, StateTreeViewModel)
	];
}

void SGraphAbilityNode_State::CreatePinWidgets()
{
	UGraphAbilityNode_State* StateNode = CastChecked<UGraphAbilityNode_State>(GraphNode);

	for (int32 PinIdx = 0; PinIdx < StateNode->Pins.Num(); PinIdx++)
	{
		UEdGraphPin* OutPin = StateNode->Pins[PinIdx];
		if (!OutPin->bHidden)
		{
			TSharedPtr<SGraphPin> NewPin = nullptr;

			FStackedPinInfo* StackedPinInfoPtr = StateNode->FindStackedPinInfo(OutPin);
			if (!StackedPinInfoPtr)
			{
				NewPin = SNew(SGraphAbilityPin, SharedThis(this), OutPin)
					.ToolTipText(this, &SGraphAbilityNode_State::GetPinTooltip, OutPin);
			}
			else
			{
				NewPin = SNew(SGraphAbilityStackedPin, SharedThis(this), OutPin, *StackedPinInfoPtr)
					.ToolTipText(this, &SGraphAbilityNode_State::GetPinTooltip, OutPin);
			}

			AddPin(NewPin.ToSharedRef());
		}
	}
}

FSlateColor SGraphAbilityNode_State::GetBorderBackgroundColor() const
{
	FLinearColor InactiveStateColor(0.08f, 0.08f, 0.08f);
	FLinearColor ActiveStateColorDim(0.4f, 0.3f, 0.15f);
	FLinearColor ActiveStateColorBright(1.f, 0.6f, 0.35f);
	FLinearColor InstancedSelectorColor(0.08f, 0.08f, 0.08f);
	FLinearColor UnInstancedSelectorColor(0.f, 0.f, 0.f);
	/*return GetBorderBackgroundColor_Internal(InactiveStateColor, ActiveStateColorDim, ActiveStateColorBright);*/

	UGraphAbilityNode_State* MyNode = CastChecked<UGraphAbilityNode_State>(GraphNode);


	return MyNode->NodeInstance ? InstancedSelectorColor : UnInstancedSelectorColor;

}