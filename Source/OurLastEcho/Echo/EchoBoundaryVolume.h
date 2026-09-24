// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoBoundaryVolume.generated.h"

class UBoxComponent;

/**
 *  Invisible wall that blocks player characters of BOTH realms and nothing else.
 *  Cameras, traces and physics objects pass through, so it never snags the camera boom.
 *  Used to stop players climbing out of the canyon or walking off its ends.
 */
UCLASS()
class AEchoBoundaryVolume : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Box;

protected:

	/** Box size in cm. The actor origin is the box centre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boundary")
	FVector BoxSize = FVector(1000.0f, 100.0f, 10000.0f);

public:

	AEchoBoundaryVolume();

	virtual void OnConstruction(const FTransform& Transform) override;
};
