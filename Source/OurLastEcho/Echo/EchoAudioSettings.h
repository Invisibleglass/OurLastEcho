// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EchoAudioSettings.generated.h"

/**
 *  Per-machine sound volume. Each player sets their own; it's saved in the local GameUserSettings.ini
 *  ([/Script/OurLastEcho.EchoAudio] MasterVolume) and applied to the world's audio device, so it scales
 *  every sound that machine plays.
 */
UCLASS()
class UEchoAudioSettings : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Saved master volume, 0..1 (1 if never set) */
	UFUNCTION(BlueprintPure, Category="Echo|Audio")
	static float GetMasterVolume();

	/** Sets, saves and applies the master volume (0..1) on this machine */
	UFUNCTION(BlueprintCallable, Category="Echo|Audio", meta = (WorldContext = "WorldContextObject"))
	static void SetMasterVolume(const UObject* WorldContextObject, float Volume);

	/** Applies the saved volume to this world's audio device (the local player controller calls this on BeginPlay) */
	UFUNCTION(BlueprintCallable, Category="Echo|Audio", meta = (WorldContext = "WorldContextObject"))
	static void ApplySavedVolume(const UObject* WorldContextObject);
};
