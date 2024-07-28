#pragma once

#include "Styling/SlateStyle.h"

class ISlateStyle;

class FStateAbilityEditorStyle
	: public FSlateStyleSet
{
public:
	static FStateAbilityEditorStyle& Get();

protected:
	friend class FStateAbilityScriptEditorModule;

	static void Register();
	static void Unregister();

private:
	FStateAbilityEditorStyle();
};
