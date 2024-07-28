#include "Asset/StateAbilityAssetFactory.h"

#include "Component/StateAbility/Script/StateAbilityScript.h"

#define LOCTEXT_NAMESPACE "StoryAssetFactory"

//////////////////////////////////////////////////////////////////////////
// SAS

UStateAbilityScriptFactory::UStateAbilityScriptFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UStateAbilityScriptArchetype::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

bool UStateAbilityScriptFactory::CanCreateNew() const
{
	return true;
}

UObject* UStateAbilityScriptFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UStateAbilityScriptArchetype::StaticClass()));
	UStateAbilityScriptArchetype* ScriptArchetype = NewObject<UStateAbilityScriptArchetype>(InParent, Class, Name, Flags);

	UStateAbilityScriptClass* NewClass = NewObject<UStateAbilityScriptClass>(InParent, UStateAbilityScriptClass::StaticClass(), FName(*FString::Printf(TEXT("%s_C"), *Name.ToString())), RF_Public | RF_Standalone | RF_Transactional);
	ScriptArchetype->GeneratedScriptClass = NewClass;
	
	// This is just used for Blueprint
	//NewClass->ClassGeneratedBy = ScriptArchetype;
	
	NewClass->SetSuperStruct(UStateAbilityScript::StaticClass());

	// Relink the class
	NewClass->Bind();
	NewClass->StaticLink(true);

	UStateAbilityScript* CDO = Cast<UStateAbilityScript>(NewClass->GetDefaultObject(true)); // Force the default object to be constructed if it isn't already
	check(CDO);
	CDO->SetScriptArchetype(ScriptArchetype);

	// 这么做的目的仅仅是为了通知FClassHierarchy更新，将新的ScriptClass塞入。
	GEditor->OnClassPackageLoadedOrUnloaded().Broadcast();

	return ScriptArchetype;
}

#undef LOCTEXT_NAMESPACE