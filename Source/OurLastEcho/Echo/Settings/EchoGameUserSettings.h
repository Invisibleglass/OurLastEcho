// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "EchoGameUserSettings.generated.h"

class USoundClass;
class USoundMix;
class UWorld;

/** Aiming with the spirit bow: hold the aim button, or press once to start and again to stop */
UENUM(BlueprintType)
enum class EEchoAimMode : uint8
{
	Hold,
	Toggle
};

/**
 *  All of the game's settings, on top of the engine's graphics settings (window mode, resolution, quality,
 *  v-sync, frame rate cap). Saved per machine in Saved/Config/<Platform>/GameUserSettings.ini and loaded at
 *  start-up (set as GameUserSettingsClassName in DefaultEngine.ini).
 *
 *  Everything applies immediately (ApplyNonResolutionSettings / ApplyAudio / ApplyInputAndAccessibility)
 *  except the window mode and resolution, which apply with ApplyResolutionSettings (the settings menu's
 *  Apply button). Key bindings are separate: Enhanced Input's own user settings save those.
 */
UCLASS(config=GameUserSettings, configdonotcheckdefaults)
class UEchoGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:

	UEchoGameUserSettings();

	/** The engine's settings object, as ours (never null once the engine is up) */
	UFUNCTION(BlueprintPure, Category="Echo|Settings")
	static UEchoGameUserSettings* GetEchoSettings();

	// ---- Graphics (the rest are UGameUserSettings')

	/** 0.5 .. 1.5, 1 = default. Applied as the display gamma */
	UPROPERTY(Config, BlueprintReadWrite, Category="Echo|Settings|Graphics")
	float Brightness = 1.0f;

	// ---- Audio, all 0..1

	UPROPERTY(Config, BlueprintReadWrite, Category="Echo|Settings|Audio")
	float MasterVolume = 1.0f;

	UPROPERTY(Config, BlueprintReadWrite, Category="Echo|Settings|Audio")
	float MusicVolume = 0.8f;

	UPROPERTY(Config, BlueprintReadWrite, Category="Echo|Settings|Audio")
	float SoundEffectsVolume = 1.0f;

	UPROPERTY(Config, BlueprintReadWrite, Category="Echo|Settings|Audio")
	float DialogueVolume = 1.0f;

	UPROPERTY(Config, BlueprintReadWrite, Category="Echo|Settings|Audio")
	float VoiceChatVolume = 1.0f;

	/** Voice chat microphone (there's no voice chat yet: this is the switch it will read) */
	UPROPERTY(Config, BlueprintReadWrite, Category="Echo|Settings|Audio")
	bool bMicrophoneEnabled = true;

	// ---- Controls

	/** Multiplies mouse look (0.1 .. 3) */
	UPROPERTY(Config, BlueprintReadWrite, Category="Echo|Settings|Controls")
	float MouseSensitivity = 1.0f;

	/** Multiplies look input while aiming the bow (0.1 .. 2), on top of the mouse/gamepad sensitivity */
	UPROPERTY(Config, BlueprintReadWrite, Category="Echo|Settings|Controls")
	float AimSensitivity = 0.6f;

	/** Multiplies gamepad look (0.1 .. 3) */
	UPROPERTY(Config, BlueprintReadWrite, Category="Echo|Settings|Controls")
	float GamepadSensitivity = 1.0f;

	UPROPERTY(Config, BlueprintReadWrite, Category="Echo|Settings|Controls")
	bool bInvertY = false;

	UPROPERTY(Config, BlueprintReadWrite, Category="Echo|Settings|Controls")
	EEchoAimMode AimMode = EEchoAimMode::Hold;

	// ---- Accessibility

	UPROPERTY(Config, BlueprintReadWrite, Category="Echo|Settings|Accessibility")
	bool bSubtitlesEnabled = true;

	/** Subtitle text scale: 0.75 small, 1 medium, 1.35 large, 1.75 extra large */
	UPROPERTY(Config, BlueprintReadWrite, Category="Echo|Settings|Accessibility")
	float SubtitleScale = 1.0f;

	/** Camera shakes play at a fifth of their strength */
	UPROPERTY(Config, BlueprintReadWrite, Category="Echo|Settings|Accessibility")
	bool bReduceCameraShake = false;

	// ---- Applying

	/** Resets everything (graphics included) to defaults; call Apply... to use them */
	virtual void SetToDefaults() override;

	/** Graphics except resolution/window mode, plus our audio, brightness and subtitle settings */
	virtual void ApplyNonResolutionSettings() override;

	/** Pushes the volumes into the game's sound mix (every game world on this machine) */
	UFUNCTION(BlueprintCallable, Category="Echo|Settings")
	void ApplyAudio() const;

	/** Brightness (display gamma) and subtitles */
	UFUNCTION(BlueprintCallable, Category="Echo|Settings")
	void ApplyDisplayAndAccessibility() const;

	/** Look input multiplier for mouse or gamepad, with the aim multiplier while aiming */
	UFUNCTION(BlueprintPure, Category="Echo|Settings")
	float GetLookScale(bool bGamepad, bool bAiming) const;

	/** 1 normally, 0.2 with Reduce Camera Shake on */
	UFUNCTION(BlueprintPure, Category="Echo|Settings")
	float GetCameraShakeScale() const { return bReduceCameraShake ? 0.2f : 1.0f; }

	/** The sound class each volume drives (for tests and for assigning sounds) */
	UFUNCTION(BlueprintPure, Category="Echo|Settings")
	USoundClass* GetSoundClassForVolume(FName VolumeName) const;

	/** Effective volume of a sound class on this world's audio device, after the settings mix (for tests) */
	UFUNCTION(BlueprintCallable, Category="Echo|Settings", meta = (WorldContext = "WorldContextObject"))
	static float GetEffectiveSoundClassVolume(UObject* WorldContextObject, USoundClass* SoundClass);

	/** The sound mix and classes, made by Scripts/build_title_screen.py */
	UPROPERTY(Config)
	FSoftObjectPath SettingsSoundMix = FSoftObjectPath(TEXT("/Game/Echo/Audio/SMix_Settings.SMix_Settings"));

	UPROPERTY(Config)
	FSoftObjectPath MasterSoundClass = FSoftObjectPath(TEXT("/Game/Echo/Audio/SC_Master.SC_Master"));

	UPROPERTY(Config)
	FSoftObjectPath MusicSoundClass = FSoftObjectPath(TEXT("/Game/Echo/Audio/SC_Music.SC_Music"));

	UPROPERTY(Config)
	FSoftObjectPath EffectsSoundClass = FSoftObjectPath(TEXT("/Game/Echo/Audio/SC_SFX.SC_SFX"));

	UPROPERTY(Config)
	FSoftObjectPath DialogueSoundClass = FSoftObjectPath(TEXT("/Game/Echo/Audio/SC_Dialogue.SC_Dialogue"));

	UPROPERTY(Config)
	FSoftObjectPath VoiceSoundClass = FSoftObjectPath(TEXT("/Game/Echo/Audio/SC_Voice.SC_Voice"));

private:

	void ApplyAudioToWorld(UWorld* World) const;
};
