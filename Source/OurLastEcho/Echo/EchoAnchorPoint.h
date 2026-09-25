// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoAnchorPoint.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USoundBase;

/**
 *  An anchor point: one of Bat's arrows stuck in an anchorable surface (see UEchoAnchorRules).
 *  Spawned by the server (UEchoSpiritBowComponent::CreateAnchor) and replicated, so both players agree
 *  where it is. It never moves. The bow keeps at most MaxActiveAnchors and removes the oldest.
 *
 *  Bat sees his arrow stuck in the target. Saraa sees a glowing blue spirit anchor floating just off the
 *  surface: that's the point her sword whip latches onto (GetSwingPoint). Her whip's targeting highlights
 *  it locally (SetHighlighted), and latching plays a flash and snap on every machine (PlayLatchCue).
 *
 *  The actor sits at the impact point, facing along the arrow's flight. SurfaceNormal (replicated once)
 *  places the spirit anchor.
 */
UCLASS()
class AEchoAnchorPoint : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	/** Bat's view: the stuck arrow */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ArrowShaft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ArrowFletching;

	/** Saraa's view: a glowing orb in a halo ring, at the swing point */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> SpiritOrb;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> SpiritHalo;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> SpiritLight;

public:

	/** How far off the surface the swing point (and Saraa's orb) sits, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anchor")
	float SwingPointOffset = 35.0f;

	UPROPERTY(EditDefaultsOnly, Category="Anchor|Look")
	float OrbDiameter = 32.0f;

	/** Orb size and glow multipliers while Saraa's whip targets this anchor */
	UPROPERTY(EditDefaultsOnly, Category="Anchor|Look")
	float HighlightScale = 1.7f;

	UPROPERTY(EditDefaultsOnly, Category="Anchor|Look")
	float HighlightGlow = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category="Anchor|Look")
	float LightIntensity = 12.0f;

	UPROPERTY(EditDefaultsOnly, Category="Anchor|Look")
	TSoftObjectPtr<UMaterialInterface> ArrowMaterial;

	UPROPERTY(EditDefaultsOnly, Category="Anchor|Look")
	TSoftObjectPtr<UMaterialInterface> SpiritMaterial;

	/** Translucent disc behind the orb */
	UPROPERTY(EditDefaultsOnly, Category="Anchor|Look")
	TSoftObjectPtr<UMaterialInterface> HaloMaterial;

	/** Played where the anchor appears (both players) */
	UPROPERTY(EditDefaultsOnly, Category="Anchor|Sound")
	TSoftObjectPtr<USoundBase> AppearSound;

	/** The whip's snap as it latches on (both players) */
	UPROPERTY(EditDefaultsOnly, Category="Anchor|Sound")
	TSoftObjectPtr<USoundBase> LatchSound;

	AEchoAnchorPoint();

	/** Server: sets which way the surface faces. Call right after spawning */
	void InitSurface(const FVector& Normal);

	/** The point Saraa's whip latches onto: just off the surface, where she sees the spirit anchor */
	UFUNCTION(BlueprintPure, Category="Anchor")
	FVector GetSwingPoint() const;

	UFUNCTION(BlueprintPure, Category="Anchor")
	FVector GetSurfaceNormal() const { return SurfaceNormal; }

	/** Local cosmetic: the whip's targeting highlight on this machine */
	UFUNCTION(BlueprintCallable, Category="Anchor")
	void SetHighlighted(bool bNewHighlighted);

	UFUNCTION(BlueprintPure, Category="Anchor")
	bool IsHighlighted() const { return bHighlighted; }

	/** Local cosmetic: flash and snap when a whip latches on */
	void PlayLatchCue();

	/** Number of latch cues played on THIS machine (lets tests confirm the cue ran everywhere) */
	UFUNCTION(BlueprintPure, Category="Anchor")
	int32 GetLocalLatchCues() const { return LocalLatchCues; }

	/** Is the spirit orb (Saraa's view) / the stuck arrow (Bat's view) shown on this machine? */
	UFUNCTION(BlueprintPure, Category="Anchor")
	bool IsSpiritAnchorVisibleLocally() const;

	UFUNCTION(BlueprintPure, Category="Anchor")
	bool IsArrowVisibleLocally() const;

	/** Server time this anchor was made; the oldest goes first when there are too many */
	UFUNCTION(BlueprintPure, Category="Anchor")
	float GetCreationServerTime() const { return CreationServerTime; }

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_SurfaceNormal();

	/** Places the orb and halo from SurfaceNormal (every machine) */
	void PlaceSpiritAnchor();

	void UpdateLocalVisibility();

	UPROPERTY(ReplicatedUsing = OnRep_SurfaceNormal)
	FVector_NetQuantizeNormal SurfaceNormal = FVector::UpVector;

	UPROPERTY(Replicated)
	float CreationServerTime = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SpiritMID;

	bool bHighlighted = false;
	bool bSpiritVisible = false;
	bool bArrowVisible = false;
	bool bVisibilityKnown = false;
	float RestingGlow = 1.0f;
	float FlashRemaining = 0.0f;
	float Age = 0.0f;
	int32 LocalLatchCues = 0;
};
