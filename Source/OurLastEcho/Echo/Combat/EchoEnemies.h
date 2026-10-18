// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "EchoEnemy.h"
#include "EchoEnemies.generated.h"

class USoundBase;
class UStaticMeshComponent;

/** Which enemy type (debug spawning, arena waves) */
UENUM(BlueprintType)
enum class EEchoEnemyType : uint8
{
	DustWraith,
	Shade,
	StoneGuardian,
	SkyTalon
};

/**
 *  Dust Wraith: exists only in Bat's world (the present). Only Bat sees it, hits it or is attacked by it;
 *  Saraa can't see or affect it at all. A swirling column of dust that drifts in and claws.
 */
UCLASS(Blueprintable)
class AEchoDustWraith : public AEchoEnemy
{
	GENERATED_BODY()

public:

	AEchoDustWraith(const FObjectInitializer& ObjectInitializer);

protected:

	virtual void BuildBody() override;
	virtual void UpdateLook(float DeltaSeconds) override;
};

/**
 *  Shade: exists only in Saraa's world (the past). Only Saraa sees it, hits it or is attacked by it;
 *  Bat can't see or affect it at all. A dark hooded shape with pale eyes.
 */
UCLASS(Blueprintable)
class AEchoShade : public AEchoEnemy
{
	GENERATED_BODY()

public:

	AEchoShade(const FObjectInitializer& ObjectInitializer);

protected:

	virtual void BuildBody() override;
	virtual void UpdateLook(float DeltaSeconds) override;
};

/**
 *  Stone Guardian (Saraa first): both players see it. A heavy enemy in a stone shell: arrows bounce off (a clink).
 *  When Saraa's whip hits it, the shell cracks open for OpenDuration seconds and Bat's arrows hurt it; then the
 *  shell reforms. Saraa's whip never damages it. Attack: a slow, heavy ground slam.
 */
UCLASS(Blueprintable)
class AEchoStoneGuardian : public AEchoEnemy
{
	GENERATED_BODY()

public:

	AEchoStoneGuardian(const FObjectInitializer& ObjectInitializer);

	/** Seconds the shell stays cracked open after a whip hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Stone Guardian", meta = (ClampMin = 0.1))
	float OpenDuration = 4.0f;

	/** Played on every machine as the shell cracks */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Stone Guardian")
	TObjectPtr<USoundBase> CrackSound;

	/** Played as the shell reforms */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Stone Guardian")
	TObjectPtr<USoundBase> ReformSound;

	virtual bool IsOpen() const override;
	virtual float GetOpenFraction() const override;
	virtual EEchoDamageResult FilterIncomingDamage(FEchoDamageInfo& Info) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BuildBody() override;
	virtual void UpdateLook(float DeltaSeconds) override;
	virtual float GetGlowAmount() const override;

	UFUNCTION()
	void OnRep_OpenUntil();

	/** Server time until which the shell is open (0 = closed) */
	UPROPERTY(ReplicatedUsing = OnRep_OpenUntil)
	float OpenUntil = 0.0f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ShellParts;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Core;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CoreMID;

	bool bWasOpen = false;
};

/**
 *  Sky Talon (Bat first): both players see it. A fast flyer that keeps HoverHeight above the ground, out of
 *  Saraa's reach (her whip only clinks off it). When Bat's arrow hits it, it's knocked to the ground and stunned
 *  for StunDuration seconds, and Saraa's whip hurts it; arrows don't. Then it flies back up.
 *  Attack: it hovers and screeches (the telegraph), then dives at where its target was.
 */
UCLASS(Blueprintable)
class AEchoSkyTalon : public AEchoEnemy
{
	GENERATED_BODY()

public:

	AEchoSkyTalon(const FObjectInitializer& ObjectInitializer);

	/** Cruising height above the ground, in cm (Saraa's whip reaches about 4.5 m) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Sky Talon", meta = (ClampMin = 0))
	float HoverHeight = 650.0f;

	/** Seconds on the ground after an arrow knocks it down */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Sky Talon", meta = (ClampMin = 0.1))
	float StunDuration = 4.0f;

	/** Speed of the dive, in cm/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Sky Talon", meta = (ClampMin = 0))
	float DiveSpeed = 2200.0f;

	/** Played on every machine when it's knocked down */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Sky Talon")
	TObjectPtr<USoundBase> KnockdownSound;

	virtual bool IsStunned() const override;
	virtual bool IsOpen() const override { return IsStunned(); }
	virtual float GetOpenFraction() const override;
	virtual bool ChaseTick(float DeltaTime) override;
	virtual void IdleTick(float DeltaTime) override;
	virtual bool CanStartAttack() const override;
	virtual void StartAttack() override;
	virtual bool AttackTick(float DeltaTime) override;
	virtual void PerformStrike() override;
	virtual bool StunnedTick(float DeltaTime) override;
	virtual EEchoDamageResult FilterIncomingDamage(FEchoDamageInfo& Info) override;
	virtual FVector GetHealthBarLocation() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Height of the ground under it (server, refreshed a few times a second) */
	float GetGroundZ() const { return GroundZ; }

protected:

	virtual void BeginPlay() override;
	virtual void BuildBody() override;
	virtual void UpdateLook(float DeltaSeconds) override;
	virtual float GetGlowAmount() const override;

	/** Steer towards a point in the air (server) */
	void FlyTowards(const FVector& Point, float Scale = 1.0f);
	void RefreshGround();

	UFUNCTION()
	void OnRep_StunnedUntil();

	/** Server time until which it's on the ground stunned (0 = flying) */
	UPROPERTY(ReplicatedUsing = OnRep_StunnedUntil)
	float StunnedUntil = 0.0f;

	FVector DiveTarget = FVector::ZeroVector;
	bool bStruckThisDive = false;
	float GroundZ = 0.0f;
	float GroundTimer = 0.0f;
	bool bWasStunned = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Wings;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> StunRing;
};
