// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EchoHealthComponent.h"
#include "EchoPlayerCombatComponent.generated.h"

class AOurLastEchoCharacter;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;

/**
 *  The parts of combat both players share (Milestone 8), on every AOurLastEchoCharacter:
 *  - Dodge roll (IA_Dodge: Left Shift / gamepad B): a quick predicted burst of movement (see
 *    UEchoCharacterMovementComponent) with a moment of invulnerability, granted by the server.
 *  - Downed and respawn: at zero health the player goes down (can't move or attack); after RespawnDelay the
 *    server revives them beside the other player. If both are down, AEchoGameMode restarts the arena (or sends
 *    both to their checkpoints outside an arena).
 *  - Hurt reaction: a flinch everyone sees, a camera shake and a red flash for the hurt player (UEchoHUD).
 *  Health itself is the character's UEchoHealthComponent. All tuning is here and on that component: select them
 *  on BP_ThirdPersonCharacter (Bat) and BP_Saraa.
 */
UCLASS(ClassGroup=(Echo), meta=(BlueprintSpawnableComponent))
class UEchoPlayerCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UEchoPlayerCombatComponent();

	// ---- Dodge roll

	/** Ground speed of the roll, in cm/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Dodge", meta = (ClampMin = 0))
	float DodgeSpeed = 1150.0f;

	/** A small hop so the roll leaves the ground briefly (its length comes from speed x airtime), in cm/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Dodge", meta = (ClampMin = 0))
	float DodgeHop = 260.0f;

	/** Seconds after a roll before the next */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Dodge", meta = (ClampMin = 0))
	float DodgeCooldown = 0.75f;

	/** Seconds of invulnerability from the start of a roll */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Dodge", meta = (ClampMin = 0))
	float DodgeInvulnerability = 0.45f;

	/** How long the roll's spin lasts (look only) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Dodge", meta = (ClampMin = 0.05))
	float DodgeLookDuration = 0.4f;

	// ---- Downed and respawn

	/** Seconds down before reviving beside the other player */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Respawn", meta = (ClampMin = 0))
	float RespawnDelay = 3.0f;

	/** How far beside the other player a revived player appears, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Respawn", meta = (ClampMin = 0))
	float PartnerRespawnOffset = 160.0f;

	// ---- Hurt reaction

	/** Camera shake strength when hurt (scaled by Reduce Camera Shake) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Hurt", meta = (ClampMin = 0))
	float HurtShakeScale = 1.5f;

	/** How far the body flinches back, in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Hurt", meta = (ClampMin = 0))
	float FlinchAngle = 18.0f;

	// ---- Input (made by Scripts/build_combat.py)

	UPROPERTY(EditAnywhere, Category="Combat|Input")
	TSoftObjectPtr<UInputAction> DodgeAction;

	UPROPERTY(EditAnywhere, Category="Combat|Input")
	TSoftObjectPtr<UInputMappingContext> CombatMappingContext;

	UPROPERTY(EditAnywhere, Category="Combat|Input")
	int32 MappingPriority = 1;

	// ---- Use

	/** Asks for a roll now (owning client or the host's own character). False if it can't (cooling down, down, mid-air, swinging) */
	UFUNCTION(BlueprintCallable, Category="Combat")
	bool RequestDodge();

	UFUNCTION(BlueprintPure, Category="Combat")
	bool IsDowned() const { return bDowned; }

	/** Seconds until a downed player is revived (0 when not down; every machine, from replicated server time) */
	UFUNCTION(BlueprintPure, Category="Combat")
	float GetRespawnRemaining() const;

	/** Rolls the server has granted (for tests) */
	UFUNCTION(BlueprintPure, Category="Combat")
	int32 GetDodgeCount() const { return DodgeCount; }

	/** World seconds of the last hurt cue on this machine (the HUD's red flash) */
	float GetLastHurtTime() const { return LastHurtTime; }

	/** Called by the movement component when a roll really starts (never for replayed moves) */
	void NotifyDodged();

	bool IsDodgeCoolingDown() const;

	/** May the movement component start a roll now (server's own cooldown check)? */
	bool CanServerDodge() const;

	/** Server: back on their feet with full health (respawn) */
	void ServerRevive();

	/** Called by the character from SetupPlayerInputComponent */
	void SetupPlayerInput(UEnhancedInputComponent* Input);

	UEchoHealthComponent* GetHealth() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleDepleted(UEchoHealthComponent* Health, const FEchoDamageInfo& Info);

	void HandleHitCue(UEchoHealthComponent* Health, const FEchoHitCue& Cue);

	void OnRespawnTimer();

	void OnDodgePressed();

	UFUNCTION()
	void OnRep_DodgeCount();

	UFUNCTION()
	void OnRep_Downed();

	AOurLastEchoCharacter* GetCharacter() const;

	UPROPERTY(ReplicatedUsing = OnRep_Downed, BlueprintReadOnly, Category="Combat")
	bool bDowned = false;

	/** Server world time the player went down (clients compare with the game state's server time) */
	UPROPERTY(Replicated)
	float DownedAt = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_DodgeCount)
	int32 DodgeCount = 0;

	/** Owner: world time of the last roll asked for */
	float LastDodgeTime = -1000.0f;

	/** World time of the last roll really started (the server's cooldown; on the host the same machine asks and grants) */
	float LastGrantedDodgeTime = -1000.0f;

	FTimerHandle RespawnTimer;

	// Look state (every machine)
	FRotator BaseMeshRotation = FRotator::ZeroRotator;
	FVector BaseMeshLocation = FVector::ZeroVector;
	bool bMeshBaseCaptured = false;
	float DodgeLookRemaining = 0.0f;
	float FlinchRemaining = 0.0f;
	float LastHurtTime = -1000.0f;
	float DownedBlend = 0.0f;
};
