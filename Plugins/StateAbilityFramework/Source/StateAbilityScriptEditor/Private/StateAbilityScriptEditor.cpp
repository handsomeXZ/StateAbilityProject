// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateAbilityScriptEditor.h"

#define LOCTEXT_NAMESPACE "FStateAbilityScriptEditorModule"

void FStateAbilityScriptEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FStateAbilityScriptEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FStateAbilityScriptEditorModule, StateAbilityScriptEditor)