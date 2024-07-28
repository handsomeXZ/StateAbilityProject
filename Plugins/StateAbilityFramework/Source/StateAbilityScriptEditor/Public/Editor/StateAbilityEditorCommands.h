// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "Templates/SharedPointer.h"


class FUICommandInfo;

class FStateAbilityEditorCommonCommands : public TCommands<FStateAbilityEditorCommonCommands>
{
public:
	FStateAbilityEditorCommonCommands();

	TSharedPtr<FUICommandInfo> NewScriptArchetype;

	//create a comment node
	TSharedPtr< FUICommandInfo > CreateComment;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};

