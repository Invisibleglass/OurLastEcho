// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EchoSettingsMenu.generated.h"

class UButton;
class USlider;
class UTextBlock;

/**
 *  The pause / settings menu. Logic lives here; the layout is the Widget Blueprint
 *  /Game/Echo/UI/WBP_SettingsMenu (restyle it freely, just keep the widget names below).
 *
 *  Opening it (AOurLastEchoPlayerController::OpenSettingsMenu) pauses the game for BOTH players until
 *  everyone who opened it has closed it. Closes on Resume, Esc/P, or gamepad Start/B.
 */
UCLASS(Abstract)
class UEchoSettingsMenu : public UUserWidget
{
	GENERATED_BODY()

protected:

	/** Master volume, 0..1 (this machine only) */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> MasterVolumeSlider;

	/** Shows the volume as a percentage */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MasterVolumeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ResumeButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuitButton;

public:

	/** Puts keyboard/gamepad focus on the volume slider */
	UFUNCTION(BlueprintCallable, Category="Echo|Settings Menu")
	void FocusFirstControl();

protected:

	virtual void NativeConstruct() override;

	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void OnVolumeChanged(float Value);

	UFUNCTION()
	void OnResumeClicked();

	UFUNCTION()
	void OnQuitClicked();

	void RefreshVolumeText(float Value);
};
