// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "EchoUIWidgets.h"
#include "EchoSettingsScreen.generated.h"

class UEchoGameUserSettings;
class UEnhancedInputUserSettings;
class UScrollBox;
class UInputMappingContext;

/**
 *  Settings, in four tabs: Graphics, Audio, Controls (with key rebinding) and Accessibility.
 *
 *  Changes take effect immediately, so you see and hear them, except window mode and resolution, which wait for
 *  Apply (a bad resolution shouldn't be one arrow press away). Apply saves everything; leaving with unsaved
 *  changes asks whether to apply or discard them, and Discard puts every setting back as it was.
 *  Reset to Defaults resets every setting and key binding (then Apply to keep them).
 *  Tabs: click them, or LB / RB (Q / E on a keyboard).
 */
UCLASS()
class UEchoSettingsScreen : public UEchoScreen
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category="Echo|Settings")
	void SelectTab(int32 NewTab);

	UFUNCTION(BlueprintPure, Category="Echo|Settings")
	int32 GetTab() const { return Tab; }

	/** Applies (window mode and resolution too) and saves */
	UFUNCTION(BlueprintCallable, Category="Echo|Settings")
	void ApplyChanges();

	/** Puts everything back as it was when the screen opened (or at the last Apply) */
	UFUNCTION(BlueprintCallable, Category="Echo|Settings")
	void DiscardChanges();

	UFUNCTION(BlueprintCallable, Category="Echo|Settings")
	void ResetToDefaults();

	UFUNCTION(BlueprintPure, Category="Echo|Settings")
	bool HasUnsavedChanges() const;

	/** Presses left (-1) or right (+1) on the row with this label in the current tab, like the arrow keys (for tests) */
	UFUNCTION(BlueprintCallable, Category="Echo|Settings")
	bool StepRow(const FString& RowLabel, int32 Direction);

	/** The row's shown value (for tests) */
	UFUNCTION(BlueprintPure, Category="Echo|Settings")
	FString GetRowValue(const FString& RowLabel) const;

	/** Rebinds a key mapping (e.g. "Jump") on the keyboard/mouse, as choosing a key in its row does (for tests) */
	UFUNCTION(BlueprintCallable, Category="Echo|Settings")
	bool RebindKey(FName MappingName, FKey NewKey);

	/** The current keyboard/mouse key of a mapping (for tests) */
	UFUNCTION(BlueprintPure, Category="Echo|Settings")
	FKey GetBoundKey(FName MappingName) const;

	/** The mappable input contexts the Controls tab registers with Enhanced Input's user settings */
	static TArray<UInputMappingContext*> LoadMappableContexts();

protected:

	virtual void BuildContent(UVerticalBox* Content) override;
	virtual void NativeOnActivated() override;
	virtual void OnBack() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void RebuildRows();
	void BuildGraphics();
	void BuildAudio();
	void BuildControls();
	void BuildAccessibility();

	UEchoSettingRow* AddRow();
	void AddHeading(const FText& Text);

	void TakeSnapshot();
	void UpdatePreview();

	UEchoGameUserSettings* GetSettings() const;
	UEnhancedInputUserSettings* GetInputSettings() const;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> RowList;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UEchoButton>> TabButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UEchoSettingRow>> Rows;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SubtitlePreview;

	/** Every setting's value, for Discard and the unsaved-changes check */
	struct FValues
	{
		int32 WindowMode = 0;
		FIntPoint Resolution = FIntPoint::ZeroValue;
		int32 Quality = 0;
		bool bVSync = false;
		float FrameRateLimit = 0.0f;
		float Brightness = 1.0f;
		float Master = 1.0f;
		float Music = 1.0f;
		float Effects = 1.0f;
		float Dialogue = 1.0f;
		float Voice = 1.0f;
		bool bMicrophone = true;
		float Mouse = 1.0f;
		float Aim = 1.0f;
		float Gamepad = 1.0f;
		bool bInvertY = false;
		uint8 AimMode = 0;
		bool bSubtitles = true;
		float SubtitleScale = 1.0f;
		bool bReduceShake = false;

		bool Equals(const FValues& Other) const;
	};

	static FValues Capture(const UEchoGameUserSettings* Settings);
	static void Restore(UEchoGameUserSettings* Settings, const FValues& Values);

	/** As last saved */
	FValues Saved;

	struct FKeyChange
	{
		FName MappingName;
		uint8 Slot = 0;
		FKey OldKey;
	};
	TArray<FKeyChange> PendingKeyChanges;

	/** Set by the unsaved-changes dialog's Apply / Discard: close as soon as the dialog has gone and this screen is
	 *  on top again (taking this screen off the stack while the dialog is still closing jams CommonUI's stack) */
	bool bCloseWhenActivated = false;

	TMap<FString, TObjectPtr<UEchoSettingRow>> RowsByLabel;
	TArray<FIntPoint> Resolutions;
	int32 Tab = 0;
};
