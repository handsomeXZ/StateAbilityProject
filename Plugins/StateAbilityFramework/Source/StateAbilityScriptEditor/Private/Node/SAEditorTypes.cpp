#include "Node/SAEditorTypes.h"

#include "UObject/NameTypes.h"



const FName USASEditorTypes::PinCategory_Defualt("DefualtNode");
const FName USASEditorTypes::PinCategory_Entry("EntryNode");
const FName USASEditorTypes::PinCategory_Exit("ExitNode");
const FName USASEditorTypes::PinCategory_Action("ActionNode");
USASEditorTypes::USASEditorTypes(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

FString SASClassUtils::ClassToString(UClass* InClass)
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
