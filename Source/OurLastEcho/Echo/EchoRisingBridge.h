// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoRisingBridge.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/**
 *  A normal (both-realm) bridge that starts out of place and moves into its placed position when Raise() is called:
 *  - Milestone 1's bridge rises from below (LoweredOffset).
 *  - With bHingeAtStart and StartRotationOffset it's a drawbridge-style ramp that swings down about its start edge
 *    (The Climb's ramp for Bat).
 *  Only the raised flag is replicated; every machine plays the animation locally so it's smooth for everyone.
 *  In the editor it shows at its final position. The actor origin is the top surface centre, or with
 *  bHingeAtStart, the centre of the hinge edge (the bridge extends along +X from there).
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

	/** If true the actor origin is the -X edge (the hinge) instead of the centre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bridge")
	bool bHingeAtStart = false;

	/** Rotation about the origin before Raise(); it swings from this to its placed rotation (e.g. Pitch 100 = stowed upright) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bridge")
	FRotator StartRotationOffset = FRotator::ZeroRotator;

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

	UFUNCTION(BlueprintPure, Category="Bridge")
	bool IsRaised() const { return bRaised; }

	/** 0 = start pose, 1 = in place (local animation progress) */
	UFUNCTION(BlueprintPure, Category="Bridge")
	float GetRaiseProgress() const { return RaiseAlpha; }

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
