#pragma once

#include "CoreMinimal.h"

#include "StateAbilityEditorTypes.generated.h"

// Maximum distance a drag can be off a node edge to require 'push off' from node
const int32 NodeDistance = 60;

namespace StateAbilityClassUtils
{
	FString ClassToString(UClass* InClass);
}

UCLASS()
class UStateAbilityEditorTypes : public UObject
{
	GENERATED_UCLASS_BODY()

	static const FName PinCategory_Defualt;
	static const FName PinCategory_Entry;
	static const FName PinCategory_Exit;
	static const FName PinCategory_Exec;
	static const FName PinCategory_Event;
};
