// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoTrainingDummy.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class USoundBase;

/**
 *  Greybox training dummy for Saraa's whip lash (Milestone 4 stretch goal, groundwork for combat).
 *  A post with arms and a head from engine shapes. The server counts hits (ReceiveLash) and replicates the
 *  count, so both players see it wobble and hear the hit. Solid for both realms.
 */
UCLASS()
class AEchoTrainingDummy : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	/** Everything above the base tilts when hit */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Post;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Arms;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Head;

public:

	/** How far it tips over when hit, in degrees, and how long the wobble lasts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Training Dummy")
	float WobbleAngle = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Training Dummy")
	float WobbleDuration = 0.8f;

	UPROPERTY(EditAnywhere, Category="Training Dummy")
	TSoftObjectPtr<UMaterialInterface> DummyMaterial;

	UPROPERTY(EditAnywhere, Category="Training Dummy")
	TSoftObjectPtr<USoundBase> HitSound;

	AEchoTrainingDummy();

	/** Server: a whip lash hit it, coming from Direction */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Training Dummy")
	void ReceiveLash(AActor* Attacker, FVector Direction);

	UFUNCTION(BlueprintPure, Category="Training Dummy")
	int32 GetHitCount() const { return HitCount; }

	/** Hit reactions played on THIS machine (lets tests confirm both players saw it) */
	UFUNCTION(BlueprintPure, Category="Training Dummy")
	int32 GetLocalHitReactions() const { return LocalHitReactions; }

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_HitCount();

	/** Wobble and sound (every machine) */
	void PlayHitReaction();

	UPROPERTY(ReplicatedUsing = OnRep_HitCount, BlueprintReadOnly, Category="Training Dummy")
	int32 HitCount = 0;

	/** Horizontal direction of the last hit, for the wobble */
	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal LastHitDirection = FVector::ForwardVector;

	float WobbleRemaining = 0.0f;
	int32 LocalHitReactions = 0;
};
