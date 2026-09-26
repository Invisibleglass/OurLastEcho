// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "EchoCameraShake.generated.h"

class APlayerController;

/**
 *  A small, short camera kick (firing the bow, the whip snapping taut). Always play it through Play(), which
 *  scales it by the player's Reduce Camera Shake setting.
 */
UCLASS()
class UEchoCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:

	UEchoCameraShake(const FObjectInitializer& ObjectInitializer);

	/** Plays the shake on a local player's camera at Strength x the settings' camera shake scale */
	static void Play(APlayerController* PlayerController, float Strength = 1.0f);

	/** The scale the last shake played at (for tests) */
	UFUNCTION(BlueprintPure, Category="Echo|Camera")
	static float GetLastPlayedScale() { return LastPlayedScale; }

private:

	static float LastPlayedScale;
};
