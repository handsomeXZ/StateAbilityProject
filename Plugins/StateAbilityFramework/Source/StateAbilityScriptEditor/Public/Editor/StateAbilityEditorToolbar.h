// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class FStateAbilityEditor;
class FExtender;
class FToolBarBuilder;

class FStateAbilityEditorToolbar : public TSharedFromThis<FStateAbilityEditorToolbar>
{
public:
	FStateAbilityEditorToolbar(TSharedPtr<FStateAbilityEditor> InEditor)
		: SAEditor(InEditor) {}

	void AddModesToolbar(TSharedPtr<FExtender> Extender);
	void AddToolbar(TSharedPtr<FExtender> Extender);
private:
	void FillModesToolbar(FToolBarBuilder& ToolbarBuilder);
	void FillToolbar(FToolBarBuilder& ToolbarBuilder);

protected:
	/** Pointer back to the blueprint editor tool that owns us */
	TWeakPtr<FStateAbilityEditor> SAEditor;
};
