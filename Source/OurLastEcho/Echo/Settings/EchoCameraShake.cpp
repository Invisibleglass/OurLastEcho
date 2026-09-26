// Our Last Echo

#include "EchoCameraShake.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Shakes/PerlinNoiseCameraShakePattern.h"
#include "EchoGameUserSettings.h"

float UEchoCameraShake::LastPlayedScale = 0.0f;

UEchoCameraShake::UEchoCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UPerlinNoiseCameraShakePattern* Pattern = CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(TEXT("Pattern"));
	Pattern->Duration = 0.25f;
	Pattern->BlendInTime = 0.02f;
	Pattern->BlendOutTime = 0.15f;
	Pattern->LocationAmplitudeMultiplier = 0.0f;
	Pattern->Pitch.Amplitude = 0.8f;
	Pattern->Pitch.Frequency = 14.0f;
	Pattern->Yaw.Amplitude = 0.5f;
	Pattern->Yaw.Frequency = 11.0f;
	SetRootShakePattern(Pattern);
}

void UEchoCameraShake::Play(APlayerController* PlayerController, float Strength)
{
	if (!PlayerController || !PlayerController->IsLocalController() || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	const UEchoGameUserSettings* Settings = UEchoGameUserSettings::GetEchoSettings();
	LastPlayedScale = Strength * (Settings ? Settings->GetCameraShakeScale() : 1.0f);
	PlayerController->PlayerCameraManager->StartCameraShake(StaticClass(), LastPlayedScale);
}
