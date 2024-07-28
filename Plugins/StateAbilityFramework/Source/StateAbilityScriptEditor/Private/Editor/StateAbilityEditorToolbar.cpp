// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/StateAbilityEditorToolbar.h"

#include "Editor/StateAbilityEditorToolkit.h"
#include "Editor/StateAbilityEditorCommands.h"
#include "Editor/StateAbilityEditorToolbar.h"

#include "Delegates/Delegate.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/SlateDelegates.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/Margin.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "WorkflowOrientedApp/SModeWidget.h"

class SWidget;

#define LOCTEXT_NAMESPACE "StateAbilityEditorToolbar"

class SStateAbilityModeSeparator : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SStateAbilityModeSeparator) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArg)
	{
		SBorder::Construct(
			SBorder::FArguments()
			.BorderImage(FAppStyle::GetBrush("BlueprintEditor.PipelineSeparator"))
			.Padding(0.0f)
			);
	}

	// SWidget interface
	virtual FVector2D ComputeDesiredSize(float) const override
	{
		const float Height = 20.0f;
		const float Thickness = 16.0f;
		return FVector2D(Thickness, Height);
	}
	// End of SWidget interface
};

void FStateAbilityEditorToolbar::AddModesToolbar(TSharedPtr<FExtender> Extender)
{
	check(SAEditor.IsValid());
	TSharedPtr<FStateAbilityEditor> SAEditorPtr = SAEditor.Pin();

	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		SAEditorPtr->GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP( this, &FStateAbilityEditorToolbar::FillModesToolbar ) );
}

void FStateAbilityEditorToolbar::FillModesToolbar(FToolBarBuilder& ToolbarBuilder)
{
	check(SAEditor.IsValid());
	TSharedPtr<FStateAbilityEditor> SAEditorPtr = SAEditor.Pin();

	TAttribute<FName> GetActiveMode(SAEditorPtr.ToSharedRef(), &FStateAbilityEditor::GetCurrentMode);
	FOnModeChangeRequested SetActiveMode = FOnModeChangeRequested::CreateSP(SAEditorPtr.ToSharedRef(), &FStateAbilityEditor::SetCurrentMode);

	// Left side padding
	SAEditorPtr->AddToolbarWidget(SNew(SSpacer).Size(FVector2D(4.0f, 1.0f)));
	
	SAEditorPtr->AddToolbarWidget(
		SNew(SModeWidget, FStateAbilityEditor::GetLocalizedMode( FStateAbilityEditor::StateTreeModeName), FStateAbilityEditor::StateTreeModeName)
		.OnGetActiveMode(GetActiveMode)
		.OnSetActiveMode(SetActiveMode)
		.CanBeSelected(SAEditorPtr.Get(), &FStateAbilityEditor::CanAccessStateTreeMode)
		.ToolTipText(LOCTEXT("StateAbilityModeNameButtonTooltip", "Switch to State Tree Mode"))
		.IconImage(FAppStyle::GetBrush("BTEditor.SwitchToBehaviorTreeMode"))
	);

	SAEditorPtr->AddToolbarWidget(SNew(SSpacer).Size(FVector2D(10.0f, 1.0f)));

	SAEditorPtr->AddToolbarWidget(
		SNew(SModeWidget, FStateAbilityEditor::GetLocalizedMode(FStateAbilityEditor::NodeGraphModeName), FStateAbilityEditor::NodeGraphModeName)
		.OnGetActiveMode(GetActiveMode)
		.OnSetActiveMode(SetActiveMode)
		.CanBeSelected(SAEditorPtr.Get(), &FStateAbilityEditor::CanAccessNodeGraphMode)
		.ToolTipText(LOCTEXT("StateAbilityModeNameButtonTooltip", "Switch to Node Graph Mode"))
		.IconImage(FAppStyle::GetBrush("BTEditor.SwitchToBehaviorTreeMode"))
	);

	// Right side padding
	SAEditorPtr->AddToolbarWidget(SNew(SSpacer).Size(FVector2D(10.0f, 1.0f)));
}


void FStateAbilityEditorToolbar::AddToolbar(TSharedPtr<FExtender> Extender)
{
	check(SAEditor.IsValid());
	TSharedPtr<FStateAbilityEditor> SAEditorPtr = SAEditor.Pin();

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	ToolbarExtender->AddToolBarExtension("Asset", EExtensionHook::After, SAEditorPtr->GetToolkitCommands(), FToolBarExtensionDelegate::CreateSP(this, &FStateAbilityEditorToolbar::FillToolbar));
	SAEditorPtr->AddToolbarExtender(ToolbarExtender);
}

void FStateAbilityEditorToolbar::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	check(SAEditor.IsValid());
	TSharedPtr<FStateAbilityEditor> SAEditorPtr = SAEditor.Pin();
	if (SAEditorPtr->GetCurrentMode() == FStateAbilityEditor::NodeGraphModeName)
	{

		ToolbarBuilder.BeginSection("NodeGraphMode");
		{
			const FText NewCommonActionTooltip = LOCTEXT("Compile", "Compile");

			ToolbarBuilder.AddToolBarButton(
				FUIAction(
					FExecuteAction::CreateSP(SAEditorPtr.Get(), &FStateAbilityEditor::OnCompile),
					FCanExecuteAction(),
					FIsActionChecked(),
					FIsActionButtonVisible()
				),
				NAME_None,
				TAttribute<FText>(),
				NewCommonActionTooltip,
				TAttribute<FSlateIcon>(SAEditorPtr.ToSharedRef(), &FStateAbilityEditor::GetCompileStatusImage)
			);
		}
		ToolbarBuilder.EndSection();
	}
}

#undef LOCTEXT_NAMESPACE
