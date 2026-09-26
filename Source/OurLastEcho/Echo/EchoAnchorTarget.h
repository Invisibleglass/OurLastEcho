// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoAnchorTarget.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UEchoAnchorableComponent;

/**
 *  Placeholder anchor target: a round, ringed target board for canyon walls, overhangs and rock arches.
 *  When Bat's arrow hits it, the arrow sticks and becomes an anchor point (AEchoAnchorPoint) that Saraa
 *  can latch onto with her sword whip.
 *
 *  The actor's +X axis is the direction the target faces. A block of rock (the mount) sits behind the board and
 *  can reach up (MountReachUp) into an overhang it hangs from. It's present-day wood, so
 *  only Bat sees it (Saraa sees the spirit anchor his arrow makes). The board blocks only the EchoArrow
 *  channel, so arrows and the bow's aim trace hit it but nothing else does.
 *
 *  Whether it can hold an anchor is decided by its UEchoAnchorableComponent via UEchoAnchorRules.
 */
UCLASS()
class AEchoAnchorTarget : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Board;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Ring;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Bullseye;

	/** Rock behind the board (both players see it: it's rock, not wood) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mount;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEchoAnchorableComponent> Anchorable;

public:

	/** Diameter of the board in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Anchor Target", meta = (ClampMin = 20))
	float Diameter = 120.0f;

	/** Board thickness in cm (it sticks out of the rock by this much) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Anchor Target", meta = (ClampMin = 2))
	float Thickness = 12.0f;

	/** A block of rock behind the board, so it looks mounted instead of floating */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Anchor Target|Mount")
	bool bMount = true;

	/** How far the mount reaches up above the board's centre, in cm (along the board's up axis). Set it to reach
	 *  into the rock above a board hanging under an overhang; at 0 the mount is just a slab behind the board */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Anchor Target|Mount", meta = (ClampMin = 0))
	float MountReachUp = 0.0f;

	/** Thickness of the mount behind the board, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Anchor Target|Mount", meta = (ClampMin = 5))
	float MountDepth = 40.0f;

	UPROPERTY(EditAnywhere, Category="Anchor Target|Look")
	TSoftObjectPtr<UMaterialInterface> MountMaterial;

	UPROPERTY(EditAnywhere, Category="Anchor Target|Look")
	TSoftObjectPtr<UMaterialInterface> BoardMaterial;

	UPROPERTY(EditAnywhere, Category="Anchor Target|Look")
	TSoftObjectPtr<UMaterialInterface> RingMaterial;

	AEchoAnchorTarget();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category="Anchor Target")
	bool IsVisibleLocally() const { return bLocallyVisible; }

	/** Centre of the board's front face */
	UFUNCTION(BlueprintPure, Category="Anchor Target")
	FVector GetFaceCenter() const;

protected:

	virtual void BeginPlay() override;

	/** Bat (Living) sees it; Saraa only with the EchoShowAllPlatforms debug view */
	void UpdateLocalVisibility();

	bool bLocallyVisible = true;
};
