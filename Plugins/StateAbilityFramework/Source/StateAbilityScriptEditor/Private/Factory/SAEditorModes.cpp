// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/SAEditorModes.h"

#include "Factory/SAEditorTabFactories.h"
#include "Editor/StateAbilityEditorToolbar.h"
#include "Editor/StateAbilityEditorToolkit.h"

#include "Framework/Docking/TabManager.h"
#include "Misc/AssertionMacros.h"
#include "Types/SlateEnums.h"

#define LOCTEXT_NAMESPACE "StateAbilityApplicationMode"

/************************************************************************/
/* Node Graph Mode														*/
/************************************************************************/
FGraphEditorApplicationMode::FGraphEditorApplicationMode(TSharedPtr<class FStateAbilityEditor> InEditor)
	: FApplicationMode(FStateAbilityEditor::NodeGraphModeName, FStateAbilityEditor::GetLocalizedMode)
{
	SEditor = InEditor;
	
	STabFactories.RegisterFactory(MakeShareable(new FNodeGraphEditorSummoner(InEditor)));
	STabFactories.RegisterFactory(MakeShareable(new FNodeGraphDetailsSummoner(InEditor)));
	STabFactories.RegisterFactory(MakeShareable(new FStateTreeAttributeDetailsSummoner(InEditor)));

	TabLayout = FTabManager::NewLayout("Standalone_StateAbilityNodeGraph_Layout_V0")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->Split
				(

					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.7f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(1.f)
						->AddTab(FStateAbilityEditor::NodeGraphEditorTab, ETabState::OpenedTab)
						->SetHideTabWell(true)
					)
				)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.3f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.6f)
						->AddTab(FStateAbilityEditor::ConfigVarsDetailsTab, ETabState::OpenedTab)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.4f)
						->AddTab(FStateAbilityEditor::AttributeDetailsTab, ETabState::OpenedTab)
					)
				)
			)

		);

	
	InEditor->GetToolbarBuilder()->AddModesToolbar(ToolbarExtender);

	InEditor->GetToolbarBuilder()->AddToolbar(ToolbarExtender);
}

void FGraphEditorApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	check(SEditor.IsValid());
	TSharedPtr<FStateAbilityEditor> SEditorPtr = SEditor.Pin();
	
	SEditorPtr->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	SEditorPtr->PushTabFactories(STabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FGraphEditorApplicationMode::PostActivateMode()
{
	// Reopen any documents that were open when the BT was last saved
	check(SEditor.IsValid());
	TSharedPtr<FStateAbilityEditor> SEditorPtr = SEditor.Pin();

	FApplicationMode::PostActivateMode();
}


/************************************************************************/
/* State Tree Mode														*/
/************************************************************************/
FStateTreeEditorApplicationMode::FStateTreeEditorApplicationMode(TSharedPtr<class FStateAbilityEditor> InEditor)
	: FApplicationMode(FStateAbilityEditor::StateTreeModeName, FStateAbilityEditor::GetLocalizedMode)
{
	SEditor = InEditor;

	STabFactories.RegisterFactory(MakeShareable(new FStateTreeEditorSummoner(InEditor)));
	STabFactories.RegisterFactory(MakeShareable(new FStateTreeDetailsSummoner(InEditor)));
	STabFactories.RegisterFactory(MakeShareable(new FStateTreeAttributeDetailsSummoner(InEditor)));

	TabLayout = FTabManager::NewLayout("Standalone_StateAbilityStateTree_Layout_V0")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.7f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(1.f)
						->AddTab(FStateAbilityEditor::StateTreeEditorTab, ETabState::OpenedTab)
						->SetHideTabWell(true)
					)
				)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.3f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.6f)
						->AddTab(FStateAbilityEditor::ConfigVarsDetailsTab, ETabState::OpenedTab)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.4f)
						->AddTab(FStateAbilityEditor::AttributeDetailsTab, ETabState::OpenedTab)
					)
				)
			)
		);

	InEditor->GetToolbarBuilder()->AddModesToolbar(ToolbarExtender);

	// ToolbarExtender只要绑定一次
	// InEditor->GetToolbarBuilder()->AddToolbar(ToolbarExtender);
}

void FStateTreeEditorApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	check(SEditor.IsValid());
	TSharedPtr<FStateAbilityEditor> SEditorPtr = SEditor.Pin();

	SEditorPtr->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	SEditorPtr->PushTabFactories(STabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FStateTreeEditorApplicationMode::PostActivateMode()
{
	// Reopen any documents that were open when the BT was last saved
	check(SEditor.IsValid());
	TSharedPtr<FStateAbilityEditor> SEditorPtr = SEditor.Pin();

	FApplicationMode::PostActivateMode();
}

#undef LOCTEXT_NAMESPACE
