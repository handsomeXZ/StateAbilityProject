#include "Node/StateAbilityEditorTypes.h"

#include "UObject/NameTypes.h"



const FName UStateAbilityEditorTypes::PinCategory_Defualt("DefualtNode");
const FName UStateAbilityEditorTypes::PinCategory_Entry("EntryNode");
const FName UStateAbilityEditorTypes::PinCategory_Exit("ExitNode");
const FName UStateAbilityEditorTypes::PinCategory_Action("ActionNode");
UStateAbilityEditorTypes::UStateAbilityEditorTypes(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

FString StateAbilityClassUtils::ClassToString(UClass* InClass)
{
	FString ShortName = InClass ? InClass->GetMetaData(TEXT("DisplayName")) : FString();
	if (!ShortName.IsEmpty())
	{
		return ShortName;
	}

	if (InClass)
	{
		FString ClassDesc = InClass->GetName();

		if (InClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
		{
			return ClassDesc.LeftChop(2);
		}

		const int32 ShortNameIdx = ClassDesc.Find(TEXT("_"), ESearchCase::CaseSensitive);
		if (ShortNameIdx != INDEX_NONE)
		{
			ClassDesc.MidInline(ShortNameIdx + 1, MAX_int32, false);
		}

		return ClassDesc;
	}

	return TEXT("Selector");
}
