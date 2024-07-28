// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "AssetTypeCategories.h"

class FStateAbilityScriptEditorModule : public IModuleInterface
{
public:
	/** Story app identifier string */
	static const FName StateAbilityEditorAppIdentifier;


	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/* Only virtual so that it can be called across the DLL boundary. */
	virtual TSharedRef<class FStateAbilityEditor> CreateEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UObject* Object);

	EAssetTypeCategories::Type GetAssetCategoryBit() const { return StateAbilityAssetCategoryBit; }

	TSharedPtr<struct FGraphNodeClassHelper> GetClassCache() { return ClassCache; }
protected:
	EAssetTypeCategories::Type StateAbilityAssetCategoryBit;

private:

	/** Asset type actions */
	TArray<TSharedPtr<class FAssetTypeActions_Base>> ItemDataAssetTypeActions;
	TSharedPtr<class FSAGraphNodeFactory> SAGraphNodeFactory;
	//TSharedPtr<struct FStoryPinConnectionFactory> StoryPinConnectionFactory;

	TSharedPtr<struct FGraphNodeClassHelper> ClassCache;
};
