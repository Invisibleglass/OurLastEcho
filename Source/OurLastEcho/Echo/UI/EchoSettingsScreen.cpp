// Our Last Echo

#include "EchoSettingsScreen.h"
#include "Blueprint/WidgetTree.h"
#include "Containers/Ticker.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameplayTagContainer.h"
#include "InputMappingContext.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "EchoGameUserSettings.h"
#include "OurLastEcho.h"

#define LOCTEXT_NAMESPACE "EchoSettings"

namespace
{
	const TCHAR* MappableContextPaths[] =
	{
		TEXT("/Game/Input/IMC_Default.IMC_Default"),
		TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"),
		TEXT("/Game/Input/IMC_Bow.IMC_Bow"),
		TEXT("/Game/Input/IMC_Whip.IMC_Whip"),
		TEXT("/Game/Input/IMC_Menu.IMC_Menu"),
	};

	/** Key rows in this order (anything else registered goes after) */
	const TCHAR* KeyOrder[] = { TEXT("MoveForward"), TEXT("MoveBackward"), TEXT("MoveLeft"), TEXT("MoveRight"), TEXT("Jump"), TEXT("Aim"), TEXT("Fire"), TEXT("Whip"), TEXT("Menu") };

	FText Percent(float Value)
	{
		return FText::Format(LOCTEXT("PercentFmt", "{0}%"), FText::AsNumber(FMath::RoundToInt(Value * 100.0f)));
	}

	FText Multiplier(float Value)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 2;
		Options.MaximumFractionalDigits = 2;
		return FText::Format(LOCTEXT("MultFmt", "x{0}"), FText::AsNumber(Value, &Options));
	}

	const float SubtitleScales[] = { 0.75f, 1.0f, 1.35f, 1.75f };
	const float FrameRates[] = { 30.0f, 60.0f, 120.0f, 144.0f, 0.0f };

	/** The keyboard/mouse mapping of a mappable name (the one whose default key isn't a gamepad key) */
	const FPlayerKeyMapping* FindKeyboardMapping(const UEnhancedInputUserSettings* InputSettings, FName MappingName)
	{
		const UEnhancedPlayerMappableKeyProfile* Profile = InputSettings ? InputSettings->GetActiveKeyProfile() : nullptr;
		const FKeyMappingRow* Row = Profile ? Profile->FindKeyMappingRow(MappingName) : nullptr;
		if (!Row)
		{
			return nullptr;
		}
		for (const FPlayerKeyMapping& Mapping : Row->Mappings)
		{
			if (!Mapping.GetDefaultKey().IsGamepadKey())
			{
				return &Mapping;
			}
		}
		return nullptr;
	}
}

bool UEchoSettingsScreen::FValues::Equals(const FValues& Other) const
{
	return WindowMode == Other.WindowMode && Resolution == Other.Resolution && Quality == Other.Quality && bVSync == Other.bVSync
		&& FMath::IsNearlyEqual(FrameRateLimit, Other.FrameRateLimit) && FMath::IsNearlyEqual(Brightness, Other.Brightness)
		&& FMath::IsNearlyEqual(Master, Other.Master) && FMath::IsNearlyEqual(Music, Other.Music) && FMath::IsNearlyEqual(Effects, Other.Effects)
		&& FMath::IsNearlyEqual(Dialogue, Other.Dialogue) && FMath::IsNearlyEqual(Voice, Other.Voice) && bMicrophone == Other.bMicrophone
		&& FMath::IsNearlyEqual(Mouse, Other.Mouse) && FMath::IsNearlyEqual(Aim, Other.Aim) && FMath::IsNearlyEqual(Gamepad, Other.Gamepad)
		&& bInvertY == Other.bInvertY && AimMode == Other.AimMode && bSubtitles == Other.bSubtitles
		&& FMath::IsNearlyEqual(SubtitleScale, Other.SubtitleScale) && bReduceShake == Other.bReduceShake;
}

UEchoSettingsScreen::FValues UEchoSettingsScreen::Capture(const UEchoGameUserSettings* Settings)
{
	FValues Values;
	Values.WindowMode = Settings->GetFullscreenMode();
	Values.Resolution = Settings->GetScreenResolution();
	Values.Quality = Settings->GetOverallScalabilityLevel();
	Values.bVSync = Settings->IsVSyncEnabled();
	Values.FrameRateLimit = Settings->GetFrameRateLimit();
	Values.Brightness = Settings->Brightness;
	Values.Master = Settings->MasterVolume;
	Values.Music = Settings->MusicVolume;
	Values.Effects = Settings->SoundEffectsVolume;
	Values.Dialogue = Settings->DialogueVolume;
	Values.Voice = Settings->VoiceChatVolume;
	Values.bMicrophone = Settings->bMicrophoneEnabled;
	Values.Mouse = Settings->MouseSensitivity;
	Values.Aim = Settings->AimSensitivity;
	Values.Gamepad = Settings->GamepadSensitivity;
	Values.bInvertY = Settings->bInvertY;
	Values.AimMode = static_cast<uint8>(Settings->AimMode);
	Values.bSubtitles = Settings->bSubtitlesEnabled;
	Values.SubtitleScale = Settings->SubtitleScale;
	Values.bReduceShake = Settings->bReduceCameraShake;
	return Values;
}

void UEchoSettingsScreen::Restore(UEchoGameUserSettings* Settings, const FValues& Values)
{
	Settings->SetFullscreenMode(static_cast<EWindowMode::Type>(Values.WindowMode));
	Settings->SetScreenResolution(Values.Resolution);
	if (Values.Quality >= 0)
	{
		Settings->SetOverallScalabilityLevel(Values.Quality);
	}
	Settings->SetVSyncEnabled(Values.bVSync);
	Settings->SetFrameRateLimit(Values.FrameRateLimit);
	Settings->Brightness = Values.Brightness;
	Settings->MasterVolume = Values.Master;
	Settings->MusicVolume = Values.Music;
	Settings->SoundEffectsVolume = Values.Effects;
	Settings->DialogueVolume = Values.Dialogue;
	Settings->VoiceChatVolume = Values.Voice;
	Settings->bMicrophoneEnabled = Values.bMicrophone;
	Settings->MouseSensitivity = Values.Mouse;
	Settings->AimSensitivity = Values.Aim;
	Settings->GamepadSensitivity = Values.Gamepad;
	Settings->bInvertY = Values.bInvertY;
	Settings->AimMode = static_cast<EEchoAimMode>(Values.AimMode);
	Settings->bSubtitlesEnabled = Values.bSubtitles;
	Settings->SubtitleScale = Values.SubtitleScale;
	Settings->bReduceCameraShake = Values.bReduceShake;
}

TArray<UInputMappingContext*> UEchoSettingsScreen::LoadMappableContexts()
{
	TArray<UInputMappingContext*> Contexts;
	for (const TCHAR* Path : MappableContextPaths)
	{
		if (UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, Path))
		{
			Contexts.Add(Context);
		}
	}
	return Contexts;
}

UEchoGameUserSettings* UEchoSettingsScreen::GetSettings() const
{
	return UEchoGameUserSettings::GetEchoSettings();
}

UEnhancedInputUserSettings* UEchoSettingsScreen::GetInputSettings() const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	return Subsystem ? Subsystem->GetUserSettings() : nullptr;
}

void UEchoSettingsScreen::BuildContent(UVerticalBox* Content)
{
	AddTitle(Content, LOCTEXT("Title", "Settings"), 36);

	// Tabs
	UHorizontalBox* TabRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	const FText TabNames[] = { LOCTEXT("Graphics", "Graphics"), LOCTEXT("Audio", "Audio"), LOCTEXT("Controls", "Controls"), LOCTEXT("Accessibility", "Accessibility") };
	for (int32 TabIndex = 0; TabIndex < UE_ARRAY_COUNT(TabNames); ++TabIndex)
	{
		UEchoButton* TabButton = MakeButton(TabNames[TabIndex], [this, TabIndex]() { SelectTab(TabIndex); });
		TabButton->SetMinWidth(180.0f);
		TabRow->AddChildToHorizontalBox(TabButton)->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		TabButtons.Add(TabButton);
	}
	Content->AddChildToVerticalBox(TabRow);
	AddText(Content, LOCTEXT("TabHint", "LB / RB (Q / E) switch tabs.  Left / right change a setting."), 14, EchoUI::DimTextColor);
	AddSpacer(Content, 8.0f);

	// Rows
	USizeBox* ListBox = WidgetTree->ConstructWidget<USizeBox>();
	ListBox->SetHeightOverride(470.0f);
	ListBox->SetWidthOverride(760.0f);
	RowList = WidgetTree->ConstructWidget<UScrollBox>();
	// Keyboard/gamepad focus moving down the list scrolls it along
	RowList->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll);
	ListBox->SetContent(RowList);
	Content->AddChildToVerticalBox(ListBox);
	AddSpacer(Content, 12.0f);

	StatusText = AddText(Content, FText::GetEmpty(), 15, EchoUI::DimTextColor);

	// Footer
	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>();
	const TPair<FText, TFunction<void()>> FooterButtons[] =
	{
		{ LOCTEXT("Apply", "Apply"), [this]() { ApplyChanges(); } },
		{ LOCTEXT("Reset", "Reset to Defaults"), [this]()
			{
				GetRoot()->ShowDialog(LOCTEXT("ResetTitle", "Reset to Defaults"), LOCTEXT("ResetMessage", "Reset every setting and key binding to its default? (Apply to keep them.)"),
					{ { LOCTEXT("ResetYes", "Reset"), [this]() { ResetToDefaults(); } }, { LOCTEXT("ResetNo", "Cancel"), nullptr } });
			} },
		{ LOCTEXT("Back", "Back"), [this]() { OnBack(); } },
	};
	for (const TPair<FText, TFunction<void()>>& Entry : FooterButtons)
	{
		UEchoButton* Button = MakeButton(Entry.Key, Entry.Value);
		Button->SetMinWidth(200.0f);
		Footer->AddChildToHorizontalBox(Button)->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	}
	Content->AddChildToVerticalBox(Footer);
}

void UEchoSettingsScreen::NativeOnActivated()
{
	if (bCloseWhenActivated)
	{
		// Back on top after the unsaved-changes dialog: close on the next tick, outside the stack's own update
		bCloseWhenActivated = false;
		Super::NativeOnActivated();
		TWeakObjectPtr<UEchoSettingsScreen> WeakThis(this);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
		{
			if (WeakThis.IsValid() && WeakThis->IsActivated())
			{
				WeakThis->DeactivateWidget();
			}
			return false;
		}));
		return;
	}

	// The Controls tab lists every mappable key, even on the title screen where no character has added them yet
	if (UEnhancedInputUserSettings* InputSettings = GetInputSettings())
	{
		for (UInputMappingContext* Context : LoadMappableContexts())
		{
			InputSettings->RegisterInputMappingContext(Context);
		}
	}

	TakeSnapshot();
	SelectTab(Tab);
	Super::NativeOnActivated();
}

void UEchoSettingsScreen::TakeSnapshot()
{
	if (const UEchoGameUserSettings* Settings = GetSettings())
	{
		Saved = Capture(Settings);
	}
	PendingKeyChanges.Reset();
}

bool UEchoSettingsScreen::HasUnsavedChanges() const
{
	const UEchoGameUserSettings* Settings = GetSettings();
	return PendingKeyChanges.Num() > 0 || (Settings && !Capture(Settings).Equals(Saved));
}

void UEchoSettingsScreen::SelectTab(int32 NewTab)
{
	Tab = (NewTab + 4) % 4;
	for (int32 Index = 0; Index < TabButtons.Num(); ++Index)
	{
		TabButtons[Index]->SetLit(Index == Tab);
	}
	RebuildRows();
}

FReply UEchoSettingsScreen::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const bool bListening = Rows.ContainsByPredicate([](const UEchoSettingRow* Row) { return Row && Row->IsListeningForKey(); });
	if (!bListening && (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Q || Key == EKeys::Gamepad_RightShoulder || Key == EKeys::E))
	{
		SelectTab(Tab + ((Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Q) ? -1 : 1));
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

UEchoSettingRow* UEchoSettingsScreen::AddRow()
{
	UEchoSettingRow* Row = CreateWidget<UEchoSettingRow>(this, UEchoSettingRow::StaticClass());
	RowList->AddChild(Row);
	Rows.Add(Row);
	return Row;
}

void UEchoSettingsScreen::AddHeading(const FText& Text)
{
	UTextBlock* Heading = EchoUI::MakeText(WidgetTree, Text, 16, EchoUI::AccentColor, true);
	RowList->AddChild(Heading);
}

void UEchoSettingsScreen::RebuildRows()
{
	RowList->ClearChildren();
	Rows.Reset();
	RowsByLabel.Reset();
	SubtitlePreview = nullptr;

	switch (Tab)
	{
	case 0: BuildGraphics(); break;
	case 1: BuildAudio(); break;
	case 2: BuildControls(); break;
	default: BuildAccessibility(); break;
	}

	// Start on the first row
	FirstFocus = Rows.Num() > 0 ? static_cast<UWidget*>(Rows[0]) : static_cast<UWidget*>(TabButtons[Tab]);
	if (IsActivated() && FirstFocus)
	{
		FirstFocus->SetFocus();
	}
	StatusText->SetText(HasUnsavedChanges() ? LOCTEXT("Unsaved", "You have unsaved changes: Apply to keep them.") : FText::GetEmpty());
}

// ------------------------------------------------------------------ tabs

void UEchoSettingsScreen::BuildGraphics()
{
	UEchoGameUserSettings* S = GetSettings();

	UEchoSettingRow* Row = AddRow();
	RowsByLabel.Add(TEXT("Window mode"), Row);
	Row->InitOptions(LOCTEXT("WindowMode", "Window mode (on Apply)"),
		{ LOCTEXT("Fullscreen", "Fullscreen"), LOCTEXT("Borderless", "Windowed fullscreen"), LOCTEXT("Windowed", "Windowed") },
		static_cast<int32>(S->GetFullscreenMode()), [this, S](int32 Index)
		{
			S->SetFullscreenMode(static_cast<EWindowMode::Type>(Index));
			StatusText->SetText(LOCTEXT("OnApply", "Window mode and resolution change when you Apply."));
		});

	Resolutions.Reset();
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	if (Resolutions.Num() == 0)
	{
		UKismetSystemLibrary::GetConvenientWindowedResolutions(Resolutions);
	}
	Resolutions.AddUnique(S->GetScreenResolution());
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X * A.Y < B.X * B.Y; });
	TArray<FText> ResolutionNames;
	for (const FIntPoint& Resolution : Resolutions)
	{
		ResolutionNames.Add(FText::FromString(FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y)));
	}
	Row = AddRow();
	RowsByLabel.Add(TEXT("Resolution"), Row);
	Row->InitOptions(LOCTEXT("Resolution", "Resolution (on Apply)"), ResolutionNames, Resolutions.IndexOfByKey(S->GetScreenResolution()), [this, S](int32 Index)
	{
		S->SetScreenResolution(Resolutions[Index]);
		StatusText->SetText(LOCTEXT("OnApply2", "Window mode and resolution change when you Apply."));
	});

	TArray<FText> Qualities = { LOCTEXT("Low", "Low"), LOCTEXT("Medium", "Medium"), LOCTEXT("High", "High"), LOCTEXT("Epic", "Epic") };
	int32 Quality = S->GetOverallScalabilityLevel();
	if (Quality < 0 || Quality > 3)
	{
		Qualities.Add(LOCTEXT("Custom", "Custom"));
		Quality = Qualities.Num() - 1;
	}
	Row = AddRow();
	RowsByLabel.Add(TEXT("Quality"), Row);
	Row->InitOptions(LOCTEXT("Quality", "Quality preset"), Qualities, Quality, [S](int32 Index)
	{
		if (Index <= 3)
		{
			S->SetOverallScalabilityLevel(Index);
			S->ApplyNonResolutionSettings();
		}
	});

	Row = AddRow();
	RowsByLabel.Add(TEXT("V-sync"), Row);
	Row->InitToggle(LOCTEXT("VSync", "V-sync"), S->IsVSyncEnabled(), [S](bool bOn)
	{
		S->SetVSyncEnabled(bOn);
		S->ApplyNonResolutionSettings();
	});

	int32 RateIndex = UE_ARRAY_COUNT(FrameRates) - 1;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(FrameRates); ++Index)
	{
		if (FMath::IsNearlyEqual(FrameRates[Index], S->GetFrameRateLimit()))
		{
			RateIndex = Index;
		}
	}
	Row = AddRow();
	RowsByLabel.Add(TEXT("Frame rate cap"), Row);
	Row->InitOptions(LOCTEXT("FrameRate", "Frame rate cap"),
		{ FText::FromString(TEXT("30")), FText::FromString(TEXT("60")), FText::FromString(TEXT("120")), FText::FromString(TEXT("144")), LOCTEXT("Unlimited", "Unlimited") },
		RateIndex, [S](int32 Index)
		{
			S->SetFrameRateLimit(FrameRates[Index]);
			S->ApplyNonResolutionSettings();
		});

	Row = AddRow();
	RowsByLabel.Add(TEXT("Brightness"), Row);
	Row->InitSlider(LOCTEXT("Brightness", "Brightness"), 0.5f, 1.5f, 0.05f, S->Brightness, &Percent, [S](float Value)
	{
		S->Brightness = Value;
		S->ApplyDisplayAndAccessibility();
	});
}

void UEchoSettingsScreen::BuildAudio()
{
	UEchoGameUserSettings* S = GetSettings();

	struct FVolume { const TCHAR* Label; FText Text; float UEchoGameUserSettings::* Member; };
	const FVolume Volumes[] =
	{
		{ TEXT("Master volume"), LOCTEXT("Master", "Master volume"), &UEchoGameUserSettings::MasterVolume },
		{ TEXT("Music volume"), LOCTEXT("Music", "Music volume"), &UEchoGameUserSettings::MusicVolume },
		{ TEXT("Sound effects volume"), LOCTEXT("Effects", "Sound effects volume"), &UEchoGameUserSettings::SoundEffectsVolume },
		{ TEXT("Dialogue volume"), LOCTEXT("Dialogue", "Dialogue volume"), &UEchoGameUserSettings::DialogueVolume },
		{ TEXT("Voice chat volume"), LOCTEXT("Voice", "Voice chat volume"), &UEchoGameUserSettings::VoiceChatVolume },
	};
	for (const FVolume& Volume : Volumes)
	{
		UEchoSettingRow* Row = AddRow();
		RowsByLabel.Add(Volume.Label, Row);
		float UEchoGameUserSettings::* Member = Volume.Member;
		Row->InitSlider(Volume.Text, 0.0f, 1.0f, 0.05f, S->*Member, &Percent, [S, Member](float Value)
		{
			S->*Member = Value;
			S->ApplyAudio();
		});
	}

	UEchoSettingRow* Row = AddRow();
	RowsByLabel.Add(TEXT("Microphone"), Row);
	Row->InitToggle(LOCTEXT("Mic", "Microphone (voice chat)"), S->bMicrophoneEnabled, [S](bool bOn) { S->bMicrophoneEnabled = bOn; });
}

void UEchoSettingsScreen::BuildControls()
{
	UEchoGameUserSettings* S = GetSettings();

	UEchoSettingRow* Row = AddRow();
	RowsByLabel.Add(TEXT("Mouse sensitivity"), Row);
	Row->InitSlider(LOCTEXT("MouseSens", "Mouse sensitivity"), 0.1f, 3.0f, 0.05f, S->MouseSensitivity, &Multiplier, [S](float Value) { S->MouseSensitivity = Value; });

	Row = AddRow();
	RowsByLabel.Add(TEXT("Aim sensitivity"), Row);
	Row->InitSlider(LOCTEXT("AimSens", "Aim sensitivity (while aiming)"), 0.1f, 2.0f, 0.05f, S->AimSensitivity, &Multiplier, [S](float Value) { S->AimSensitivity = Value; });

	Row = AddRow();
	RowsByLabel.Add(TEXT("Gamepad sensitivity"), Row);
	Row->InitSlider(LOCTEXT("PadSens", "Gamepad sensitivity"), 0.1f, 3.0f, 0.05f, S->GamepadSensitivity, &Multiplier, [S](float Value) { S->GamepadSensitivity = Value; });

	Row = AddRow();
	RowsByLabel.Add(TEXT("Invert Y"), Row);
	Row->InitToggle(LOCTEXT("InvertY", "Invert Y"), S->bInvertY, [S](bool bOn) { S->bInvertY = bOn; });

	Row = AddRow();
	RowsByLabel.Add(TEXT("Aim mode"), Row);
	Row->InitOptions(LOCTEXT("AimMode", "Aim mode"), { LOCTEXT("Hold", "Hold"), LOCTEXT("Toggle", "Toggle") }, static_cast<int32>(S->AimMode),
		[S](int32 Index) { S->AimMode = static_cast<EEchoAimMode>(Index); });

	// Key bindings (keyboard and mouse), from Enhanced Input's user settings
	UEnhancedInputUserSettings* InputSettings = GetInputSettings();
	const UEnhancedPlayerMappableKeyProfile* Profile = InputSettings ? InputSettings->GetActiveKeyProfile() : nullptr;
	AddHeading(LOCTEXT("Keys", "Keyboard and mouse (Enter / A to change, Esc to cancel)"));
	if (!Profile)
	{
		AddHeading(LOCTEXT("NoKeys", "Key rebinding isn't available."));
		return;
	}

	TArray<FName> Names;
	for (const TCHAR* Name : KeyOrder)
	{
		if (Profile->FindKeyMappingRow(Name))
		{
			Names.Add(Name);
		}
	}
	for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
	{
		Names.AddUnique(Pair.Key);
	}

	for (const FName Name : Names)
	{
		const FPlayerKeyMapping* Mapping = FindKeyboardMapping(InputSettings, Name);
		if (!Mapping)
		{
			continue;
		}
		Row = AddRow();
		RowsByLabel.Add(Name.ToString(), Row);
		const FText DisplayName = Mapping->GetDisplayName().IsEmpty() ? FText::FromName(Name) : Mapping->GetDisplayName();
		Row->InitKey(DisplayName, Mapping->GetCurrentKey(), [this, Name](const FKey& NewKey) { RebindKey(Name, NewKey); });
	}
}

void UEchoSettingsScreen::BuildAccessibility()
{
	UEchoGameUserSettings* S = GetSettings();

	UEchoSettingRow* Row = AddRow();
	RowsByLabel.Add(TEXT("Subtitles"), Row);
	Row->InitToggle(LOCTEXT("Subs", "Subtitles"), S->bSubtitlesEnabled, [this, S](bool bOn)
	{
		S->bSubtitlesEnabled = bOn;
		S->ApplyDisplayAndAccessibility();
		UpdatePreview();
	});

	int32 SizeIndex = 1;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SubtitleScales); ++Index)
	{
		if (FMath::IsNearlyEqual(SubtitleScales[Index], S->SubtitleScale))
		{
			SizeIndex = Index;
		}
	}
	Row = AddRow();
	RowsByLabel.Add(TEXT("Subtitle size"), Row);
	Row->InitOptions(LOCTEXT("SubSize", "Subtitle size"), { LOCTEXT("Small", "Small"), LOCTEXT("MediumSize", "Medium"), LOCTEXT("Large", "Large"), LOCTEXT("XLarge", "Extra large") },
		SizeIndex, [this, S](int32 Index)
		{
			S->SubtitleScale = SubtitleScales[Index];
			UpdatePreview();
		});

	Row = AddRow();
	RowsByLabel.Add(TEXT("Reduce camera shake"), Row);
	Row->InitToggle(LOCTEXT("Shake", "Reduce camera shake"), S->bReduceCameraShake, [S](bool bOn) { S->bReduceCameraShake = bOn; });

	// What subtitles will look like
	SubtitlePreview = EchoUI::MakeText(WidgetTree, LOCTEXT("SubPreview", "Saraa: \"I'm right here, brother.\""), 24);
	RowList->AddChild(SubtitlePreview);
	UpdatePreview();
}

void UEchoSettingsScreen::UpdatePreview()
{
	const UEchoGameUserSettings* S = GetSettings();
	if (SubtitlePreview && S)
	{
		SubtitlePreview->SetFont(EchoUI::Font(FMath::RoundToInt(24.0f * S->SubtitleScale)));
		SubtitlePreview->SetVisibility(S->bSubtitlesEnabled ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

// ------------------------------------------------------------------ keys

FKey UEchoSettingsScreen::GetBoundKey(FName MappingName) const
{
	const FPlayerKeyMapping* Mapping = FindKeyboardMapping(GetInputSettings(), MappingName);
	return Mapping ? Mapping->GetCurrentKey() : EKeys::Invalid;
}

bool UEchoSettingsScreen::RebindKey(FName MappingName, FKey NewKey)
{
	UEnhancedInputUserSettings* InputSettings = GetInputSettings();
	const FPlayerKeyMapping* Mapping = FindKeyboardMapping(InputSettings, MappingName);
	if (!Mapping || !NewKey.IsValid())
	{
		return false;
	}

	FKeyChange Change;
	Change.MappingName = MappingName;
	Change.Slot = static_cast<uint8>(Mapping->GetSlot());
	Change.OldKey = Mapping->GetCurrentKey();

	FMapPlayerKeyArgs Args;
	Args.MappingName = MappingName;
	Args.Slot = Mapping->GetSlot();
	Args.NewKey = NewKey;
	FGameplayTagContainer Failure;
	InputSettings->MapPlayerKey(Args, Failure);
	if (!Failure.IsEmpty())
	{
		UE_LOG(LogOurLastEcho, Warning, TEXT("Settings: couldn't bind %s to %s (%s)"), *MappingName.ToString(), *NewKey.ToString(), *Failure.ToStringSimple());
		return false;
	}
	InputSettings->ApplySettings();
	PendingKeyChanges.Add(Change);
	StatusText->SetText(LOCTEXT("Unsaved2", "You have unsaved changes: Apply to keep them."));
	UE_LOG(LogOurLastEcho, Log, TEXT("Settings: %s is now %s"), *MappingName.ToString(), *NewKey.ToString());

	if (TObjectPtr<UEchoSettingRow>* Row = RowsByLabel.Find(MappingName.ToString()))
	{
		(*Row)->SetKey(NewKey);
	}
	return true;
}

// ------------------------------------------------------------------ apply / discard / reset

void UEchoSettingsScreen::ApplyChanges()
{
	UEchoGameUserSettings* S = GetSettings();
	if (!S)
	{
		return;
	}

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Play-In-Editor: changing the window mode or resolution would resize the editor, so only save them
		S->ApplyNonResolutionSettings();
		S->SaveSettings();
	}
	else
#endif
	{
		S->ApplySettings(false);
	}

	if (UEnhancedInputUserSettings* InputSettings = GetInputSettings())
	{
		InputSettings->SaveSettings();
	}
	TakeSnapshot();
	StatusText->SetText(LOCTEXT("Applied", "Settings saved."));
	UE_LOG(LogOurLastEcho, Log, TEXT("Settings: applied and saved"));
}

void UEchoSettingsScreen::DiscardChanges()
{
	UEchoGameUserSettings* S = GetSettings();
	if (S)
	{
		Restore(S, Saved);
		S->ApplyNonResolutionSettings();
	}

	// Undo key changes, newest first
	if (UEnhancedInputUserSettings* InputSettings = GetInputSettings())
	{
		for (int32 Index = PendingKeyChanges.Num() - 1; Index >= 0; --Index)
		{
			const FKeyChange& Change = PendingKeyChanges[Index];
			FMapPlayerKeyArgs Args;
			Args.MappingName = Change.MappingName;
			Args.Slot = static_cast<EPlayerMappableKeySlot>(Change.Slot);
			Args.NewKey = Change.OldKey;
			FGameplayTagContainer Failure;
			InputSettings->MapPlayerKey(Args, Failure);
		}
		InputSettings->ApplySettings();
	}
	PendingKeyChanges.Reset();
	RebuildRows();
}

void UEchoSettingsScreen::ResetToDefaults()
{
	UEchoGameUserSettings* S = GetSettings();
	if (!S)
	{
		return;
	}
	S->SetToDefaults();
	S->ApplyNonResolutionSettings();

	// Key bindings back to their defaults (each change is remembered so Discard can undo it)
	if (UEnhancedInputUserSettings* InputSettings = GetInputSettings())
	{
		if (const UEnhancedPlayerMappableKeyProfile* Profile = InputSettings->GetActiveKeyProfile())
		{
			for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
			{
				for (const FPlayerKeyMapping& Mapping : Pair.Value.Mappings)
				{
					if (Mapping.GetCurrentKey() != Mapping.GetDefaultKey())
					{
						PendingKeyChanges.Add({ Pair.Key, static_cast<uint8>(Mapping.GetSlot()), Mapping.GetCurrentKey() });
					}
				}
			}
		}
		FGameplayTagContainer Failure;
		InputSettings->ResetKeyProfileIdToDefault(InputSettings->GetActiveKeyProfile()->GetProfileIdString(), Failure);
		InputSettings->ApplySettings();
	}
	RebuildRows();
}

void UEchoSettingsScreen::OnBack()
{
	if (!HasUnsavedChanges())
	{
		DeactivateWidget();
		return;
	}

	GetRoot()->ShowDialog(LOCTEXT("UnsavedTitle", "Unsaved changes"), LOCTEXT("UnsavedMessage", "Keep the changes you made?"),
	{
		{ LOCTEXT("UnsavedApply", "Apply"), [this]() { ApplyChanges(); bCloseWhenActivated = true; } },
		{ LOCTEXT("UnsavedDiscard", "Discard"), [this]() { DiscardChanges(); bCloseWhenActivated = true; } },
		{ LOCTEXT("UnsavedCancel", "Cancel"), nullptr },
	});
}

// ------------------------------------------------------------------ test helpers

bool UEchoSettingsScreen::StepRow(const FString& RowLabel, int32 Direction)
{
	TObjectPtr<UEchoSettingRow>* Row = RowsByLabel.Find(RowLabel);
	if (!Row)
	{
		return false;
	}
	(*Row)->Step(Direction);
	if (HasUnsavedChanges())
	{
		StatusText->SetText(LOCTEXT("Unsaved3", "You have unsaved changes: Apply to keep them."));
	}
	return true;
}

FString UEchoSettingsScreen::GetRowValue(const FString& RowLabel) const
{
	const TObjectPtr<UEchoSettingRow>* Row = RowsByLabel.Find(RowLabel);
	return Row ? (*Row)->GetValueText() : FString();
}

#undef LOCTEXT_NAMESPACE
