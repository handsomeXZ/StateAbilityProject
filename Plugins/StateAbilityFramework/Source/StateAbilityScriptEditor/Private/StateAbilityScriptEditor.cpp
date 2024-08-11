// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateAbilityScriptEditor.h"

#include "Asset/AssetTypeActions_StateAbilityAsset.h"
#include "Factory/SAGraphFactory.h"
#include "Editor/StateAbilityEditorToolkit.h"
#include "Component/StateAbility/Script\StateAbilityScript.h"
#include "StructFilter.h"
#include "Editor/StateAbilityEditorStyle.h"
#include "Component/StateAbility/StateAbilityState.h"

#include "AIGraphTypes.h"

#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "FStateAbilityScriptEditorModule"

const FName FStateAbilityScriptEditorModule::StateAbilityEditorAppIdentifier(TEXT("StateAbilityScriptEditor"));

void FStateAbilityScriptEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FStateAbilityEditorStyle::Register();


	SAGraphNodeFactory = MakeShared<FSAGraphNodeFactory>();
	SAPinConnectionFactory = MakeShared<FSAPinConnectionFactory>();
	FEdGraphUtilities::RegisterVisualNodeFactory(SAGraphNodeFactory);
	FEdGraphUtilities::RegisterVisualPinConnectionFactory(SAPinConnectionFactory);

	//StoryPinConnectionFactory = MakeShareable(new FStoryPinConnectionFactory());
	//FEdGraphUtilities::RegisterVisualPinConnectionFactory(StoryPinConnectionFactory);

	// Register asset types
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	StateAbilityAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("StateAbilityAsset")), LOCTEXT("StateAbilityAssetCategory", "State Ability"));
	
	const TSharedPtr<FAssetTypeActions_StateAbilityScript> SAAssetTypeAction = MakeShareable(new FAssetTypeActions_StateAbilityScript);

	ItemDataAssetTypeActions.Add(SAAssetTypeAction);
	AssetTools.RegisterAssetTypeActions(SAAssetTypeAction.ToSharedRef());

	//FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	////注册绑定
	//PropertyEditorModule.RegisterCustomClassLayout(FName(UStateAbilityNodeBase::StaticClass()->GetName()), FOnGetDetailCustomizationInstance::CreateStatic(&FDetailsConfigVarsView::MakeInstance));
	////通知自定义模块修改完成
	//PropertyEditorModule.NotifyCustomizationModuleChanged();


}

void FStateAbilityScriptEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FStateAbilityEditorStyle::Unregister();

	ClassCache.Reset();

	if (SAGraphNodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(SAGraphNodeFactory);
		FEdGraphUtilities::UnregisterVisualPinConnectionFactory(SAPinConnectionFactory);
		SAGraphNodeFactory.Reset();
		SAPinConnectionFactory.Reset();
	}

	//if (StoryPinConnectionFactory.IsValid())
	//{
	//	FEdGraphUtilities::UnregisterVisualPinConnectionFactory(StoryPinConnectionFactory);
	//	StoryPinConnectionFactory.Reset();
	//}

	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (auto& AssetTypeAction : ItemDataAssetTypeActions)
		{
			if (AssetTypeAction.IsValid())
			{
				AssetToolsModule.UnregisterAssetTypeActions(AssetTypeAction.ToSharedRef());
			}
		}
	}
	ItemDataAssetTypeActions.Empty();

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	//模块卸载时，记得Unregister
	//PropertyEditorModule.UnregisterCustomClassLayout(FName(UStateAbilityState::StaticClass()->GetName()));
} 

TSharedRef<FStateAbilityEditor> FStateAbilityScriptEditorModule::CreateEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UObject* Object)
{
	if (!ClassCache.IsValid())
	{
		ClassCache = MakeShareable(new FGraphNodeClassHelper(UStateAbilityNodeBase::StaticClass()));
		ClassCache->UpdateAvailableBlueprintClasses();
	}

	TSharedRef<FStateAbilityEditor> NewSAEditor(new FStateAbilityEditor());
	NewSAEditor->InitializeAssetEditor(Mode, InitToolkitHost, Object);
	return NewSAEditor;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FStateAbilityScriptEditorModule, StateAbilityScriptEditor)