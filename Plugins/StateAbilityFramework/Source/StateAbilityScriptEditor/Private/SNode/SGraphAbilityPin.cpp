#include "SNode/SGraphAbilityPin.h"

void SGraphAbilityPin::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	this->SetCursor(EMouseCursor::Default);

	bShowLabel = true;

	GraphPinObj = InPin;
	check(GraphPinObj != NULL);

	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
	check(Schema);

	SBorder::Construct(SBorder::FArguments()
		.BorderImage(this, &SGraphAbilityPin::GetPinBorder)
		.BorderBackgroundColor(this, &SGraphAbilityPin::GetPinColor)
		.OnMouseButtonDown(this, &SGraphAbilityPin::OnPinMouseDown)
		.Cursor(this, &SGraphAbilityPin::GetPinCursor)
		.Padding(FMargin(10.0f))
	);
}

TSharedRef<SWidget>	SGraphAbilityPin::GetDefaultValueWidget()
{
	return SNew(STextBlock);
}

const FSlateBrush* SGraphAbilityPin::GetPinBorder() const
{
	return FAppStyle::GetBrush(TEXT("Graph.StateNode.Body"));
}

FSlateColor SGraphAbilityPin::GetPinColor() const
{
	return FSlateColor(IsHovered() ? FLinearColor(1.0f, 0.7f, 0.0f) : FLinearColor(0.02f, 0.02f, 0.02f));
}


/////////////////////////////////////////////////////
// SGraphSASPinEntry

void SGraphSASPinEntry::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InPin);

	// Call utility function so inheritors can also call it since arguments can't be passed through
	CachePinIcons();
}

void SGraphSASPinEntry::CachePinIcons()
{
	CachedImg_Pin_ConnectedHovered = FAppStyle::GetBrush(TEXT("Graph.ExecPin.ConnectedHovered"));
	CachedImg_Pin_Connected = FAppStyle::GetBrush(TEXT("Graph.ExecPin.Connected"));
	CachedImg_Pin_DisconnectedHovered = FAppStyle::GetBrush(TEXT("Graph.ExecPin.DisconnectedHovered"));
	CachedImg_Pin_Disconnected = FAppStyle::GetBrush(TEXT("Graph.ExecPin.Disconnected"));
}

TSharedRef<SWidget>	SGraphSASPinEntry::GetDefaultValueWidget()
{
	return SNew(SSpacer);
}

const FSlateBrush* SGraphSASPinEntry::GetPinIcon() const
{
	const FSlateBrush* Brush = nullptr;

	if (IsConnected())
	{
		Brush = IsHovered() ? CachedImg_Pin_ConnectedHovered : CachedImg_Pin_Connected;
	}
	else
	{
		Brush = IsHovered() ? CachedImg_Pin_DisconnectedHovered : CachedImg_Pin_Disconnected;
	}

	return Brush;
}