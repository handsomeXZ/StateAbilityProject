// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateAbilityScriptRuntime.h"

#include "Attribute/Reactive/AttributeRegistry.h"

#define LOCTEXT_NAMESPACE "FStateAbilityScriptRuntimeModule"

void FStateAbilityScriptRuntimeModule::StartupModule()
{
	FModuleManager::Get().OnModulesChanged().AddRaw(this, &FStateAbilityScriptRuntimeModule::ProcessReactiveRegistry);
	Attribute::Reactive::FReactiveRegistry::ProcessPendingRegistrations();
}

void FStateAbilityScriptRuntimeModule::ShutdownModule()
{
	FModuleManager::Get().OnModulesChanged().RemoveAll(this);
}

void FStateAbilityScriptRuntimeModule::ProcessReactiveRegistry(FName InModule, EModuleChangeReason InReason)
{
	if (InReason == EModuleChangeReason::ModuleLoaded)
	{
		Attribute::Reactive::FReactiveRegistry::ProcessPendingRegistrations();
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FStateAbilityScriptRuntimeModule, StateAbilityScriptRuntime)