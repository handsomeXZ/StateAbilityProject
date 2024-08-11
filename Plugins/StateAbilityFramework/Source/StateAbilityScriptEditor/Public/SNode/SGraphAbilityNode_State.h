#pragma once

#include "SNode/SGraphAbilityNodeBase.h"


class UStateTreeBaseNode;
class SScriptStateTreeView;
class FScriptStateTreeViewModel;

class SGraphAbilityNode_State : public SGraphAbilityNodeBase
{
public:
	SLATE_BEGIN_ARGS(SGraphAbilityNode_State) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, class UGraphAbilityNode* InNode);
	virtual ~SGraphAbilityNode_State();

	// SGraphAbilityNodeBase interface
	virtual void UpdateGraphNode() override;
	virtual void CreatePinWidgets() override;
	virtual FSlateColor GetBorderBackgroundColor() const;
	// End of SGraphAbilityNodeBase interface

protected:
	void HandleModelNodeAdded(UStateTreeBaseNode* ParentNode, UStateTreeBaseNode* NewNode) { bIsDirty = true; }
	void HandleModelNodesMoved(const TSet<UStateTreeBaseNode*>& AffectedParents, const TSet<UStateTreeBaseNode*>& MovedNodes) { bIsDirty = true; }
	void HandleDetailsViewChangingProperties(UObject* SelectedObject) { bIsDirty = true; }
	void HandleUpdateStateTree() { bIsDirty = true; }
protected:
	TSharedPtr<SScriptStateTreeView> StateTreeEditor;
	TSharedPtr<FScriptStateTreeViewModel> StateTreeViewModel;
};
