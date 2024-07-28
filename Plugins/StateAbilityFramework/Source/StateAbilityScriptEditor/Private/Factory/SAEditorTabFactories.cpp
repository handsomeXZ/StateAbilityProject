// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/SAEditorTabFactories.h"

#include "Component/StateAbility/Script/StateAbilityScript.h"
#include "Editor/StateAbilityEditorToolkit.h"

#include "Containers/Array.h"
#include "Engine/Blueprint.h"
#include "GraphEditor.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "Styling/AppStyle.h"
#include "Styling/ISlateStyle.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Docking/SDockTab.h"

class SWidget;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "StateAbilityEditorFactories"

/************************************************************************/
/* State Tree					                                        */
/************************************************************************/
// FStateTreeEditorSummoner
FStateTreeEditorSummoner::FStateTreeEditorSummoner(TSharedPtr<class FStateAbilityEditor> InEditorPtr)
	: FWorkflowTabFactory(FStateAbilityEditor::StateTreeEditorTab, InEditorPtr)
	, SEditorPtr(InEditorPtr)
{
	TabLabel = LOCTEXT("StateAbilityEditor", "Editor");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Editor");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("StateAbilityScriptEditorView", "Editor");
	ViewMenuTooltip = LOCTEXT("StateAbilityScriptEditorView_ToolTip", "Show the Editor view");
}
TSharedRef<SWidget> FStateTreeEditorSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	check(SEditorPtr.IsValid());
	return SEditorPtr.Pin()->SpawnStateTreeEdTab();
}
FText FStateTreeEditorSummoner::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
	return LOCTEXT("StateAbilityEditorTabTooltip", "The StateTree Editor tab allows editing of the properties of State Tree Nodes");
}
// FStateTreeDetailsSummoner
FStateTreeDetailsSummoner::FStateTreeDetailsSummoner(TSharedPtr<class FStateAbilityEditor> InEditorPtr)
	: FWorkflowTabFactory(FStateAbilityEditor::ConfigVarsDetailsTab, InEditorPtr)
	, SEditorPtr(InEditorPtr)
{
	TabLabel = LOCTEXT("StateAbilityEditor", "Details");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("StateAbilityScriptDetailsView", "Details");
	ViewMenuTooltip = LOCTEXT("StateAbilityScriptDetailsView_ToolTip", "Show the details view");
}
TSharedRef<SWidget> FStateTreeDetailsSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	check(SEditorPtr.IsValid());
	return SEditorPtr.Pin()->SpawnConfigVarsDetailsTab();
}
FText FStateTreeDetailsSummoner::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
	return LOCTEXT("StateAbilityScriptDetailsTabTooltip", "The StateTree Editor tab allows editing of the properties of State Tree Nodes");
}

FStateTreeAttributeDetailsSummoner::FStateTreeAttributeDetailsSummoner(TSharedPtr<class FStateAbilityEditor> InEditorPtr)
	: FWorkflowTabFactory(FStateAbilityEditor::AttributeDetailsTab, InEditorPtr)
	, SEditorPtr(InEditorPtr)
{
	TabLabel = LOCTEXT("StateAbilityEditor", "AttributeBag");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.AttributeDetails");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("StateAbilityScriptDetailsView", "AttributeDetails");
	ViewMenuTooltip = LOCTEXT("StateAbilityScriptDetailsView_ToolTip", "Show the Attribute details view");
}
TSharedRef<SWidget> FStateTreeAttributeDetailsSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	check(SEditorPtr.IsValid());
	return SEditorPtr.Pin()->SpawnAttributeDetailsTab();
}
FText FStateTreeAttributeDetailsSummoner::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
	return LOCTEXT("StateAbilityScriptDetailsTabTooltip", "The StateTree Editor tab allows editing of the properties of State Tree Nodes");
}

/************************************************************************/
/* Node Graph					                                        */
/************************************************************************/
// FNodeGraphEditorSummoner
FNodeGraphEditorSummoner::FNodeGraphEditorSummoner(TSharedPtr<class FStateAbilityEditor> InEditorPtr)
	: FWorkflowTabFactory(FStateAbilityEditor::NodeGraphEditorTab, InEditorPtr)
	, SEditorPtr(InEditorPtr)
{
	TabLabel = LOCTEXT("StateAbilityEditor", "Editor");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Editor");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("StateAbilityScriptEditorView", "Editor");
	ViewMenuTooltip = LOCTEXT("StateAbilityScriptEditorView_ToolTip", "Show the Editor view");
}
TSharedRef<SWidget> FNodeGraphEditorSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	check(SEditorPtr.IsValid());
	return SEditorPtr.Pin()->SpawnNodeGraphEdTab();
}
FText FNodeGraphEditorSummoner::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
	return LOCTEXT("StateAbilityEditorTabTooltip", "The NodeGraph Editor tab allows editing of the properties of Node Graph Nodes");
}
// FNodeGraphDetailsSummoner
FNodeGraphDetailsSummoner::FNodeGraphDetailsSummoner(TSharedPtr<class FStateAbilityEditor> InEditorPtr)
	: FWorkflowTabFactory(FStateAbilityEditor::ConfigVarsDetailsTab, InEditorPtr)
	, SEditorPtr(InEditorPtr)
{
	TabLabel = LOCTEXT("StateAbilityEditor", "Details");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("StateAbilityScriptDetailsView", "Details");
	ViewMenuTooltip = LOCTEXT("StateAbilityScriptDetailsView_ToolTip", "Show the details view");
}
TSharedRef<SWidget> FNodeGraphDetailsSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	check(SEditorPtr.IsValid());
	return SEditorPtr.Pin()->SpawnConfigVarsDetailsTab();
}
FText FNodeGraphDetailsSummoner::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
	return LOCTEXT("StateAbilityScriptDetailsTabTooltip", "The NodeGraph details tab allows editing of the properties of Node Graph Nodes");
}

#undef LOCTEXT_NAMESPACE
