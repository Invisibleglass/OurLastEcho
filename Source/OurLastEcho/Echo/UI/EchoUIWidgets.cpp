// Our Last Echo

#include "EchoUIWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/CommonUIInputTypes.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "EchoUI"

// ------------------------------------------------------------------ style

namespace EchoUI
{
	const FLinearColor TextColor(0.95f, 0.92f, 0.86f, 1.0f);
	const FLinearColor DimTextColor(0.65f, 0.62f, 0.58f, 1.0f);
	const FLinearColor AccentColor(1.0f, 0.62f, 0.24f, 1.0f);
	const FLinearColor PanelColor(0.02f, 0.015f, 0.01f, 0.72f);
	const FLinearColor ButtonColor(0.08f, 0.07f, 0.06f, 0.82f);
	const FLinearColor DisabledColor(0.3f, 0.3f, 0.3f, 0.6f);

	FSlateFontInfo Font(int32 Size, bool bBold)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Text, int32 Size, const FLinearColor& Color, bool bBold)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>();
		Block->SetText(Text);
		Block->SetFont(Font(Size, bBold));
		Block->SetColorAndOpacity(FSlateColor(Color));
		return Block;
	}
}

// ------------------------------------------------------------------ button

UEchoButtonStyle::UEchoButtonStyle()
{
	FSlateBrush Empty;
	Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
	NormalBase = NormalHovered = NormalPressed = Empty;
	SelectedBase = SelectedHovered = SelectedPressed = Empty;
	Disabled = Empty;
	ButtonPadding = FMargin(0.0f);
	CustomPadding = FMargin(0.0f);
}

UEchoButton::UEchoButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Style = UEchoButtonStyle::StaticClass();
}

bool UEchoButton::Initialize()
{
	// The content has to exist before UCommonButtonBase::Initialize wraps it in the real button
	if (!bBuilt && !HasAnyFlags(RF_ClassDefaultObject))
	{
		bBuilt = true;
		if (!WidgetTree)
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}
		SizeBox = WidgetTree->ConstructWidget<USizeBox>();
		SizeBox->SetMinDesiredWidth(320.0f);
		Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetPadding(FMargin(22.0f, 10.0f));
		Label = EchoUI::MakeText(WidgetTree, FText::GetEmpty(), 20);
		Background->SetContent(Label);
		SizeBox->SetContent(Background);
		WidgetTree->RootWidget = SizeBox;
	}

	const bool bResult = Super::Initialize();
	UpdateLook();
	return bResult;
}

void UEchoButton::SetLabel(const FText& InLabel)
{
	if (Label)
	{
		Label->SetText(InLabel);
	}
}

FText UEchoButton::GetLabel() const
{
	return Label ? Label->GetText() : FText::GetEmpty();
}

void UEchoButton::SetMinWidth(float Width)
{
	if (SizeBox)
	{
		SizeBox->SetMinDesiredWidth(Width);
	}
}

void UEchoButton::NativeOnClicked()
{
	Super::NativeOnClicked();
	if (ClickCallback)
	{
		// Copy: the callback may destroy this button's screen
		TFunction<void()> Callback = ClickCallback;
		Callback();
	}
}

void UEchoButton::NativeOnHovered()
{
	Super::NativeOnHovered();
	bHoveredNow = true;
	// The mouse and the keyboard/gamepad share one highlight: hovering moves focus here
	if (GetIsEnabled())
	{
		SetFocus();
	}
	UpdateLook();
}

void UEchoButton::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	bHoveredNow = false;
	UpdateLook();
}

void UEchoButton::HandleFocusReceived()
{
	Super::HandleFocusReceived();
	bFocusedNow = true;
	UpdateLook();
}

void UEchoButton::HandleFocusLost()
{
	Super::HandleFocusLost();
	bFocusedNow = false;
	UpdateLook();
}

void UEchoButton::NativeOnEnabled()
{
	Super::NativeOnEnabled();
	UpdateLook();
}

void UEchoButton::NativeOnDisabled()
{
	Super::NativeOnDisabled();
	UpdateLook();
}

void UEchoButton::UpdateLook()
{
	if (!Background || !Label)
	{
		return;
	}

	const bool bEnabled = GetIsEnabled();
	const bool bHighlight = bEnabled && (bFocusedNow || bHoveredNow);
	Background->SetBrushColor(!bEnabled ? EchoUI::DisabledColor : bHighlight ? EchoUI::AccentColor : bLit ? FLinearColor(0.35f, 0.22f, 0.1f, 0.9f) : EchoUI::ButtonColor);
	Label->SetColorAndOpacity(FSlateColor(bHighlight ? FLinearColor(0.05f, 0.03f, 0.02f, 1.0f) : bEnabled ? EchoUI::TextColor : EchoUI::DimTextColor));
}

// ------------------------------------------------------------------ settings row

void UEchoSettingRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);

	Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetPadding(FMargin(16.0f, 8.0f));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Background->SetContent(Row);

	LabelText = EchoUI::MakeText(WidgetTree, FText::GetEmpty(), 18);
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText);
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LabelSlot->SetVerticalAlignment(VAlign_Center);

	LeftArrow = EchoUI::MakeText(WidgetTree, FText::FromString(TEXT("  <  ")), 18, EchoUI::AccentColor, true);
	Row->AddChildToHorizontalBox(LeftArrow)->SetVerticalAlignment(VAlign_Center);

	// The value, and (for sliders) a bar behind it
	USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>();
	ValueBox->SetWidthOverride(260.0f);
	ValueBox->SetHeightOverride(30.0f);
	UOverlay* ValueOverlay = WidgetTree->ConstructWidget<UOverlay>();
	ValueBox->SetContent(ValueOverlay);
	Bar = WidgetTree->ConstructWidget<UProgressBar>();
	Bar->SetFillColorAndOpacity(FLinearColor(0.75f, 0.42f, 0.15f, 0.85f));
	Bar->SetVisibility(ESlateVisibility::Collapsed);
	UOverlaySlot* BarSlot = ValueOverlay->AddChildToOverlay(Bar);
	BarSlot->SetHorizontalAlignment(HAlign_Fill);
	BarSlot->SetVerticalAlignment(VAlign_Fill);
	ValueText = EchoUI::MakeText(WidgetTree, FText::GetEmpty(), 18);
	ValueText->SetJustification(ETextJustify::Center);
	UOverlaySlot* ValueSlot = ValueOverlay->AddChildToOverlay(ValueText);
	ValueSlot->SetHorizontalAlignment(HAlign_Center);
	ValueSlot->SetVerticalAlignment(VAlign_Center);
	Row->AddChildToHorizontalBox(ValueBox)->SetVerticalAlignment(VAlign_Center);

	RightArrow = EchoUI::MakeText(WidgetTree, FText::FromString(TEXT("  >  ")), 18, EchoUI::AccentColor, true);
	Row->AddChildToHorizontalBox(RightArrow)->SetVerticalAlignment(VAlign_Center);

	WidgetTree->RootWidget = Background;
	UpdateLook();
}

void UEchoSettingRow::InitOptions(const FText& InLabel, const TArray<FText>& InOptions, int32 InIndex, TFunction<void(int32)> InOnChanged)
{
	Kind = EKind::Options;
	LabelText->SetText(InLabel);
	Options = InOptions;
	Index = FMath::Clamp(InIndex, 0, FMath::Max(0, Options.Num() - 1));
	OnIndexChanged = MoveTemp(InOnChanged);
	Refresh();
}

void UEchoSettingRow::InitToggle(const FText& InLabel, bool bInValue, TFunction<void(bool)> InOnChanged)
{
	InitOptions(InLabel, { LOCTEXT("Off", "Off"), LOCTEXT("On", "On") }, bInValue ? 1 : 0, [Callback = MoveTemp(InOnChanged)](int32 NewIndex)
	{
		Callback(NewIndex == 1);
	});
}

void UEchoSettingRow::InitSlider(const FText& InLabel, float InMin, float InMax, float InStep, float InValue, TFunction<FText(float)> InFormat, TFunction<void(float)> InOnChanged)
{
	Kind = EKind::Slider;
	LabelText->SetText(InLabel);
	Min = InMin;
	Max = InMax;
	StepSize = InStep;
	Value = FMath::Clamp(InValue, Min, Max);
	Format = MoveTemp(InFormat);
	OnValueChanged = MoveTemp(InOnChanged);
	Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
	Refresh();
}

void UEchoSettingRow::InitKey(const FText& InLabel, const FKey& InKey, TFunction<void(const FKey&)> InOnKeyChosen)
{
	Kind = EKind::Key;
	LabelText->SetText(InLabel);
	Key = InKey;
	OnKeyChosen = MoveTemp(InOnKeyChosen);
	LeftArrow->SetVisibility(ESlateVisibility::Hidden);
	RightArrow->SetVisibility(ESlateVisibility::Hidden);
	Refresh();
}

void UEchoSettingRow::SetIndex(int32 InIndex)
{
	Index = FMath::Clamp(InIndex, 0, FMath::Max(0, Options.Num() - 1));
	Refresh();
}

void UEchoSettingRow::SetValue(float InValue)
{
	Value = FMath::Clamp(InValue, Min, Max);
	Refresh();
}

void UEchoSettingRow::SetKey(const FKey& InKey)
{
	Key = InKey;
	bListening = false;
	Refresh();
}

FString UEchoSettingRow::GetValueText() const
{
	return ValueText ? ValueText->GetText().ToString() : FString();
}

void UEchoSettingRow::Refresh()
{
	switch (Kind)
	{
	case EKind::Options:
		ValueText->SetText(Options.IsValidIndex(Index) ? Options[Index] : FText::GetEmpty());
		break;
	case EKind::Slider:
		ValueText->SetText(Format ? Format(Value) : FText::AsNumber(Value));
		Bar->SetPercent(Max > Min ? (Value - Min) / (Max - Min) : 0.0f);
		break;
	case EKind::Key:
		ValueText->SetText(bListening ? LOCTEXT("PressKey", "Press a key... (Esc to cancel)") : Key.GetDisplayName());
		break;
	}
	UpdateLook();
}

void UEchoSettingRow::UpdateLook()
{
	if (Background)
	{
		Background->SetBrushColor(bListening ? FLinearColor(0.45f, 0.25f, 0.08f, 0.95f) : bFocused ? FLinearColor(0.3f, 0.18f, 0.08f, 0.92f) : EchoUI::ButtonColor);
		LabelText->SetColorAndOpacity(FSlateColor(bFocused ? EchoUI::AccentColor : EchoUI::TextColor));
	}
}

void UEchoSettingRow::Step(int32 Direction)
{
	if (Kind == EKind::Options && Options.Num() > 0)
	{
		// Lists stop at the ends; on/off toggles either way
		const int32 NewIndex = Options.Num() == 2 ? 1 - Index : FMath::Clamp(Index + Direction, 0, Options.Num() - 1);
		if (NewIndex != Index)
		{
			Index = NewIndex;
			Refresh();
			if (OnIndexChanged)
			{
				OnIndexChanged(Index);
			}
		}
	}
	else if (Kind == EKind::Slider)
	{
		const float NewValue = FMath::Clamp(FMath::GridSnap(Value + Direction * StepSize, StepSize), Min, Max);
		if (!FMath::IsNearlyEqual(NewValue, Value))
		{
			Value = NewValue;
			Refresh();
			if (OnValueChanged)
			{
				OnValueChanged(Value);
			}
		}
	}
}

void UEchoSettingRow::Activate()
{
	if (Kind == EKind::Key)
	{
		bListening = true;
		Refresh();
	}
	else if (Kind == EKind::Options)
	{
		// Enter / A cycles lists and flips toggles
		if (Options.Num() == 2 || Index < Options.Num() - 1)
		{
			Step(1);
		}
		else if (Options.Num() > 0)
		{
			Index = 0;
			Refresh();
			if (OnIndexChanged)
			{
				OnIndexChanged(Index);
			}
		}
	}
}

void UEchoSettingRow::ChooseKey(const FKey& InKey)
{
	bListening = false;
	if (InKey != EKeys::Escape && InKey.IsValid())
	{
		Key = InKey;
		if (OnKeyChosen)
		{
			OnKeyChosen(InKey);
		}
	}
	Refresh();
}

FNavigationReply UEchoSettingRow::NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply)
{
	// Left/right (arrows, d-pad, stick) change the value instead of moving focus
	if (bListening)
	{
		return FNavigationReply::Stop();
	}
	const EUINavigation Direction = InNavigationEvent.GetNavigationType();
	if (Kind != EKind::Key && (Direction == EUINavigation::Left || Direction == EUINavigation::Right))
	{
		Step(Direction == EUINavigation::Left ? -1 : 1);
		return FNavigationReply::Stop();
	}
	return Super::NativeOnNavigation(MyGeometry, InNavigationEvent, InDefaultReply);
}

FReply UEchoSettingRow::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// While listening, the next key (keyboard or gamepad) is the new binding, before anything else sees it
	if (bListening)
	{
		ChooseKey(InKeyEvent.GetKey());
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UEchoSettingRow::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Pressed = InKeyEvent.GetKey();
	if (Pressed == EKeys::Enter || Pressed == EKeys::SpaceBar || Pressed == EKeys::Gamepad_FaceButton_Bottom)
	{
		Activate();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UEchoSettingRow::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bListening)
	{
		// Mouse buttons can be bindings too (e.g. aim on the right button)
		ChooseKey(InMouseEvent.GetEffectingButton());
		return FReply::Handled();
	}

	SetFocus();
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Handled();
	}

	const FVector2D Position = InMouseEvent.GetScreenSpacePosition();
	if (Kind == EKind::Key)
	{
		Activate();
	}
	else if (LeftArrow->GetCachedGeometry().IsUnderLocation(Position))
	{
		Step(-1);
	}
	else if (RightArrow->GetCachedGeometry().IsUnderLocation(Position))
	{
		Step(1);
	}
	else if (Kind == EKind::Slider && Bar->GetCachedGeometry().IsUnderLocation(Position))
	{
		bDragging = true;
		SetValueFromMouse(Position);
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	else if (Kind == EKind::Options)
	{
		Activate();
	}
	return FReply::Handled();
}

FReply UEchoSettingRow::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging)
	{
		SetValueFromMouse(InMouseEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UEchoSettingRow::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging)
	{
		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UEchoSettingRow::SetValueFromMouse(const FVector2D& ScreenPosition)
{
	const FGeometry& Geometry = Bar->GetCachedGeometry();
	const float Width = Geometry.GetLocalSize().X;
	if (Width <= 0.0f)
	{
		return;
	}
	const float Alpha = FMath::Clamp(Geometry.AbsoluteToLocal(ScreenPosition).X / Width, 0.0f, 1.0f);
	const float NewValue = FMath::Clamp(FMath::GridSnap(FMath::Lerp(Min, Max, Alpha), StepSize), Min, Max);
	if (!FMath::IsNearlyEqual(NewValue, Value))
	{
		Value = NewValue;
		Refresh();
		if (OnValueChanged)
		{
			OnValueChanged(Value);
		}
	}
}

void UEchoSettingRow::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (!bListening)
	{
		SetFocus();
	}
}

void UEchoSettingRow::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	bFocused = true;
	UpdateLook();
}

void UEchoSettingRow::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	bFocused = false;
	if (bListening)
	{
		bListening = false;
		Refresh();
	}
	UpdateLook();
}

// ------------------------------------------------------------------ stack

UEchoMenuStack::UEchoMenuStack()
{
	// A gentle cross-fade between screens
	TransitionType = ECommonSwitcherTransition::FadeOnly;
	TransitionDuration = 0.25f;
}

// ------------------------------------------------------------------ screen base

UEchoScreen::UEchoScreen()
{
	bIsBackHandler = true;
	bSupportsActivationFocus = true;
}

void UEchoScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>();

	if (GetBackdropOpacity() > 0.0f)
	{
		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
		Backdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, GetBackdropOpacity()));
		UOverlaySlot* BackdropSlot = RootOverlay->AddChildToOverlay(Backdrop);
		BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
		BackdropSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// One column on a dark panel: on the left for the title menus, centred for dialogs and the pause menu
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(EchoUI::PanelColor);
	Panel->SetPadding(FMargin(48.0f, 40.0f));
	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Content);
	UOverlaySlot* PanelSlot = RootOverlay->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(IsCentred() ? HAlign_Center : HAlign_Left);
	PanelSlot->SetVerticalAlignment(VAlign_Center);
	PanelSlot->SetPadding(IsCentred() ? FMargin(0.0f) : FMargin(110.0f, 0.0f, 0.0f, 0.0f));

	WidgetTree->RootWidget = RootOverlay;
	BuildContent(Content);
}

TOptional<FUIInputConfig> UEchoScreen::GetDesiredInputConfig() const
{
	// Menus own the input and show the cursor; nothing reaches the character underneath
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

UWidget* UEchoScreen::NativeGetDesiredFocusTarget() const
{
	return FirstFocus ? FirstFocus.Get() : Super::NativeGetDesiredFocusTarget();
}

bool UEchoScreen::NativeOnHandleBackAction()
{
	// One Back press closes one screen: ignore Back for a moment after this screen became the active one, or the
	// same press that closed the screen above would reach this one too (e.g. Credits -> main menu -> "Quit?")
	if (FPlatformTime::Seconds() - ActivatedAt > 0.3)
	{
		OnBack();
	}
	return true;
}

void UEchoScreen::NativeOnActivated()
{
	ActivatedAt = FPlatformTime::Seconds();
	Super::NativeOnActivated();
}

UTextBlock* UEchoScreen::AddTitle(UVerticalBox* Content, const FText& Text, int32 Size)
{
	UTextBlock* Title = EchoUI::MakeText(WidgetTree, Text, Size, EchoUI::TextColor, true);
	Content->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
	return Title;
}

UTextBlock* UEchoScreen::AddText(UVerticalBox* Content, const FText& Text, int32 Size, const FLinearColor& Color)
{
	UTextBlock* Block = EchoUI::MakeText(WidgetTree, Text, Size, Color);
	Block->SetAutoWrapText(true);
	Content->AddChildToVerticalBox(Block)->SetPadding(FMargin(0.0f, 4.0f));
	return Block;
}

UEchoButton* UEchoScreen::MakeButton(const FText& Text, TFunction<void()> OnClicked)
{
	UEchoButton* Button = CreateWidget<UEchoButton>(this, UEchoButton::StaticClass());
	Button->SetLabel(Text);
	Button->SetOnClickedCallback(MoveTemp(OnClicked));
	Buttons.Add(Button);
	if (!FirstFocus)
	{
		FirstFocus = Button;
	}
	return Button;
}

UEchoButton* UEchoScreen::AddButton(UVerticalBox* Content, const FText& Text, TFunction<void()> OnClicked)
{
	UEchoButton* Button = MakeButton(Text, MoveTemp(OnClicked));
	Content->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 5.0f));
	return Button;
}

void UEchoScreen::AddSpacer(UVerticalBox* Content, float Height)
{
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>();
	Spacer->SetSize(FVector2D(1.0f, Height));
	Content->AddChildToVerticalBox(Spacer);
}

UEchoButton* UEchoScreen::FindButton(const FString& LabelText) const
{
	for (UEchoButton* Button : Buttons)
	{
		if (Button && Button->GetLabel().ToString().Equals(LabelText, ESearchCase::IgnoreCase))
		{
			return Button;
		}
	}
	return nullptr;
}

bool UEchoScreen::ClickButton(const FString& LabelText)
{
	UEchoButton* Button = FindButton(LabelText);
	if (!Button || !Button->GetIsEnabled())
	{
		return false;
	}
	// The same path a real click takes
	Button->SimulateClick();
	return true;
}

// ------------------------------------------------------------------ dialog

void UEchoDialog::Setup(const FText& InTitle, const FText& InMessage, TArray<FChoice> InChoices)
{
	TitleText->SetText(InTitle);
	MessageText->SetText(InMessage);
	Choices = MoveTemp(InChoices);

	ButtonBox->ClearChildren();
	Buttons.Reset();
	FirstFocus = nullptr;
	for (int32 ChoiceIndex = 0; ChoiceIndex < Choices.Num(); ++ChoiceIndex)
	{
		UEchoButton* Button = MakeButton(Choices[ChoiceIndex].Label, [this, ChoiceIndex]()
		{
			// Close first, then act (the action may open another screen)
			TFunction<void()> Callback = Choices.IsValidIndex(ChoiceIndex) ? Choices[ChoiceIndex].Callback : nullptr;
			DeactivateWidget();
			if (Callback)
			{
				Callback();
			}
		});
		ButtonBox->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 5.0f));
	}
	if (FirstFocus && IsActivated())
	{
		FirstFocus->SetFocus();
	}
}

void UEchoDialog::SetMessage(const FText& InMessage)
{
	MessageText->SetText(InMessage);
}

FString UEchoDialog::GetMessageText() const
{
	return MessageText ? MessageText->GetText().ToString() : FString();
}

void UEchoDialog::BuildContent(UVerticalBox* Content)
{
	TitleText = AddTitle(Content, FText::GetEmpty(), 28);
	MessageText = AddText(Content, FText::GetEmpty(), 18);
	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
	Width->SetWidthOverride(520.0f);
	Content->AddChildToVerticalBox(Width);
	AddSpacer(Content, 16.0f);
	ButtonBox = WidgetTree->ConstructWidget<UVerticalBox>();
	Content->AddChildToVerticalBox(ButtonBox);
}

void UEchoDialog::OnBack()
{
	// Back = the last choice (Cancel / OK); a dialog with no choices can't be dismissed
	if (Choices.Num() > 0)
	{
		TFunction<void()> Callback = Choices.Last().Callback;
		DeactivateWidget();
		if (Callback)
		{
			Callback();
		}
	}
}

// ------------------------------------------------------------------ root

void UEchoUIRoot::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>();
	Stack = WidgetTree->ConstructWidget<UEchoMenuStack>();
	UOverlaySlot* StackSlot = RootOverlay->AddChildToOverlay(Stack);
	StackSlot->SetHorizontalAlignment(HAlign_Fill);
	StackSlot->SetVerticalAlignment(VAlign_Fill);

	UBorder* ToastBack = WidgetTree->ConstructWidget<UBorder>();
	ToastBack->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	ToastBack->SetPadding(FMargin(24.0f, 10.0f));
	Toast = EchoUI::MakeText(WidgetTree, FText::GetEmpty(), 20);
	ToastBack->SetContent(Toast);
	ToastBack->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* ToastSlot = RootOverlay->AddChildToOverlay(ToastBack);
	ToastSlot->SetHorizontalAlignment(HAlign_Center);
	ToastSlot->SetVerticalAlignment(VAlign_Top);
	ToastSlot->SetPadding(FMargin(0.0f, 40.0f, 0.0f, 0.0f));
	ToastBack->SetRenderOpacity(0.0f);

	WidgetTree->RootWidget = RootOverlay;
	Stack->OnDisplayedWidgetChanged().AddUObject(this, &UEchoUIRoot::HandleDisplayedWidgetChanged);
}

void UEchoUIRoot::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (ToastRemaining > 0.0f)
	{
		// Fade in quickly, hold, fade out over the last second
		ToastRemaining = FMath::Max(0.0f, ToastRemaining - InDeltaTime);
		if (UWidget* ToastBack = Toast->GetParent())
		{
			ToastBack->SetRenderOpacity(FMath::Clamp(ToastRemaining, 0.0f, 1.0f));
		}
	}
}

UEchoDialog* UEchoUIRoot::ShowDialog(const FText& Title, const FText& Message, TArray<UEchoDialog::FChoice> Choices)
{
	UEchoDialog* Dialog = Push<UEchoDialog>();
	if (Dialog)
	{
		Dialog->Setup(Title, Message, MoveTemp(Choices));
	}
	return Dialog;
}

void UEchoUIRoot::ShowToast(const FText& Message, float Seconds)
{
	Toast->SetText(Message);
	ToastRemaining = Seconds;
	if (UWidget* ToastBack = Toast->GetParent())
	{
		ToastBack->SetRenderOpacity(1.0f);
	}
}

FString UEchoUIRoot::GetToastText() const
{
	return ToastRemaining > 0.0f ? Toast->GetText().ToString() : FString();
}

UEchoScreen* UEchoUIRoot::GetTopScreen() const
{
	return Stack ? Cast<UEchoScreen>(Stack->GetActiveWidget()) : nullptr;
}

bool UEchoUIRoot::HasScreens() const
{
	return Stack && Stack->GetNumWidgets() > 0;
}

void UEchoUIRoot::CloseAll()
{
	if (Stack)
	{
		Stack->ClearWidgets();
	}
}

void UEchoUIRoot::HandleDisplayedWidgetChanged(UCommonActivatableWidget* Widget)
{
	if (!Widget)
	{
		OnEmptied.Broadcast();
	}
}

#undef LOCTEXT_NAMESPACE
