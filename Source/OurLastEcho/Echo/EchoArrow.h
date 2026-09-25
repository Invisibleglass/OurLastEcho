// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoArrow.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UProjectileMovementComponent;
class UMaterialInterface;
class USoundBase;

/**
 *  Glowing spirit-bow arrow. Spawned only by the server (UEchoSpiritBowComponent) and replicated,
 *  so both players see it. Every machine simulates the flight locally from the replicated launch
 *  velocity; only the server decides what the arrow hit: waking an AEchoPlatform, becoming an anchor
 *  point on an anchorable surface (UEchoAnchorRules), or nothing (it stays stuck briefly, then goes).
 *
 *  Collision: its sphere is on the EchoArrow object channel, blocks the world and ignores all pawns,
 *  so it flies through both characters but stops on walls, rocks and echo platform hit boxes.
 */
UCLASS()
class AEchoArrow : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Shaft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Tip;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProjectileMovementComponent> Movement;

	/** Glowing streak segments left behind the arrow (world-space, cosmetic, not replicated) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> TrailSegments;

protected:

	/** Number of trail segments */
	UPROPERTY(EditDefaultsOnly, Category="Arrow|Trail", meta = (ClampMin = 0, ClampMax = 32))
	int32 TrailLength = 12;

	/** Seconds between trail samples */
	UPROPERTY(EditDefaultsOnly, Category="Arrow|Trail", meta = (ClampMin = 0.005))
	float TrailInterval = 0.02f;

	/** Seconds an arrow stays stuck in what it hit before disappearing */
	UPROPERTY(EditDefaultsOnly, Category="Arrow")
	float StuckLifetime = 2.5f;

	/** Seconds a flying arrow lingers after becoming an anchor point (the anchor draws the stuck arrow from then on) */
	UPROPERTY(EditDefaultsOnly, Category="Arrow")
	float AnchoredLifetime = 0.15f;

	/** Seconds before an arrow that hit nothing is removed */
	UPROPERTY(EditDefaultsOnly, Category="Arrow")
	float FlightLifetime = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category="Arrow")
	TSoftObjectPtr<UMaterialInterface> ArrowMaterial;

	UPROPERTY(EditDefaultsOnly, Category="Arrow")
	TSoftObjectPtr<UMaterialInterface> TrailMaterial;

	/** Played on every machine when the arrow appears */
	UPROPERTY(EditDefaultsOnly, Category="Arrow")
	TSoftObjectPtr<USoundBase> ReleaseSound;

	/** Played on every machine when the arrow hits something */
	UPROPERTY(EditDefaultsOnly, Category="Arrow")
	TSoftObjectPtr<USoundBase> ImpactSound;

	/** Launch velocity and gravity, set by the server at spawn and replicated so clients fly the same arc */
	UPROPERTY(ReplicatedUsing = OnRep_Launch)
	FVector_NetQuantize100 LaunchVelocity;

	UPROPERTY(ReplicatedUsing = OnRep_Launch)
	float LaunchGravityScale = 0.0f;

	/** Set by the server when the arrow stops, so late or lagging clients stop it too */
	UPROPERTY(ReplicatedUsing = OnRep_Stuck)
	bool bStuck = false;

	/** Trail ring buffer */
	TArray<FVector> TrailPoints;
	float TrailTimer = 0.0f;

public:

	AEchoArrow();

	/** Server: sets the flight. Call right after spawning */
	void Launch(const FVector& Velocity, float GravityScale);

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Arrow")
	bool IsStuck() const { return bStuck; }

protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnArrowStop(const FHitResult& ImpactResult);

	UFUNCTION()
	void OnRep_Launch();

	UFUNCTION()
	void OnRep_Stuck();

	/** Freezes the arrow where it is (all machines) */
	void Stick();

	void UpdateTrail(float DeltaSeconds);
};
