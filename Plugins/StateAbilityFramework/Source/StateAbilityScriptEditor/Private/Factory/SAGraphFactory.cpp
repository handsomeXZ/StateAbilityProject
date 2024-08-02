#include "Factory/SAGraphFactory.h"

#include "SNode/SGraphAbilityNode_Entry.h"
#include "SNode/SGraphAbilityNode_Action.h"

#include "Node/GraphAbilityNode_Entry.h"
#include "Node/GraphAbilityNode_Action.h"

#include "StateAbilityScriptEdGraphSchema.h"
//#include "Factory/SAConnectionDrawingPolicy.h"

TSharedPtr<SGraphNode> FSAGraphNodeFactory::CreateNode(UEdGraphNode* Node) const
{
	//SAS
	if (UGraphAbilityNode_Entry* SASNode = Cast<UGraphAbilityNode_Entry>(Node))
	{
		return SNew(SGraphAbilityNode_Entry, SASNode);
	}

	if (UGraphAbilityNode_Action* SASNode = Cast<UGraphAbilityNode_Action>(Node))
	{
		return SNew(SGraphAbilityNode_Action, SASNode);
	}

	return nullptr;
}

FConnectionDrawingPolicy* FSAPinConnectionFactory::CreateConnectionPolicy(const class UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const class FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const
{
	if (Schema->IsA(UStateAbilityScriptEdGraphSchema::StaticClass()))
	{
		//return new FSCTConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj);
	}

	return nullptr;
}