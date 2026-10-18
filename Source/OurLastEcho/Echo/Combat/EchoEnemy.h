// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EchoCombatTypes.h"
#include "EchoHealthComponent.h"
#include "EchoEnemy.generated.h"

class AOurLastEchoCharacter;
class UCapsuleComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USoundBase;
class UStaticMeshComponent;
class AEchoEnemy;

/** Where an enemy's attack is */
UENUM(BlueprintType)
enum class EEchoAttackPhase : uint8
{
	None,
	/** The telegraph: glowing, swelling, a sound, before anything can hurt */
	WindUp,
	/** The blow itself (the hit check happens as this starts) */
	Strike,
	/** Getting back into shape; open to counter-attacks */
	Recover
};

DECLARE_MULTICAST_DELEGATE_OneParam(FEchoOnEnemyDefeated, AEchoEnemy* /*Enemy*/);

/**
 *  Base for every enemy (Milestone 8): a gameplay actor in the SHARED layer (never in an era level), with
 *  per-player presence like the platforms:
 *  - Presence = Both / LivingOnly (only in Bat's world) / SpiritOnly (only in Saraa's). A player outside its
 *    presence never sees it (visibility is set locally on each machine), can't touch it (collision), can't hurt it
 *    and isn't targeted or attacked by it. Its health bar (UEchoHUD) shows only to players who can see it.
 *  - Health is a UEchoHealthComponent; everything is decided by the server. Clients see the replicated health,
 *    the attack phase (telegraph), hit flashes and the defeat pop.
 *  - Brain: AEchoEnemyController (AI Perception sight, filtered by presence) runs the StateTree ST_EchoEnemy, whose
 *    tasks call this class: ChaseTick, StartAttack/AttackTick, StunnedTick, IdleTick.
 *  - Looks: greybox engine shapes in a flat colour per type (BuildBody), glowing during wind-ups.
 *  Subclasses (EchoEnemies.h) are the four types; tune them in their Blueprints (BP_DustWraith etc.).
 */
UCLASS(Abstract)
class AEchoEnemy : public ACharacter, public IEchoDamageFilter
{
	GENERATED_BODY()

public:

	AEchoEnemy(const FObjectInitializer& ObjectInitializer);

	// ---- Tuning

	/** Which players' worlds it exists in */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy")
	EEchoPresence Presence = EEchoPresence::Both;

	/** Name shown above its health bar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy")
	FText DisplayName;

	/** Walking (or flying) speed while chasing, in cm/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Movement", meta = (ClampMin = 0))
	float MoveSpeed = 350.0f;

	/** Starts an attack when its target is this close (centre to centre), in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Attack", meta = (ClampMin = 0))
	float AttackRange = 220.0f;

	/** Telegraph length before the blow lands, in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Attack", meta = (ClampMin = 0.05))
	float AttackWindUp = 0.8f;

	/** How long the blow lasts (look), in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Attack", meta = (ClampMin = 0.05))
	float AttackStrikeTime = 0.2f;

	/** Recovery after the blow, in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Attack", meta = (ClampMin = 0))
	float AttackRecover = 0.7f;

	/** Seconds between the end of one attack and the start of the next */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Attack", meta = (ClampMin = 0))
	float AttackCooldown = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Attack", meta = (ClampMin = 0))
	float AttackDamage = 15.0f;

	/** The blow hits players within this radius of a point AttackReach in front of it, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Attack", meta = (ClampMin = 0))
	float AttackRadius = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Attack", meta = (ClampMin = 0))
	float AttackReach = 110.0f;

	/** Heavy blows get a bigger hurt reaction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Attack")
	bool bHeavyAttack = false;

	/** AI Perception sight radius (and the radius it notices players in regardless, so a fight never stalls), in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Senses", meta = (ClampMin = 100))
	float SightRadius = 3500.0f;

	/** Arena enemies come at the players straight away (wave fights), without waiting to see them */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Senses")
	bool bAlwaysAware = false;

	// ---- Look

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Look")
	FLinearColor BodyColor = FLinearColor(0.5f, 0.5f, 0.5f);

	/** The telegraph glow */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Look")
	FLinearColor TelegraphColor = FLinearColor(1.0f, 0.25f, 0.05f);

	/** How much it swells during a wind-up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Look", meta = (ClampMin = 0))
	float TelegraphSwell = 0.15f;

	/** Flat-colour material with Color and Glow parameters (Scripts/build_combat.py) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Look")
	TSoftObjectPtr<UMaterialInterface> BodyMaterial;

	// ---- Sound hooks (every machine; empty = silent)

	/** Played as a wind-up starts: the "here it comes" cue */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Sound")
	TObjectPtr<USoundBase> TelegraphSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Sound")
	TObjectPtr<USoundBase> StrikeSound;

	// ---- State

	UFUNCTION(BlueprintPure, Category="Enemy")
	UEchoHealthComponent* GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category="Enemy")
	bool IsDefeated() const { return bDefeated; }

	UFUNCTION(BlueprintPure, Category="Enemy")
	EEchoAttackPhase GetAttackPhase() const { return AttackPhase; }

	UFUNCTION(BlueprintPure, Category="Enemy")
	AOurLastEchoCharacter* GetTarget() const { return Target; }

	/** Stunned (a Sky Talon knocked down): no moving or attacking */
	UFUNCTION(BlueprintPure, Category="Enemy")
	virtual bool IsStunned() const { return false; }

	/** Hit-first enemies: opened up for the second player right now */
	UFUNCTION(BlueprintPure, Category="Enemy")
	virtual bool IsOpen() const { return false; }

	/** 1 = just opened .. 0 = about to close (the fading glow and the HUD's timer) */
	UFUNCTION(BlueprintPure, Category="Enemy")
	virtual float GetOpenFraction() const { return 0.0f; }

	/** Can a player of this realm see, hit and be attacked by this enemy? */
	UFUNCTION(BlueprintPure, Category="Enemy")
	bool IsInWorldOf(EEchoRealm Realm) const { return EchoCombat::PresenceIncludes(Presence, Realm); }

	/** May it pick this player as a target? (in its world, standing, not god-moded out) */
	virtual bool CanTarget(const AOurLastEchoCharacter* Player) const;

	/** Is it shown on this machine (its presence includes the local player, or the show-all debug view)? */
	UFUNCTION(BlueprintPure, Category="Enemy")
	bool IsShownLocally() const { return bShownLocally; }

	/** Where the HUD draws its health bar (above its head) */
	virtual FVector GetHealthBarLocation() const;

	// ---- Brain (called by AEchoEnemyController and the StateTree tasks; server only)

	/** The StateTree state it's in ("Chase", "Attack"...), set by the tasks. Server; for tests and debugging */
	UFUNCTION(BlueprintPure, Category="Enemy")
	FName GetBrainState() const { return BrainState; }

	void SetBrainState(FName State) { BrainState = State; }

	void SetTarget(AOurLastEchoCharacter* NewTarget);

	/** Move towards the target. Returns true once in attack range */
	virtual bool ChaseTick(float DeltaTime);

	/** No target: stand guard (subclasses hover, drift) */
	virtual void IdleTick(float DeltaTime);

	/** In range and off cooldown? */
	virtual bool CanStartAttack() const;

	virtual void StartAttack();

	/** Runs the attack's phases. Returns true when it's over */
	virtual bool AttackTick(float DeltaTime);

	/** Stops an attack early (stunned, defeated, target gone) */
	virtual void CancelAttack();

	/** Stunned: returns true when it's no longer stunned */
	virtual bool StunnedTick(float DeltaTime) { return !IsStunned(); }

	/** Server: the blow lands now. Default: players in its world within AttackRadius of the reach point */
	virtual void PerformStrike();

	/** Server: defeated this many seconds ago or more is removed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy")
	float DefeatRemoveDelay = 0.9f;

	/** Server: removes it without a defeat (arena restarts, tests) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Enemy")
	void RemoveQuietly();

	/** Server: the arena (or the debug command) that spawned it wants to know when it's gone */
	FEchoOnEnemyDefeated OnDefeated;

	/** World time the current attack phase began (server time on every machine) */
	float GetPhaseStartTime() const { return PhaseStartTime; }

	// ---- IEchoDamageFilter

	virtual EEchoDamageResult FilterIncomingDamage(FEchoDamageInfo& Info) override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

	/** Builds the greybox body from engine shapes (every machine). Subclasses add their own parts with AddPart */
	virtual void BuildBody();

	UStaticMeshComponent* AddPart(const TCHAR* MeshPath, const FVector& Location, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator, bool bUsesBodyMaterial = true);

	/** Visibility and collision for the local player (every machine; cheap enough to run every tick) */
	void UpdatePresence();

	/** Glow, swell, hit flash and defeat pop (every machine) */
	virtual void UpdateLook(float DeltaSeconds);

	/** How strongly it glows right now (wind-up, open state...) 0..1 */
	virtual float GetGlowAmount() const;

	/** Sets the replicated attack phase (server) */
	void SetPhase(EEchoAttackPhase NewPhase);

	UFUNCTION()
	void OnRep_AttackPhase();

	UFUNCTION()
	void OnRep_Defeated();

	void HandleHitCue(UEchoHealthComponent* InHealth, const FEchoHitCue& Cue);

	UFUNCTION()
	void HandleDepleted(UEchoHealthComponent* InHealth, const FEchoDamageInfo& Info);

	/** The phase cue on this machine (sounds) */
	void PlayPhaseCue();

	float ServerNow() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UEchoHealthComponent> Health;

	/** Blocks only arrows (only if Bat can affect it), so the bow's aim and arrows can hit it */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UCapsuleComponent> ArrowHitBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> BodyRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BodyParts;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMID;

	UPROPERTY(Replicated)
	TObjectPtr<AOurLastEchoCharacter> Target;

	UPROPERTY(ReplicatedUsing = OnRep_AttackPhase)
	EEchoAttackPhase AttackPhase = EEchoAttackPhase::None;

	UPROPERTY(Replicated)
	float PhaseStartTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Defeated)
	bool bDefeated = false;

	/** Server: world time the last attack ended */
	float LastAttackEndTime = -1000.0f;

	FName BrainState;

	// Look state (every machine)
	float DefeatedAtLocal = -1.0f;
	bool bShownLocally = true;
	bool bLastShown = true;
	FVector BodyBaseScale = FVector::OneVector;
};
