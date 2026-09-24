// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoEndZone.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInterface;

/**
 *  Shared goal area. When a Living character and a Spirit character are inside at the same time,
 *  the server marks the milestone complete on the game state (which replicates to everyone).
 *  The actor origin is the floor level at the zone's centre.
 */
UCLASS()
class AEchoEndZone : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Zone;

	/** Thin glowing floor marker so players can see the zone */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Marker;

protected:

	/** Zone size in cm (X length, Y width, Z height) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="End Zone")
	FVector ZoneSize = FVector(600.0f, 800.0f, 300.0f);

	UPROPERTY(EditAnywhere, Category="End Zone")
	TSoftObjectPtr<UMaterialInterface> MarkerMaterial;

public:

	AEchoEndZone();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:

	UFUNCTION()
	void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Server: completes the milestone if both realms are present */
	void EvaluateOccupants();
};
