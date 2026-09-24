// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoCheckpoint.generated.h"

class UBoxComponent;

/**
 *  Invisible volume: any player character that enters it will respawn here from then on
 *  (kill volumes call AOurLastEchoCharacter::RespawnAtStart). Server-side only; nothing to replicate.
 */
UCLASS()
class AEchoCheckpoint : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Volume;

protected:

	/** Trigger size in cm. The actor origin is the floor centre of the box */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Checkpoint")
	FVector VolumeSize = FVector(1000.0f, 1000.0f, 400.0f);

	/** Respawn point relative to the actor (capsule centre, so keep Z above the floor). Facing = the actor's rotation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Checkpoint", meta = (MakeEditWidget = true))
	FVector RespawnPoint = FVector(0.0f, 0.0f, 100.0f);

public:

	AEchoCheckpoint();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** World transform characters respawn at */
	UFUNCTION(BlueprintPure, Category="Checkpoint")
	FTransform GetRespawnTransform() const;

protected:

	UFUNCTION()
	void OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
