// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoPlatform.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USoundBase;

/**
 *  An echo of a platform from Saraa's past that only Bat can perceive, until he shoots it with the spirit bow.
 *
 *  Dormant:  Bat sees a faint, flickering gold outline. Saraa sees nothing. Nobody can stand on it.
 *  Awakened: Saraa sees a blue spirit slab and can stand on it (it blocks only SpiritPawn). Bat still
 *            sees a steady faint outline and still falls through. Both see a flash and hear a cue.
 *
 *  The awake state is server-authoritative and replicated (bAwake); visibility is decided locally on
 *  each machine from the local player's realm, like AEchoSpiritPlatform. A hit box that blocks only the
 *  EchoArrow channel lets arrows hit the platform in either state.
 */
UCLASS()
class AEchoPlatform : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Slab;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> HitBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> FlashLight;

	/** The 12 thin bars that draw the outline */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UStaticMeshComponent>> OutlineEdges;

public:

	/** Platform size in cm (X length, Y width, Z thickness). The actor origin is the top surface centre */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo Platform")
	FVector PlatformSize = FVector(250.0f, 250.0f, 25.0f);

	/** If true, the platform goes dormant again AwakeDuration seconds after being woken. Otherwise it stays awake */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo Platform")
	bool bTimed = false;

	/** Seconds a timed platform stays awake. Shooting it again restarts the timer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo Platform", meta = (EditCondition = "bTimed", ClampMin = 0.5))
	float AwakeDuration = 8.0f;

	/** For timed platforms: Saraa's slab blinks for this many seconds before it fades */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo Platform", meta = (EditCondition = "bTimed", ClampMin = 0.0))
	float WarningDuration = 2.5f;

	/** Seconds the wake-up flash takes to fade */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo Platform|Look")
	float FlashDuration = 1.2f;

	/** Outline glow while dormant (flickering) and while awake (steady, fainter) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo Platform|Look")
	float DormantOutlineGlow = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo Platform|Look")
	float AwakeOutlineGlow = 0.6f;

	/** Outline bar thickness in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo Platform|Look")
	float OutlineThickness = 4.0f;

	UPROPERTY(EditAnywhere, Category="Echo Platform|Look")
	TSoftObjectPtr<UMaterialInterface> SlabMaterial;

	UPROPERTY(EditAnywhere, Category="Echo Platform|Look")
	TSoftObjectPtr<UMaterialInterface> OutlineMaterial;

	UPROPERTY(EditAnywhere, Category="Echo Platform|Sound")
	TSoftObjectPtr<USoundBase> AwakenSound;

	UPROPERTY(EditAnywhere, Category="Echo Platform|Sound")
	TSoftObjectPtr<USoundBase> SleepSound;

protected:

	UPROPERTY(ReplicatedUsing = OnRep_Awake, BlueprintReadOnly, Category="Echo Platform")
	bool bAwake = false;

	/** Bumped by the server on every hit (including re-hits of an awake timed platform), so every machine plays the cue */
	UPROPERTY(ReplicatedUsing = OnRep_AwakenCount)
	int32 AwakenCount = 0;

	/** Server time (AGameStateBase::GetServerWorldTimeSeconds) a timed platform goes dormant; 0 if not timed */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Echo Platform")
	float AwakeUntilServerTime = 0.0f;

	/** Incremented on THIS machine each time the wake-up flash and sound play (lets tests confirm the cue ran everywhere) */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Echo Platform")
	int32 LocalAwakenCues = 0;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SlabMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OutlineMID;

	FTimerHandle SleepTimer;

	/** The slab material's own Glow, which the wake-up flash starts from and settles back to */
	float SlabRestingGlow = 1.5f;
	float FlashRemaining = 0.0f;
	bool bLocalAwakeApplied = false;

public:

	AEchoPlatform();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: wakes the platform (or restarts a timed platform's timer). Called when an arrow hits it */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Echo Platform")
	void Awaken();

	/** Server: puts the platform back to sleep */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Echo Platform")
	void Sleep();

	UFUNCTION(BlueprintPure, Category="Echo Platform")
	bool IsAwake() const { return bAwake; }

	/** Seconds left before a timed platform sleeps (0 if not timed or dormant) */
	UFUNCTION(BlueprintPure, Category="Echo Platform")
	float GetRemainingAwakeTime() const;

	UFUNCTION(BlueprintPure, Category="Echo Platform")
	bool IsSlabVisibleLocally() const;

	UFUNCTION(BlueprintPure, Category="Echo Platform")
	bool IsOutlineVisibleLocally() const;

protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Awake();

	UFUNCTION()
	void OnRep_AwakenCount();

	/** Collision and sleep cue for the current bAwake (runs on every machine) */
	void ApplyAwakeState();

	/** Flash + sound (runs on every machine) */
	void PlayAwakenCue();

	/** Shows/hides the slab and outline for the local player's realm */
	void UpdateLocalVisibility();
};
