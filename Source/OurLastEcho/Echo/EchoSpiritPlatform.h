// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoSpiritPlatform.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/**
 *  A platform that only exists for the Spirit realm (Saraa).
 *  - Collision: blocks only the SpiritPawn channel, so Bat falls straight through.
 *  - Visibility: decided per machine from the locally controlled character's realm,
 *    so each player's screen shows what their character can see. Nothing is replicated.
 */
UCLASS()
class AEchoSpiritPlatform : public AActor
{
	GENERATED_BODY()

	/** The visible, walkable slab */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

protected:

	/** Platform size in cm (X length, Y width, Z thickness). The actor origin is the top surface centre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spirit Platform")
	FVector PlatformSize = FVector(300.0f, 300.0f, 25.0f);

	/** Glowing material. Created by Scripts/build_spirit_path.py */
	UPROPERTY(EditAnywhere, Category="Spirit Platform")
	TSoftObjectPtr<UMaterialInterface> PlatformMaterial;

	/** Current local visibility, cached so we only touch the render state on change */
	bool bLocallyVisible = true;

public:

	AEchoSpiritPlatform();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;

	/** Shows the mesh only if the local player's character is in the Spirit realm */
	void UpdateLocalVisibility();
};
