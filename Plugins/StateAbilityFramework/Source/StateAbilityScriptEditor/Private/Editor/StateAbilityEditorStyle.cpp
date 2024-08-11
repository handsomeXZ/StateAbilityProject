#include "Editor/StateAbilityEditorStyle.h"

#include "Brushes/SlateBoxBrush.h"
#include "Styling/SlateStyleRegistry.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "Misc/Paths.h"
#include "Styling/StyleColors.h"
#include "Styling/SlateStyleMacros.h"
#include "Interfaces/IPluginManager.h"

#define CORE_FONT( RelativePath, ... ) FSlateFontInfo( RootToCoreContentDir( RelativePath, TEXT( ".ttf" ) ), __VA_ARGS__ )

using namespace CoreStyleConstants;

FStateAbilityEditorStyle::FStateAbilityEditorStyle()
	: FSlateStyleSet(TEXT("StateAbilityEditorStyle"))
{
	const FVector2D IconSize(16.0f, 16.0f);
	const FVector2D ToolbarIconSize(20.0f, 20.0f);


	const FString EngineSlateContentDir = FPaths::EngineContentDir() / TEXT("Slate");
	const FString StateTreePluginContentDir = FPaths::ProjectPluginsDir() / TEXT("StateAbilityFramework/Resources");

	// These are the Slate colors which reference the dynamic colors in FSlateCoreStyle; these are the colors to put into the style
	static const FSlateColor Black = FLinearColor(0.000000, 0.000000, 0.000000);
	static const FSlateColor ForegroundInverted = FLinearColor(0.472885, 0.472885, 0.472885);
	static const FSlateColor SelectorColor = FLinearColor(0.701f, 0.225f, 0.003f);
	static const FSlateColor SelectionColor = FLinearColor(/*COLOR("18A0FBFF")*/FColor(16, 172, 132));
	static const FSlateColor SelectionColor_Inactive = FLinearColor(0.25f, 0.25f, 0.25f);
	static const FSlateColor SelectionColor_Pressed = FLinearColor(0.701f, 0.225f, 0.003f);
	static const FSlateColor HighlightColor = FLinearColor(0.068f, 0.068f, 0.068f);


	//////////////////////////////////////////////////////////////////////////
	// Engine
	SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	SetCoreContentRoot(EngineSlateContentDir);
	//////////////////////////////////////////////////////////////////////////

	const FScrollBarStyle ScrollBar = FAppStyle::GetWidgetStyle<FScrollBarStyle>("ScrollBar");
	const FTextBlockStyle& NormalText = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
	
	// Font
	const FTextBlockStyle StateTitle = FTextBlockStyle(NormalText)
		.SetFont(CORE_FONT("Fonts/Roboto-Bold", 12))
		.SetColorAndOpacity(FLinearColor(230.0f / 255.0f, 230.0f / 255.0f, 230.0f / 255.0f, 0.9f));
	const FEditableTextBoxStyle StateTitleEditableText = FEditableTextBoxStyle()
		.SetTextStyle(NormalText)
		.SetFont(CORE_FONT("Fonts/Roboto-Bold", 12))
		.SetBackgroundImageNormal(CORE_BOX_BRUSH("Common/TextBox", FMargin(4.0f / 16.0f)))
		.SetBackgroundImageHovered(CORE_BOX_BRUSH("Common/TextBox_Hovered", FMargin(4.0f / 16.0f)))
		.SetBackgroundImageFocused(CORE_BOX_BRUSH("Common/TextBox_Hovered", FMargin(4.0f / 16.0f)))
		.SetBackgroundImageReadOnly(CORE_BOX_BRUSH("Common/TextBox_ReadOnly", FMargin(4.0f / 16.0f)))
		.SetBackgroundColor(FLinearColor(0, 0, 0, 0.1f))
		.SetPadding(FMargin(0))
		.SetScrollBarStyle(ScrollBar);

	const FTextBlockStyle NamespaceForegroundStyle = FTextBlockStyle(NormalText)
		.SetFont(CORE_FONT("Fonts/Roboto-Regular", 9))
		.SetColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.1f, 0.9f));

	Set("StateStateAbilityEditorTree.State.Title", StateTitle);
	Set("StateAbilityEditor.State.TitleEditableText", StateTitleEditableText);
	Set("StateAbilityEditor.State.TitleInlineEditableText", FInlineEditableTextBlockStyle()
		.SetTextStyle(StateTitle)
		.SetEditableTextBoxStyle(StateTitleEditableText));

	Set("Attribute.NamespaceForegroundStyle", NamespaceForegroundStyle);


	static const FTableRowStyle PureTableRowStyle = FTableRowStyle()
		.SetEvenRowBackgroundBrush(FSlateNoResource())
		.SetEvenRowBackgroundHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 0.1f)))
		.SetOddRowBackgroundBrush(FSlateNoResource())
		.SetOddRowBackgroundHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 0.1f)))
		.SetSelectorFocusedBrush(BORDER_BRUSH("Common/Selector", FMargin(4.f / 16.f), /*SelectorColor*/SelectionColor))
		.SetActiveBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor))
		.SetActiveHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor))
		.SetInactiveBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor_Inactive))
		.SetInactiveHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor_Inactive))
		.SetActiveHighlightedBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, HighlightColor))
		.SetInactiveHighlightedBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, FSlateColor(FLinearColor(.1f, .1f, .1f))))

		.SetTextColor(Black)
		.SetSelectedTextColor(ForegroundInverted)
		.SetDropIndicator_Above(BOX_BRUSH("Common/DropZoneIndicator_Above", FMargin(10.0f / 16.0f, 10.0f / 16.0f, 0, 0), SelectionColor))
		.SetDropIndicator_Onto(BOX_BRUSH("Common/DropZoneIndicator_Onto", FMargin(4.0f / 16.0f), SelectionColor))
		.SetDropIndicator_Below(BOX_BRUSH("Common/DropZoneIndicator_Below", FMargin(10.0f / 16.0f, 0, 0, 10.0f / 16.0f), SelectionColor));
	Set("PureListView.Row", PureTableRowStyle);






	//////////////////////////////////////////////////////////////////////////
	// Plugin
	SetContentRoot(StateTreePluginContentDir);
	//////////////////////////////////////////////////////////////////////////
	
	// Slot
	Set("StateAbilityEditor.SlotLinked", new IMAGE_BRUSH_SVG("Slate/Slot_Linked", Icon16x16));
	Set("StateAbilityEditor.SlotRoot", new IMAGE_BRUSH_SVG("Slate/Slot_Root", Icon16x16));
	Set("StateAbilityEditor.SlotNormal", new IMAGE_BRUSH_SVG("Slate/Slot_Normal", Icon16x16));

	Set("Pin.Point", new IMAGE_BRUSH_SVG("Slate/Pin_Point", Icon16x16));

	// Node
	Set("StateAbilityEditor.SharedNode", new IMAGE_BRUSH_SVG("Slate/Node_Shared", Icon16x16));
	Set("StateAbilityEditor.PersistentState", new IMAGE_BRUSH_SVG("Slate/State_Persistent", Icon16x16));

	// 通用Error
	Set("StateAbilityEditor.Error", new IMAGE_BRUSH_SVG("Slate/Error", Icon16x16));

	// 其他
	Set("Attribute.NamespaceBorder", new BOX_BRUSH("Slate/NamespaceBorder", FMargin(4.0f / 16.0f)));
	Set("RoundedBox", new IMAGE_BRUSH_SVG("Slate/RoundedBox", Icon16x16));

	Set("Debug.Start", new IMAGE_BRUSH_SVG("Slate/Debug_Start", Icon16x16));
	Set("Debug.Stop", new IMAGE_BRUSH_SVG("Slate/Debug_Stop", Icon16x16));
	Set("Debug.Finish", new IMAGE_BRUSH_SVG("Slate/Debug_Finish", Icon16x16));
	Set("Debug.Asset", new IMAGE_BRUSH_SVG("Slate/DebugAsset", Icon16x16));
	Set("Debug.Realtime", new IMAGE_BRUSH_SVG("Slate/Debug_Realtime", Icon16x16));
	Set("Debug.History", new IMAGE_BRUSH_SVG("Slate/Debug_History", Icon16x16));
	Set("Debug.Import", new IMAGE_BRUSH_SVG("Slate/Debug_Import", Icon16x16));
	Set("Debug", new IMAGE_BRUSH_SVG("Slate/Debug", Icon16x16));

	
}

void FStateAbilityEditorStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FStateAbilityEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}

FStateAbilityEditorStyle& FStateAbilityEditorStyle::Get()
{
	static FStateAbilityEditorStyle Instance;
	return Instance;
}