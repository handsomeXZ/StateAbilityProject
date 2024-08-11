#pragma once

#include "SNode/SGraphAbilityNodeBase.h"

class SGraphPin;
struct FGraphInformationPopupInfo;
struct FNodeInfoContext;

class SGraphAbilityNode_Action : public SGraphAbilityNodeBase
{
public:
	SLATE_BEGIN_ARGS(SGraphAbilityNode_Action) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, class UGraphAbilityNode* InNode);
	virtual ~SGraphAbilityNode_Action();

	virtual FSlateColor GetBorderBackgroundColor() const;
private:
	FDelegateHandle OnUpdateGraphNodeHandle;
};
