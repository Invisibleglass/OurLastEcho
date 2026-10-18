// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoEnemies.h"
#include "EchoArenaManager.generated.h"

class UBoxComponent;
class USoundBase;
class AEchoEnemy;

/** Some enemies of one type in a wave */
USTRUCT(BlueprintType)
struct FEchoWaveEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arena")
	EEchoEnemyType Type = EEchoEnemyType::DustWraith;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arena", meta = (ClampMin = 0))
	int32 Count = 1;
};

/** One arena wave */
USTRUCT(BlueprintType)
struct FEchoArenaWave
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arena")
	FText Title;

	/** What the players should learn from it (shown with the wave) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arena")
	FText Hint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arena")
	TArray<FEchoWaveEntry> Enemies;
};

UENUM(BlueprintType)
enum class EEchoArenaState : uint8
{
	/** Waiting for a player to walk in */
	Idle,
	/** Between waves */
	Intermission,
	Fighting,
	Complete
};

/**
 *  The CombatTest arena (Milestone 8): waves of enemies, a restart, and the both-players-down restart.
 *  Placed once in the arena level (by Scripts/build_combat.py), replicated so both HUDs show the wave.
 *  - A player walking into StartZone starts wave 1. Each wave spawns its enemies at the spawn points (flyers at the
 *    air points), all coming straight for the players; when the last is defeated, the next follows after
 *    IntermissionSeconds. After the last wave the arena is complete.
 *  - Waves (default): 1 Dust Wraiths + Shades (each player alone, talking about what they see), 2 Stone Guardians
 *    (Saraa cracks, Bat shoots), 3 Sky Talons (Bat knocks down, Saraa lashes), 4 everything.
 *  - RestartArena (the restart pad by the return door, both players down, or EchoRestartArena): removes the enemies,
 *    sends both players back to the entrance checkpoint with full health, and starts again from wave 1.
 */
UCLASS()
class AEchoArenaManager : public AActor
{
	GENERATED_BODY()

public:

	AEchoArenaManager();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arena")
	TArray<FEchoArenaWave> Waves;

	/** Which class each enemy type spawns as (the Blueprints, so their tuning applies) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arena")
	TMap<EEchoEnemyType, TSubclassOf<AEchoEnemy>> EnemyClasses;

	/** Where ground enemies appear */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arena")
	TArray<TObjectPtr<AActor>> GroundSpawnPoints;

	/** Where Sky Talons appear */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arena")
	TArray<TObjectPtr<AActor>> AirSpawnPoints;

	/** Seconds between waves (and before wave 1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arena", meta = (ClampMin = 0))
	float IntermissionSeconds = 4.0f;

	/** Seconds after a restart before wave 1 starts again */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arena", meta = (ClampMin = 0))
	float RestartDelay = 3.0f;

	/** Played on every machine when a wave starts and when the arena is cleared */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arena|Sound")
	TObjectPtr<USoundBase> WaveStartSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arena|Sound")
	TObjectPtr<USoundBase> ArenaClearedSound;

	/** The arena in this world, if any */
	static AEchoArenaManager* Get(const UWorld* World);

	// ---- Server

	/** Starts from wave 1 (after the intermission) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Arena")
	void StartArena();

	/** Removes the enemies, sends both players to the checkpoint with full health, and starts over */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Arena")
	void RestartArena();

	/** Debug: jumps straight to wave N (1-based) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Arena")
	void SkipToWave(int32 WaveNumber);

	/** Spawns one enemy of a type at a location (debug spawning uses this too). Arena waves track theirs */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Arena")
	AEchoEnemy* SpawnEnemy(EEchoEnemyType Type, FVector Location, bool bCountForWave);

	/** The class an enemy type spawns as */
	TSubclassOf<AEchoEnemy> GetEnemyClass(EEchoEnemyType Type) const;

	/** Server: spawns one enemy anywhere (debug): through the arena if there is one, else its Blueprint class */
	static AEchoEnemy* SpawnEnemyAnywhere(UWorld* World, EEchoEnemyType Type, const FVector& Location);

	/** The Blueprint made for an enemy type (Scripts/build_combat.py), else the C++ class */
	static TSubclassOf<AEchoEnemy> GetDefaultEnemyClass(EEchoEnemyType Type);

	// ---- Everyone

	UFUNCTION(BlueprintPure, Category="Arena")
	EEchoArenaState GetState() const { return State; }

	/** 1-based current wave (0 before the first) */
	UFUNCTION(BlueprintPure, Category="Arena")
	int32 GetWaveNumber() const { return WaveIndex + 1; }

	UFUNCTION(BlueprintPure, Category="Arena")
	int32 GetNumWaves() const { return Waves.Num(); }

	UFUNCTION(BlueprintPure, Category="Arena")
	int32 GetEnemiesRemaining() const { return EnemiesRemaining; }

	/** Times the arena has restarted (for tests) */
	UFUNCTION(BlueprintPure, Category="Arena")
	int32 GetRestartCount() const { return RestartCount; }

	/** The HUD's arena line, e.g. "Wave 2 of 4: Stone Guardians - 2 left" */
	UFUNCTION(BlueprintPure, Category="Arena")
	FText GetStatusText() const;

	/** The current wave's hint (empty between waves) */
	UFUNCTION(BlueprintPure, Category="Arena")
	FText GetHintText() const;

	/** Server: the enemies of the current wave still standing */
	UFUNCTION(BlueprintPure, Category="Arena")
	TArray<AEchoEnemy*> GetLiveEnemies() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnStartZoneOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void BeginWave(int32 Index);
	void HandleEnemyDefeated(AEchoEnemy* Enemy);
	void ClearEnemies();
	void SetState(EEchoArenaState NewState);

	UFUNCTION()
	void OnRep_State();

	/** Walk in here to start the fight */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> StartZone;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	EEchoArenaState State = EEchoArenaState::Idle;

	UPROPERTY(Replicated)
	int32 WaveIndex = -1;

	UPROPERTY(Replicated)
	int32 EnemiesRemaining = 0;

	UPROPERTY(Replicated)
	int32 RestartCount = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AEchoEnemy>> LiveEnemies;

	FTimerHandle WaveTimer;
	int32 SpawnCursor = 0;
};
