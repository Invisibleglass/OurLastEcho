// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoSaplingSpot.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMaterialInterface;

/**
 *  The first cross-time interaction. In the past, a marked spot glows on the ground. When Saraa steps onto it she
 *  plants a sapling there, and in Bat's present, centuries later, a great tree stands on the same spot, with branches
 *  he can climb.
 *
 *  It lives in the SHARED layer, because the tree has collision: bPlanted is replicated, the server sets it, and every
 *  machine turns the tree's collision on at the same time. The tree blocks only ECC_Pawn (Bat); Saraa's past has
 *  just the sapling. What's drawn is chosen locally from the era this machine shows: the marker and the sapling in
 *  the past, the tree in the present.
 *
 *  The actor's origin is on the ground. Scripts/build_eras.py places one below the overlook ledge, so the tree is a
 *  second way up it.
 */
UCLASS()
class AEchoSaplingSpot : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> PlantZone;

	/** The glowing mark on the ground (past, before planting) */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Marker;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> SaplingStem;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> SaplingLeaves;

	/** The present's tree: trunk, crown and branch steps (collision for Bat once planted) */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Trunk;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Crown;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Branches;

public:

	AEchoSaplingSpot();

	/** Height of the grown tree's trunk, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sapling", meta = (ClampMin = 200))
	float TreeHeight = 1100.0f;

	/** Height of the top branch step, in cm above the ground (e.g. the ledge the tree lets Bat reach) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sapling", meta = (ClampMin = 100))
	float TopStepHeight = 900.0f;

	/** Yaw (degrees, relative to the actor) the top step points: towards whatever the climb leads to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sapling")
	float TopStepYaw = 0.0f;

	UPROPERTY(EditAnywhere, Category="Sapling|Look")
	TSoftObjectPtr<UMaterialInterface> MarkerMaterial;

	UPROPERTY(EditAnywhere, Category="Sapling|Look")
	TSoftObjectPtr<UMaterialInterface> BarkMaterial;

	UPROPERTY(EditAnywhere, Category="Sapling|Look")
	TSoftObjectPtr<UMaterialInterface> LeavesMaterial;

	UFUNCTION(BlueprintPure, Category="Sapling")
	bool IsPlanted() const { return bPlanted; }

	/** Server: plant it (Saraa stepping on the mark does this) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Sapling")
	void Plant();

	/** Is the grown tree solid (on this machine)? For tests */
	UFUNCTION(BlueprintPure, Category="Sapling")
	bool IsTreeSolid() const;

	/** Is the grown tree drawn on this machine? For tests */
	UFUNCTION(BlueprintPure, Category="Sapling")
	bool IsTreeVisible() const;

	/** Is the sapling drawn on this machine? For tests */
	UFUNCTION(BlueprintPure, Category="Sapling")
	bool IsSaplingVisible() const;

	/** Top surface centre of the highest branch step / of step Index (0 = lowest) */
	UFUNCTION(BlueprintPure, Category="Sapling")
	FVector GetTopStepLocation() const;

	UFUNCTION(BlueprintPure, Category="Sapling")
	FVector GetStepLocation(int32 Index) const;

	UFUNCTION(BlueprintPure, Category="Sapling")
	int32 GetNumSteps() const;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnPlantZoneOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnRep_Planted();

	/** Collision (every machine, from bPlanted) and what's drawn (this machine's era) */
	void UpdateState();

	UPROPERTY(ReplicatedUsing = OnRep_Planted)
	bool bPlanted = false;

	FTimerHandle LookTimer;
};
