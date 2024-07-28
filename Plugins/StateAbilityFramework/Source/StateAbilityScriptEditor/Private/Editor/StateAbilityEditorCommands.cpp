// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/StateAbilityEditorCommands.h"

#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/UICommandInfo.h"
#include "GenericPlatform/GenericApplication.h"
#include "InputCoreTypes.h"
#include "Internationalization/Internationalization.h"
#include "Styling/AppStyle.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

#define LOCTEXT_NAMESPACE "StateAbilityEditorCommands"

FStateAbilityEditorCommonCommands::FStateAbilityEditorCommonCommands()
	: TCommands<FStateAbilityEditorCommonCommands>("StateAbilityEditor.Common", LOCTEXT("StateAbilityEditorCommandLabel", "StateAbilityEditor"), NAME_None, FAppStyle::GetAppStyleSetName())
{
}

void FStateAbilityEditorCommonCommands::RegisterCommands()
{
	UI_COMMAND(NewScriptArchetype, "New State Ability Script Archetype", "Create a new State Ability Script Archetype Asset", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(CreateComment, "Create Comment", "Create a comment box", EUserInterfaceActionType::Button, FInputChord(EKeys::C))
}


#undef LOCTEXT_NAMESPACE
