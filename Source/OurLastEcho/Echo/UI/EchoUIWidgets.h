// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "CommonUserWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Fonts/SlateFontInfo.h"
#include "EchoUIWidgets.generated.h"

class UBorder;
class UButton;
class UOverlay;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UWidgetTree;
class UEchoUIRoot;

/**
 *  Plain greybox look shared by every menu. The menus build their own layouts in C++ (see UEchoScreen), so this
 *  is the one place to change fonts and colours.
 */
namespace EchoUI
{
	extern const FLinearColor TextColor;
	extern const FLinearColor DimTextColor;
	extern const FLinearColor AccentColor;
	extern const FLinearColor PanelColor;
	extern const FLinearColor ButtonColor;
	extern const FLinearColor DisabledColor;

	FSlateFontInfo Font(int32 Size, bool bBold = false);

	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Text, int32 Size, const FLinearColor& Color = TextColor, bool bBold = false);
}

/** Draws nothing itself: UEchoButton paints its own background, so CommonUI's default grey button stays out of the way */
UCLASS()
class UEchoButtonStyle : public UCommonButtonStyle
{
	GENERATED_BODY()

public:

	UEchoButtonStyle();
};

/**
 *  Menu button (a CommonUI button, so it works with mouse, keyboard and gamepad, and later consoles).
 *  Focus (keyboard/gamepad) and mouse hover both show the amber highlight; hovering also takes focus, so the
 *  keyboard carries on from wherever the mouse was. Gamepad A / Enter / Space / click activate it.
 */
UCLASS()
class UEchoButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:

	UEchoButton(const FObjectInitializer& ObjectInitializer);

	virtual bool Initialize() override;

	void SetLabel(const FText& InLabel);
	FText GetLabel() const;

	void SetMinWidth(float Width);

	/** Called on click (in addition to the CommonUI OnClicked event) */
	void SetOnClickedCallback(TFunction<void()> InCallback) { ClickCallback = MoveTemp(InCallback); }

	/** Tab buttons stay lit while selected */
	void SetLit(bool bInLit) { bLit = bInLit; UpdateLook(); }

	/** Clicks it through the normal click path (for tests) */
	void SimulateClick() { HandleButtonClicked(); }

protected:

	virtual void NativeOnClicked() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void HandleFocusReceived() override;
	virtual void HandleFocusLost() override;
	virtual void NativeOnEnabled() override;
	virtual void NativeOnDisabled() override;

	void UpdateLook();

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> SizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Background;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;

	TFunction<void()> ClickCallback;
	bool bHoveredNow = false;
	bool bFocusedNow = false;
	bool bLit = false;
	bool bBuilt = false;
};

/**
 *  One settings line: a label and a value you change with left/right (keyboard arrows, d-pad, stick) or with
 *  the mouse (the < > arrows, or dragging the slider). Kinds: a list of options, an on/off toggle, a slider,
 *  and a key binding (Enter / A / click to listen for the next key; Escape cancels).
 */
UCLASS()
class UEchoSettingRow : public UUserWidget
{
	GENERATED_BODY()

public:

	void InitOptions(const FText& InLabel, const TArray<FText>& InOptions, int32 InIndex, TFunction<void(int32)> InOnChanged);
	void InitToggle(const FText& InLabel, bool bInValue, TFunction<void(bool)> InOnChanged);
	void InitSlider(const FText& InLabel, float InMin, float InMax, float InStep, float InValue, TFunction<FText(float)> InFormat, TFunction<void(float)> InOnChanged);
	void InitKey(const FText& InLabel, const FKey& InKey, TFunction<void(const FKey&)> InOnKeyChosen);

	/** Show a new value without calling the callback (e.g. after Reset to Defaults) */
	void SetIndex(int32 InIndex);
	void SetValue(float InValue);
	void SetKey(const FKey& InKey);

	int32 GetIndex() const { return Index; }
	FString GetValueText() const;
	float GetValue() const { return Value; }
	bool IsListeningForKey() const { return bListening; }

	/** One step left or right, as the arrow keys do (for tests) */
	void Step(int32 Direction);

protected:

	virtual void NativeOnInitialized() override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

	enum class EKind : uint8 { Options, Slider, Key };

	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	void Refresh();
	void UpdateLook();
	void Activate();
	void ChooseKey(const FKey& Key);
	void SetValueFromMouse(const FVector2D& ScreenPosition);

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Background;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ValueText;

	/** Clickable < and > (the row itself keeps keyboard/gamepad focus) */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LeftArrow;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RightArrow;

	/** Slider fill; click or drag on it with the mouse */
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;

	bool bDragging = false;

	EKind Kind = EKind::Options;
	TArray<FText> Options;
	int32 Index = 0;
	float Value = 0.0f;
	float Min = 0.0f;
	float Max = 1.0f;
	float StepSize = 0.1f;
	FKey Key;
	bool bListening = false;
	bool bFocused = false;

	TFunction<void(int32)> OnIndexChanged;
	TFunction<void(float)> OnValueChanged;
	TFunction<FText(float)> Format;
	TFunction<void(const FKey&)> OnKeyChosen;
};

/** The screen stack: CommonUI's activatable widget stack with a gentle fade between screens */
UCLASS()
class UEchoMenuStack : public UCommonActivatableWidgetStack
{
	GENERATED_BODY()

public:

	UEchoMenuStack();
};

/**
 *  Base for every menu screen: a CommonUI activatable widget (it takes focus when shown, gives the menu the
 *  mouse cursor, and Back (Escape / gamepad B) goes back). Derived screens fill BuildContent.
 */
UCLASS(Abstract)
class UEchoScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:

	UEchoScreen();

	/** The UI root that shows this screen (set when pushed) */
	UEchoUIRoot* GetRoot() const { return Root.Get(); }
	void SetRoot(UEchoUIRoot* InRoot) { Root = InRoot; }

	/** Finds a button on this screen by its label (for tests: they press real buttons) */
	UFUNCTION(BlueprintPure, Category="Echo|UI")
	UEchoButton* FindButton(const FString& LabelText) const;

	UFUNCTION(BlueprintCallable, Category="Echo|UI")
	bool ClickButton(const FString& LabelText);

	/** The widget that has focus when the screen opens */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:

	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual bool NativeOnHandleBackAction() override;

	double ActivatedAt = 0.0;

	/** Fill the screen's column */
	virtual void BuildContent(UVerticalBox* Content) {}

	/** Back pressed; default closes the screen */
	virtual void OnBack() { DeactivateWidget(); }

	/** Darken the scene behind the menu (pause menu, dialogs) */
	virtual float GetBackdropOpacity() const { return 0.0f; }

	/** Where the column sits: false = left side (title menus), true = centred (dialogs, pause) */
	virtual bool IsCentred() const { return false; }

	UTextBlock* AddTitle(UVerticalBox* Content, const FText& Text, int32 Size = 40);
	UTextBlock* AddText(UVerticalBox* Content, const FText& Text, int32 Size = 18, const FLinearColor& Color = EchoUI::TextColor);
	UEchoButton* AddButton(UVerticalBox* Content, const FText& Text, TFunction<void()> OnClicked);
	void AddSpacer(UVerticalBox* Content, float Height);

	UEchoButton* MakeButton(const FText& Text, TFunction<void()> OnClicked);

	/** Focus goes here when the screen opens (first button by default) */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> FirstFocus;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UEchoButton>> Buttons;

	TWeakObjectPtr<UEchoUIRoot> Root;
};

/** A message with up to three buttons, over whatever is behind it */
UCLASS()
class UEchoDialog : public UEchoScreen
{
	GENERATED_BODY()

public:

	struct FChoice
	{
		FText Label;
		TFunction<void()> Callback;
	};

	void Setup(const FText& InTitle, const FText& InMessage, TArray<FChoice> InChoices);

	/** Replaces the message (e.g. "Searching..." -> a result) */
	void SetMessage(const FText& InMessage);

	UFUNCTION(BlueprintPure, Category="Echo|UI")
	FString GetMessageText() const;

protected:

	virtual void BuildContent(UVerticalBox* Content) override;
	virtual void OnBack() override;
	virtual float GetBackdropOpacity() const override { return 0.55f; }
	virtual bool IsCentred() const override { return true; }

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ButtonBox;

	TArray<FChoice> Choices;
};

/**
 *  Each local player's menu layer: a screen stack and a toast line. Front-end and gameplay player controllers
 *  both make one. Screens are pushed on top and fade in; closing one reveals the one below.
 */
UCLASS()
class UEchoUIRoot : public UCommonUserWidget
{
	GENERATED_BODY()

public:

	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	template <typename ScreenT>
	ScreenT* Push(TFunctionRef<void(ScreenT&)> Init = [](ScreenT&) {})
	{
		UEchoUIRoot* Self = this;
		return Stack->AddWidget<ScreenT>(ScreenT::StaticClass(), [Self, &Init](ScreenT& Screen)
		{
			Screen.SetRoot(Self);
			Init(Screen);
		});
	}

	/** Pushes a dialog. The last choice is what Back does */
	UEchoDialog* ShowDialog(const FText& Title, const FText& Message, TArray<UEchoDialog::FChoice> Choices);

	/** A short message along the top of the screen */
	void ShowToast(const FText& Message, float Seconds = 4.0f);

	UFUNCTION(BlueprintPure, Category="Echo|UI")
	UEchoScreen* GetTopScreen() const;

	UFUNCTION(BlueprintPure, Category="Echo|UI")
	bool HasScreens() const;

	UFUNCTION(BlueprintPure, Category="Echo|UI")
	FString GetToastText() const;

	void CloseAll();

	UEchoMenuStack* GetStack() const { return Stack; }

	/** Screens were all closed (the gameplay controller returns to game input) */
	TMulticastDelegate<void()> OnEmptied;

protected:

	void HandleDisplayedWidgetChanged(UCommonActivatableWidget* Widget);

	UPROPERTY(Transient)
	TObjectPtr<UEchoMenuStack> Stack;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Toast;

	float ToastRemaining = 0.0f;
};
