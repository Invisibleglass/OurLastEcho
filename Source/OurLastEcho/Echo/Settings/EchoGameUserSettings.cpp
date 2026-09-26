// Our Last Echo

#include "EchoGameUserSettings.h"
#include "AudioDevice.h"
#include "AudioThread.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "OurLastEcho.h"

UEchoGameUserSettings::UEchoGameUserSettings()
{
}

UEchoGameUserSettings* UEchoGameUserSettings::GetEchoSettings()
{
	return GEngine ? Cast<UEchoGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UEchoGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	// Written out rather than copied from the class default object: this is a config class, so its CDO holds the
	// values loaded from GameUserSettings.ini (the player's saved settings), not the defaults.
	// Keep in sync with the initialisers in the header.
	Brightness = 1.0f;
	MasterVolume = 1.0f;
	MusicVolume = 0.8f;
	SoundEffectsVolume = 1.0f;
	DialogueVolume = 1.0f;
	VoiceChatVolume = 1.0f;
	bMicrophoneEnabled = true;
	MouseSensitivity = 1.0f;
	AimSensitivity = 0.6f;
	GamepadSensitivity = 1.0f;
	bInvertY = false;
	AimMode = EEchoAimMode::Hold;
	bSubtitlesEnabled = true;
	SubtitleScale = 1.0f;
	bReduceCameraShake = false;
}

void UEchoGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();

	ApplyAudio();
	ApplyDisplayAndAccessibility();
}

void UEchoGameUserSettings::ApplyDisplayAndAccessibility() const
{
	if (!GEngine)
	{
		return;
	}

	// 2.2 is the engine's default display gamma; brightness moves it by up to +-0.6
	GEngine->DisplayGamma = 2.2f + (FMath::Clamp(Brightness, 0.5f, 1.5f) - 1.0f) * 1.2f;
	GEngine->bSubtitlesEnabled = bSubtitlesEnabled;
}

void UEchoGameUserSettings::ApplyAudio() const
{
	if (!GEngine)
	{
		return;
	}

	// Every game world on this machine (in PIE each player has its own world and audio device)
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (World && World->IsGameWorld())
		{
			ApplyAudioToWorld(World);
		}
	}
}

void UEchoGameUserSettings::ApplyAudioToWorld(UWorld* World) const
{
	USoundMix* Mix = Cast<USoundMix>(SettingsSoundMix.TryLoad());
	USoundClass* Master = Cast<USoundClass>(MasterSoundClass.TryLoad());
	if (!Mix || !Master)
	{
		UE_LOG(LogOurLastEcho, Warning, TEXT("Settings: sound mix or classes missing (run Scripts/build_title_screen.py)"));
		return;
	}

	// The master class scales everything under it; the others are its children
	UGameplayStatics::SetSoundMixClassOverride(World, Mix, Master, FMath::Clamp(MasterVolume, 0.0f, 1.0f), 1.0f, 0.0f, true);
	const TPair<const FSoftObjectPath*, float> Children[] =
	{
		{ &MusicSoundClass, MusicVolume },
		{ &EffectsSoundClass, SoundEffectsVolume },
		{ &DialogueSoundClass, DialogueVolume },
		{ &VoiceSoundClass, VoiceChatVolume },
	};
	for (const TPair<const FSoftObjectPath*, float>& Child : Children)
	{
		if (USoundClass* Class = Cast<USoundClass>(Child.Key->TryLoad()))
		{
			UGameplayStatics::SetSoundMixClassOverride(World, Mix, Class, FMath::Clamp(Child.Value, 0.0f, 1.0f), 1.0f, 0.0f, true);
		}
	}

	// Pop first so repeated applies don't stack references to the same mix
	UGameplayStatics::PopSoundMixModifier(World, Mix);
	UGameplayStatics::PushSoundMixModifier(World, Mix);
}

float UEchoGameUserSettings::GetLookScale(bool bGamepad, bool bAiming) const
{
	const float Base = bGamepad ? GamepadSensitivity : MouseSensitivity;
	return FMath::Max(0.01f, Base) * (bAiming ? FMath::Max(0.01f, AimSensitivity) : 1.0f);
}

USoundClass* UEchoGameUserSettings::GetSoundClassForVolume(FName VolumeName) const
{
	const FSoftObjectPath* Path =
		VolumeName == TEXT("Master") ? &MasterSoundClass :
		VolumeName == TEXT("Music") ? &MusicSoundClass :
		VolumeName == TEXT("Effects") ? &EffectsSoundClass :
		VolumeName == TEXT("Dialogue") ? &DialogueSoundClass :
		VolumeName == TEXT("Voice") ? &VoiceSoundClass : nullptr;
	return Path ? Cast<USoundClass>(Path->TryLoad()) : nullptr;
}

float UEchoGameUserSettings::GetEffectiveSoundClassVolume(UObject* WorldContextObject, USoundClass* SoundClass)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	FAudioDeviceHandle Device = World ? World->GetAudioDevice() : FAudioDeviceHandle();
	if (!Device || !SoundClass)
	{
		return -1.0f;
	}
	// Sound class state lives on the audio thread: read it there and wait
	float Volume = -1.0f;
	FAudioDevice* DevicePtr = Device.GetAudioDevice();
	FAudioThread::RunCommandOnAudioThread([DevicePtr, SoundClass, &Volume]()
	{
		if (const FSoundClassProperties* Properties = DevicePtr->GetSoundClassCurrentProperties(SoundClass))
		{
			Volume = Properties->Volume;
		}
	});
	FAudioCommandFence Fence;
	Fence.BeginFence();
	Fence.Wait();
	return Volume;
}
