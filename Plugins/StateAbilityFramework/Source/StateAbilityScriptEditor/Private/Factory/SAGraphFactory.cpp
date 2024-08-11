#include "Factory/SAGraphFactory.h"

#include "SNode/SGraphAbilityNode_Entry.h"
#include "SNode/SGraphAbilityNode_Action.h"
#include "SNode/SGraphAbilityNode_State.h"

#include "Node/GraphAbilityNode_Entry.h"
#include "Node/GraphAbilityNode_Action.h"
#include "Node/GraphAbilityNode_State.h"

#include "StateAbilityScriptEdGraphSchema.h"
#include "Graph/StateAbilityConnectionPolicy.h"
//#include "Factory/SAConnectionDrawingPolicy.h"

TSharedPtr<SGraphNode> FSAGraphNodeFactory::CreateNode(UEdGraphNode* Node) const
{
	//SAS
	if (UGraphAbilityNode_Entry* Entry_Node = Cast<UGraphAbilityNode_Entry>(Node))
	{
		return SNew(SGraphAbilityNode_Entry, Entry_Node);
	}

	if (UGraphAbilityNode_Action* Action_Node = Cast<UGraphAbilityNode_Action>(Node))
	{
		return SNew(SGraphAbilityNode_Action, Action_Node);
	}

	if (UGraphAbilityNode_State* State_Node = Cast<UGraphAbilityNode_State>(Node))
	{
		return SNew(SGraphAbilityNode_State, State_Node);
	}

	return nullptr;
}

FConnectionDrawingPolicy* FSAPinConnectionFactory::CreateConnectionPolicy(const class UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const class FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const
{
	return new FStateAbilityConnectionPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}