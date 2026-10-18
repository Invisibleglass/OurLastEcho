// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoArenaProps.generated.h"

class UBoxComponent;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class USoundBase;

/**
 *  A glowing door frame with a label (Milestone 8). Either player walking into it takes BOTH players to TargetMap
 *  (AEchoGameMode::TravelBothTo: a seamless travel online, so the session, connection and voice stay up), arriving
 *  at the PlayerStarts tagged ArrivalTag_Bat / ArrivalTag_Saraa. Both players see it and can use it.
 *  The first level's "Combat Test" door goes to CombatTest; the arena's return door comes back to it.
 */
UCLASS()
class AEchoTravelDoor : public AActor
{
	GENERATED_BODY()

public:

	AEchoTravelDoor();

	/** The level to go to, e.g. /Game/Echo/Maps/CombatTest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	FString TargetMap;

	/** Players arrive at the PlayerStarts tagged this + "_Bat" / "_Saraa" there */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	FName ArrivalTag;

	/** Written on the door */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	FText Label;

	/** Seconds after the level starts before the door works (so arriving next to it doesn't send you straight back) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta = (ClampMin = 0))
	float ArmDelay = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	FLinearColor GlowColor = FLinearColor(1.0f, 0.55f, 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	TObjectPtr<USoundBase> OpenSound;

	UPROPERTY(EditAnywhere, Category="Door")
	TSoftObjectPtr<UMaterialInterface> FrameMaterial;

	UPROPERTY(EditAnywhere, Category="Door")
	TSoftObjectPtr<UMaterialInterface> PortalMaterial;

	/** Server: send both players through (what walking in does; also for tests) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Door")
	bool Open(AActor* OpenedBy);

	virtual void OnConstruction(const FTransform& Transform) override;

protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnEnter(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> LeftPost;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> RightPost;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Lintel;

	/** The glowing doorway */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Portal;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> LabelText;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UBoxComponent> Trigger;

	float ArmedAt = 0.0f;
};

/**
 *  "Restart arena" button by the arena's return door (Milestone 8): either player stepping onto it restarts the
 *  arena (AEchoArenaManager::RestartArena). For testing.
 */
UCLASS()
class AEchoArenaRestartPad : public AActor
{
	GENERATED_BODY()

public:

	AEchoArenaRestartPad();

	/** Seconds before it can be used again */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Restart")
	float Cooldown = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Restart")
	TObjectPtr<USoundBase> PressSound;

	UPROPERTY(EditAnywhere, Category="Restart")
	TSoftObjectPtr<UMaterialInterface> FrameMaterial;

	UPROPERTY(EditAnywhere, Category="Restart")
	TSoftObjectPtr<UMaterialInterface> ButtonMaterial;

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Times it has been pressed (server; for tests) */
	UFUNCTION(BlueprintPure, Category="Restart")
	int32 GetPressCount() const { return PressCount; }

protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnEnter(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPressed();

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Button;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> LabelText;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UBoxComponent> Trigger;

	float LastPressed = -1000.0f;
	int32 PressCount = 0;
};
