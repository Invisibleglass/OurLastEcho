// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoTypes.h"
#include "EchoEraDirector.generated.h"

class ULevelStreamingDynamic;
class UMaterialParameterCollection;
class APlayerController;

/**
 *  Loads the local player's era of the canyon on THIS machine only.
 *
 *  The gameplay level is the shared layer: every walkable shape, all collision, and all gameplay actors, on every
 *  machine. Each era's look (visible canyon rock, props, water, lighting, sky, fog, post-processing, ambience) is its
 *  own level with no collision: PresentLevel for Bat, PastLevel for Saraa. The director loads the one that matches
 *  the local player's character with ULevelStreamingDynamic::LoadLevelInstance, which is local: nothing about it is
 *  replicated, and the other machine never loads it.
 *
 *  While an era is loading the screen is held black, then fades in, so nobody sees the bare shared layer. It keeps
 *  the era through respawns (the role doesn't change) and the level instance goes with the world when the player
 *  returns to the title screen. It also sets the "Past" value of the era material parameter collection on this
 *  machine (0 = present, 1 = past), which tints what stays visible in the shared layer (the canyon floor, rock mounts).
 *
 *  Debug (console, see AOurLastEchoPlayerController): EchoSwapEra previews the other era locally, EchoEra
 *  present|past|auto picks one, and EchoShowBothEras loads both at once to check that they line up.
 *
 *  Placed once in the gameplay level (Scripts/build_eras.py does it). Ticks even while the game is paused.
 */
UCLASS()
class AEchoEraDirector : public AActor
{
	GENERATED_BODY()

public:

	AEchoEraDirector();

	/** Bat's era: the dry present-day canyon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Echo|Eras")
	TSoftObjectPtr<UWorld> PresentLevel;

	/** Saraa's era: the lush ancient canyon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Echo|Eras")
	TSoftObjectPtr<UWorld> PastLevel;

	/** Material parameter collection with a "Past" scalar, set on this machine to the era it shows */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Echo|Eras")
	TSoftObjectPtr<UMaterialParameterCollection> EraCollection;

	/** Seconds the screen takes to fade in once the era has loaded */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo|Eras", meta = (ClampMin = 0))
	float FadeInTime = 0.6f;

	/** The director in this world, if the level has one */
	static AEchoEraDirector* Get(const UObject* WorldContextObject);

	/** Debug: show this era locally instead of the local player's own (bOverride false = back to the player's own) */
	UFUNCTION(BlueprintCallable, Category="Echo|Eras|Debug")
	void SetDebugEra(bool bOverride, EEchoEra Era);

	/** Debug: switch the local view to the other era */
	UFUNCTION(BlueprintCallable, Category="Echo|Eras|Debug")
	void SwapEra();

	/** Debug: load both eras at once (to check that they line up) */
	UFUNCTION(BlueprintCallable, Category="Echo|Eras|Debug")
	void SetShowBothEras(bool bShowBoth);

	UFUNCTION(BlueprintPure, Category="Echo|Eras|Debug")
	bool IsShowingBothEras() const { return bShowBoth; }

	/** The era this machine shows (false until the local player's role is known) */
	UFUNCTION(BlueprintPure, Category="Echo|Eras")
	bool GetShownEra(EEchoEra& OutEra) const;

	/** Is this era's level loaded and visible here? */
	UFUNCTION(BlueprintPure, Category="Echo|Eras")
	bool IsEraVisible(EEchoEra Era) const;

	/** Is this era's level loaded here at all (visible or not)? */
	UFUNCTION(BlueprintPure, Category="Echo|Eras")
	bool IsEraLoaded(EEchoEra Era) const;

	/** True once everything wanted is visible and the fade-in has started */
	UFUNCTION(BlueprintPure, Category="Echo|Eras")
	bool IsReady() const { return bReady; }

	/** Actors in this era's loaded level (for tests: an era layer must have no collision) */
	UFUNCTION(BlueprintCallable, Category="Echo|Eras")
	TArray<AActor*> GetEraActors(EEchoEra Era) const;

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The era the local player's character belongs to, remembered through respawns (no pawn for a moment) */
	bool ResolveRoleEra(const APlayerController* PC);

	void LoadEra(EEchoEra Era);
	void UnloadEra(EEchoEra Era);
	void HoldBlack(APlayerController* PC);
	void ApplyEraParameter(bool bWantPresent, bool bWantPast);

	TSoftObjectPtr<UWorld>& LevelFor(EEchoEra Era) { return Era == EEchoEra::Present ? PresentLevel : PastLevel; }

	UPROPERTY(Transient)
	TObjectPtr<ULevelStreamingDynamic> Streaming[2];

	TOptional<EEchoEra> RoleEra;
	TOptional<EEchoEra> DebugEra;
	bool bShowBoth = false;
	bool bHeldBlack = false;
	bool bReady = false;
};
