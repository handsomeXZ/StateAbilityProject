#include "Asset/AssetTypeActions_StateAbilityAsset.h"
#include "StateAbilityScriptEditor.h"
#include "Component/StateAbility/Script/StateAbilityScript.h"
#include "Component/StateAbility/Script/StateAbilityScriptArchetype.h"

#include "Editor/StateAbilityEditorStyle.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

//////////////////////////////////////////////////////////////////////////
// SDT

FText FAssetTypeActions_StateAbilityScript::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_StateAbilityScript", "ScriptArchetype");
}

uint32 FAssetTypeActions_StateAbilityScript::GetCategories()
{
	FStateAbilityScriptEditorModule& CSCModule = FModuleManager::LoadModuleChecked<FStateAbilityScriptEditorModule>("StateAbilityScriptEditor");
	return CSCModule.GetAssetCategoryBit();
}

UClass* FAssetTypeActions_StateAbilityScript::GetSupportedClass() const
{
	return UStateAbilityScriptArchetype::StaticClass();
}

void FAssetTypeActions_StateAbilityScript::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
	for (auto Object : InObjects)
	{
		UStateAbilityScriptArchetype* StateAbilityArchetype = Cast<UStateAbilityScriptArchetype>(Object);
		if (StateAbilityArchetype != nullptr)
		{
			FStateAbilityScriptEditorModule& EditorModule = FModuleManager::GetModuleChecked<FStateAbilityScriptEditorModule>("StateAbilityScriptEditor");
			TSharedRef<FStateAbilityEditor> NewEditor = EditorModule.CreateEditor(Mode, EditWithinLevelEditor, StateAbilityArchetype);

		}
	}
}
#undef LOCTEXT_NAMESPACE