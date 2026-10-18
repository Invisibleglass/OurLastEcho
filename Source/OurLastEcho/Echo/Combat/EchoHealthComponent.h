// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EchoCombatTypes.h"
#include "EchoHealthComponent.generated.h"

class UEchoHealthComponent;
class USoundBase;

/** The last hit, replicated to every machine for its cue (flash, clink, hit-stop, sound) */
USTRUCT(BlueprintType)
struct FEchoHitCue
{
	GENERATED_BODY()

	/** Bumped on every hit, so a repeat of the same hit still replicates */
	UPROPERTY(BlueprintReadOnly, Category="Echo|Combat")
	uint8 Counter = 0;

	UPROPERTY(BlueprintReadOnly, Category="Echo|Combat")
	EEchoDamageResult Result = EEchoDamageResult::Applied;

	UPROPERTY(BlueprintReadOnly, Category="Echo|Combat")
	EEchoAttackKind Kind = EEchoAttackKind::Debug;

	UPROPERTY(BlueprintReadOnly, Category="Echo|Combat")
	bool bHeavy = false;

	UPROPERTY(BlueprintReadOnly, Category="Echo|Combat")
	FVector_NetQuantize Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Echo|Combat")
	TObjectPtr<AActor> Instigator = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEchoOnDamaged, UEchoHealthComponent*, Health, const FEchoDamageInfo&, Info, EEchoDamageResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEchoOnDepleted, UEchoHealthComponent*, Health, const FEchoDamageInfo&, Info);
DECLARE_MULTICAST_DELEGATE_TwoParams(FEchoOnHitCue, UEchoHealthComponent* /*Health*/, const FEchoHitCue& /*Cue*/);

/**
 *  Health for anything that can be hurt: both players, every enemy, and later bosses (Milestone 8).
 *
 *  The server decides everything: ApplyDamage asks the owner's IEchoDamageFilter (if any) what the hit does,
 *  checks invulnerability (a dodge roll's grace, god mode) and team, then changes Health. Health and the last hit
 *  replicate; every machine plays the hit's cue from that (OnHitCue: flash, clink, hit-stop, sound hooks).
 *  Clients never change health; they only ask for attacks, which the server turns into ApplyDamage calls.
 */
UCLASS(ClassGroup=(Echo), meta=(BlueprintSpawnableComponent))
class UEchoHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UEchoHealthComponent();

	/** Full health */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health", meta = (ClampMin = 1))
	float MaxHealth = 100.0f;

	/** Players or enemies: hits from your own side never count */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health")
	EEchoTeam Team = EEchoTeam::Enemies;

	// ---- Sound hooks (played on every machine with the hit's cue; empty = silent)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health|Sound")
	TObjectPtr<USoundBase> HitSound;

	/** "Did nothing": a clink */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health|Sound")
	TObjectPtr<USoundBase> BlockedSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health|Sound")
	TObjectPtr<USoundBase> DefeatSound;

	// ---- Server API

	/** Server: one hit. Returns what it did (also replicated as the hit cue) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Health")
	EEchoDamageResult ApplyDamage(const FEchoDamageInfo& Info);

	/** Server: back to full health and no longer depleted */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Health")
	void Revive();

	/** Server: hits do nothing (Immune) for this long, e.g. a dodge roll's grace */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Health")
	void SetInvulnerableFor(float Seconds);

	/** Server: god mode (debug): never takes damage */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Health")
	void SetGodMode(bool bOn);

	/** Server: replicates a hit cue without changing health (e.g. a hit that opened a shell) */
	void SendCue(const FEchoDamageInfo& Info, EEchoDamageResult Result);

	// ---- Queries (every machine)

	UFUNCTION(BlueprintPure, Category="Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category="Health")
	float GetHealthFraction() const { return MaxHealth > 0.0f ? FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f) : 0.0f; }

	/** Health reached zero (a player is down, an enemy defeated) */
	UFUNCTION(BlueprintPure, Category="Health")
	bool IsDepleted() const { return bDepleted; }

	UFUNCTION(BlueprintPure, Category="Health")
	bool IsGodMode() const { return bGodMode; }

	/** Server: invulnerable right now */
	UFUNCTION(BlueprintPure, Category="Health")
	bool IsInvulnerable() const;

	UFUNCTION(BlueprintPure, Category="Health")
	FEchoHitCue GetLastHitCue() const { return LastHitCue; }

	/** Hits that reached this component, by result (server; for tests) */
	UFUNCTION(BlueprintPure, Category="Health")
	int32 GetHitCount(EEchoDamageResult Result) const;

	/** World seconds of the last cue on this machine (for flashes) */
	float GetLastCueTime() const { return LastCueTime; }

	/** Server: any hit, with what it did */
	UPROPERTY(BlueprintAssignable, Category="Health")
	FEchoOnDamaged OnDamaged;

	/** Server: health reached zero */
	UPROPERTY(BlueprintAssignable, Category="Health")
	FEchoOnDepleted OnDepleted;

	/** Every machine: a hit cue to show */
	FEchoOnHitCue OnHitCue;

	/** Find the health component on an actor (null if it has none) */
	static UEchoHealthComponent* FindOn(const AActor* Actor);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_HitCue();

	/** The cue on this machine: sounds, a heavy hit's hit-stop, OnHitCue for owner visuals */
	void PlayCue();

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Health")
	float Health = 100.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Health")
	bool bDepleted = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Health")
	bool bGodMode = false;

	UPROPERTY(ReplicatedUsing = OnRep_HitCue)
	FEchoHitCue LastHitCue;

	/** Server world time until which hits do nothing */
	float InvulnerableUntil = -1.0f;

	float LastCueTime = -1000.0f;

	TMap<EEchoDamageResult, int32> HitCounts;
};
