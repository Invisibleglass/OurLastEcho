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
 *  The actor's +X axis is the direction the target faces (out of the rock). It's present-day wood, so
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEchoAnchorableComponent> Anchorable;

public:

	/** Diameter of the board in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Anchor Target", meta = (ClampMin = 20))
	float Diameter = 120.0f;

	/** Board thickness in cm (it sticks out of the rock by this much) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Anchor Target", meta = (ClampMin = 2))
	float Thickness = 12.0f;

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
