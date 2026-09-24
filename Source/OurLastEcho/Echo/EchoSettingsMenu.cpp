// Our Last Echo

#include "EchoSettingsMenu.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EchoAudioSettings.h"
#include "OurLastEchoPlayerController.h"

void UEchoSettingsMenu::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (MasterVolumeSlider)
	{
		const float Volume = UEchoAudioSettings::GetMasterVolume();
		MasterVolumeSlider->SetMinValue(0.0f);
		MasterVolumeSlider->SetMaxValue(1.0f);
		MasterVolumeSlider->SetStepSize(0.05f);
		MasterVolumeSlider->SetValue(Volume);
		MasterVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &UEchoSettingsMenu::OnVolumeChanged);
		RefreshVolumeText(Volume);
	}
	if (ResumeButton)
	{
		ResumeButton->OnClicked.AddUniqueDynamic(this, &UEchoSettingsMenu::OnResumeClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddUniqueDynamic(this, &UEchoSettingsMenu::OnQuitClicked);
	}
}

void UEchoSettingsMenu::FocusFirstControl()
{
	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->SetKeyboardFocus();
	}
	else
	{
		SetKeyboardFocus();
	}
}

FReply UEchoSettingsMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// The menu keys close it again (the game's own input is off while the menu has focus)
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::P || Key == EKeys::Gamepad_Special_Right || Key == EKeys::Gamepad_FaceButton_Right)
	{
		OnResumeClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UEchoSettingsMenu::OnVolumeChanged(float Value)
{
	UEchoAudioSettings::SetMasterVolume(this, Value);
	RefreshVolumeText(Value);
}

void UEchoSettingsMenu::RefreshVolumeText(float Value)
{
	if (MasterVolumeText)
	{
		MasterVolumeText->SetText(FText::AsPercent(Value));
	}
}

void UEchoSettingsMenu::OnResumeClicked()
{
	if (AOurLastEchoPlayerController* PC = Cast<AOurLastEchoPlayerController>(GetOwningPlayer()))
	{
		PC->CloseSettingsMenu();
	}
}

void UEchoSettingsMenu::OnQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
