// Our Last Echo

#include "EchoAudioSettings.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* ConfigSection = TEXT("/Script/OurLastEcho.EchoAudio");
	const TCHAR* ConfigKey = TEXT("MasterVolume");

	void ApplyToDevice(const UObject* WorldContextObject, float Volume)
	{
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
		if (!World)
		{
			return;
		}
		if (FAudioDeviceHandle Device = World->GetAudioDevice())
		{
			Device->SetTransientPrimaryVolume(Volume);
		}
	}
}

float UEchoAudioSettings::GetMasterVolume()
{
	float Volume = 1.0f;
	if (GConfig)
	{
		GConfig->GetFloat(ConfigSection, ConfigKey, Volume, GGameUserSettingsIni);
	}
	return FMath::Clamp(Volume, 0.0f, 1.0f);
}

void UEchoAudioSettings::SetMasterVolume(const UObject* WorldContextObject, float Volume)
{
	Volume = FMath::Clamp(Volume, 0.0f, 1.0f);
	if (GConfig)
	{
		GConfig->SetFloat(ConfigSection, ConfigKey, Volume, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	ApplyToDevice(WorldContextObject, Volume);
}

void UEchoAudioSettings::ApplySavedVolume(const UObject* WorldContextObject)
{
	ApplyToDevice(WorldContextObject, GetMasterVolume());
}
