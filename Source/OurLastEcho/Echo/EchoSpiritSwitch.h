// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoTypes.h"
#include "EchoSpiritSwitch.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class AEchoRisingBridge;

/**
 *  A floor pad that a character of RequiredRealm (Saraa by default) activates by stepping on it.
 *  One-shot: once pressed it stays pressed and raises TargetBridge. Visible to both players.
 */
UCLASS()
class AEchoSpiritSwitch : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Trigger;

protected:

	/** Bridge raised when the switch is pressed */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Switch")
	TObjectPtr<AEchoRisingBridge> TargetBridge;

	/** Only characters of this realm can press the switch */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Switch")
	EEchoRealm RequiredRealm = EEchoRealm::Spirit;

	/** How far the pad sinks when pressed, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Switch")
	float PressedDepth = 8.0f;

	UPROPERTY(EditAnywhere, Category="Switch")
	TSoftObjectPtr<UMaterialInterface> PadMaterial;

	UPROPERTY(ReplicatedUsing = OnRep_Activated, BlueprintReadOnly, Category="Switch")
	bool bActivated = false;

public:

	AEchoSpiritSwitch();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnRep_Activated();

	/** Places the pad at its up or pressed height */
	void UpdatePadPosition();
};
