// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoRisingBridge.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/**
 *  A normal (both-realm) bridge that starts sunk below its placed position and rises when Raise() is called.
 *  Only the raised flag is replicated; every machine plays the rise animation locally so it's smooth for everyone.
 *  In the editor it shows at its raised position (the actor origin is the bridge's top surface centre).
 */
UCLASS()
class AEchoRisingBridge : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BridgeMesh;

protected:

	/** Bridge size in cm (X length, Y width, Z thickness) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bridge")
	FVector BridgeSize = FVector(1600.0f, 300.0f, 30.0f);

	/** Offset from the raised position while lowered */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bridge")
	FVector LoweredOffset = FVector(0.0f, 0.0f, -800.0f);

	/** Seconds the rise takes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bridge", meta = (ClampMin = 0.1))
	float RaiseDuration = 3.0f;

	UPROPERTY(EditAnywhere, Category="Bridge")
	TSoftObjectPtr<UMaterialInterface> BridgeMaterial;

	UPROPERTY(ReplicatedUsing = OnRep_Raised, BlueprintReadOnly, Category="Bridge")
	bool bRaised = false;

	/** Local animation progress, 0 = lowered, 1 = raised */
	float RaiseAlpha = 0.0f;

public:

	AEchoRisingBridge();

	/** Starts raising the bridge for everyone. Server only */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Bridge")
	void Raise();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Raised();

	/** Positions the mesh for the given animation progress */
	void ApplyRaiseAlpha(float Alpha);
};
