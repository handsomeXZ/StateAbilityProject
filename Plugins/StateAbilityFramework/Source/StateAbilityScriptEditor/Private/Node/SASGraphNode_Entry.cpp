#include "Node/SASGraphNode_Entry.h"

#include "Node/SAEditorTypes.h"

#define LOCTEXT_NAMESPACE "SASGraphNode_Entry"


void USASGraphNode_Entry::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, USASEditorTypes::PinCategory_Entry, TEXT("Entry"));
}

FText USASGraphNode_Entry::GetTooltipText() const
{
	return LOCTEXT("SASNodeTooltip", "Entry point for State Ability Script");
}

UEdGraphNode* USASGraphNode_Entry::GetOutputNode() const
{
	if (Pins.Num() > 0 && Pins[0] != NULL)
	{
		check(Pins[0]->LinkedTo.Num() <= 1);
		if (Pins[0]->LinkedTo.Num() > 0 && Pins[0]->LinkedTo[0]->GetOwningNode() != NULL)
		{
			return Pins[0]->LinkedTo[0]->GetOwningNode();
		}
	}
	return NULL;
}

UEdGraphPin* USASGraphNode_Entry::GetOutputPin() const
{
	Pins[0]->PinToolTip = LOCTEXT("SASNodeTooltip", "Out").ToString();
	return Pins[0];
}


#undef LOCTEXT_NAMESPACE