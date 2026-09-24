// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoRespawnVolume.generated.h"

class UBoxComponent;

/**
 *  Invisible volume that sends any player character touching it back to where it spawned.
 *  Place it below gaps so falling (e.g. Bat through a spirit platform) isn't a dead end.
 */
UCLASS()
class AEchoRespawnVolume : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Volume;

protected:

	/** Volume size in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Respawn")
	FVector VolumeSize = FVector(10000.0f, 10000.0f, 600.0f);

public:

	AEchoRespawnVolume();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:

	UFUNCTION()
	void OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
