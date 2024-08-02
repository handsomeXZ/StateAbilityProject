#include "Node/GraphAbilityNode_Entry.h"

#include "Node/StateAbilityEditorTypes.h"

#define LOCTEXT_NAMESPACE "GraphAbilityNode_Entry"


void UGraphAbilityNode_Entry::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, UStateAbilityEditorTypes::PinCategory_Entry, TEXT("Entry"));
}

FText UGraphAbilityNode_Entry::GetTooltipText() const
{
	return LOCTEXT("SASNodeTooltip", "Entry point for State Ability Script");
}

UEdGraphNode* UGraphAbilityNode_Entry::GetOutputNode() const
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

UEdGraphPin* UGraphAbilityNode_Entry::GetOutputPin() const
{
	Pins[0]->PinToolTip = LOCTEXT("SASNodeTooltip", "Out").ToString();
	return Pins[0];
}


#undef LOCTEXT_NAMESPACE