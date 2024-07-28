#include "Factory/SAGraphFactory.h"

#include "SNode/SGraphNode_SASEntry.h"
#include "SNode/SGraphNode_SASAction.h"

#include "Node/SASGraphNode_Entry.h"
#include "Node/SASGraphNode_Action.h"

#include "StateAbilityScriptEdGraphSchema.h"
//#include "Factory/SAConnectionDrawingPolicy.h"

TSharedPtr<SGraphNode> FSAGraphNodeFactory::CreateNode(UEdGraphNode* Node) const
{
	//SAS
	if (USASGraphNode_Entry* SASNode = Cast<USASGraphNode_Entry>(Node))
	{
		return SNew(SGraphNode_SASEntry, SASNode);
	}

	if (USASGraphNode_Action* SASNode = Cast<USASGraphNode_Action>(Node))
	{
		return SNew(SGraphNode_SASAction, SASNode);
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