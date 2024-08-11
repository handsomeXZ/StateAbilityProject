


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
	SGraphAbilityNodeBase::Construct(SGraphAbilityNodeBase::FArguments(), InNode);

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
