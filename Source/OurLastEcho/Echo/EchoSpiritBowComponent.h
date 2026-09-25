// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EchoSpiritBowComponent.generated.h"

class AEchoArrow;
class AEchoAnchorPoint;
class AOurLastEchoCharacter;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

/**
 *  Bat's spirit bow. Every AOurLastEchoCharacter has this component, but it only works for a Living-realm
 *  character (Bat): Saraa gets no bow mesh, her input isn't bound and the server rejects her shots.
 *
 *  Aim (hold RMB / left trigger): the camera pulls in over the shoulder, the HUD draws a reticle,
 *  the character faces the aim direction and walks slower.
 *  Fire (LMB / right trigger, while aiming): the SERVER spawns a replicated AEchoArrow on a slight arc
 *  towards what the reticle points at, so both players see it. Unlimited arrows, short cooldown.
 *  Anchors: an arrow that sticks into an anchorable surface (UEchoAnchorRules) becomes an anchor point for
 *  Saraa's whip. This component keeps the list (server) and removes the oldest past MaxActiveAnchors.
 *
 *  All tuning values are EditAnywhere/BlueprintReadWrite: select the component on BP_ThirdPersonCharacter.
 */
UCLASS(ClassGroup=(Echo), meta=(BlueprintSpawnableComponent))
class UEchoSpiritBowComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	// ---- Arrow tuning

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spirit Bow|Arrow")
	TSubclassOf<AEchoArrow> ArrowClass;

	/** Launch speed in cm/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spirit Bow|Arrow", meta = (ClampMin = 100))
	float ArrowSpeed = 4000.0f;

	/** Arc: fraction of world gravity applied to arrows (0 = dead straight). The launch angle is corrected so the arrow still lands on the reticle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spirit Bow|Arrow", meta = (ClampMin = 0))
	float ArrowGravityScale = 0.35f;

	/** How far the reticle reaches, in cm. Targets beyond this aren't aimed at precisely */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spirit Bow|Arrow", meta = (ClampMin = 100))
	float Range = 6000.0f;

	/** Seconds between shots */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spirit Bow|Arrow", meta = (ClampMin = 0))
	float Cooldown = 0.6f;

	/** Where arrows leave from, relative to the character (X forward, Y right, Z up) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spirit Bow|Arrow")
	FVector MuzzleOffset = FVector(70.0f, 15.0f, 45.0f);

	// ---- Anchor arrows

	/** What an arrow becomes when it sticks into an anchorable surface */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spirit Bow|Anchors")
	TSubclassOf<AEchoAnchorPoint> AnchorClass;

	/** How many anchor points Bat can have at once. Making one more removes the oldest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spirit Bow|Anchors", meta = (ClampMin = 1))
	int32 MaxActiveAnchors = 2;

	// ---- Aim tuning

	/** Camera boom length while aiming (normal is the template's 400) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spirit Bow|Aim", meta = (ClampMin = 0))
	float AimCameraDistance = 160.0f;

	/** Over-the-shoulder offset of the camera while aiming */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spirit Bow|Aim")
	FVector AimCameraOffset = FVector(0.0f, 65.0f, 55.0f);

	/** Camera field of view while aiming */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spirit Bow|Aim", meta = (ClampMin = 20, ClampMax = 170))
	float AimFieldOfView = 70.0f;

	/** How fast the camera blends in and out of aim (higher = snappier) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spirit Bow|Aim", meta = (ClampMin = 0.1))
	float AimBlendSpeed = 12.0f;

	/** Walk speed while aiming, in cm/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spirit Bow|Aim", meta = (ClampMin = 0))
	float AimWalkSpeed = 220.0f;

	// ---- Input (Enhanced Input assets made by Scripts/build_spirit_bow.py)

	UPROPERTY(EditAnywhere, Category="Spirit Bow|Input")
	TSoftObjectPtr<UInputAction> AimAction;

	UPROPERTY(EditAnywhere, Category="Spirit Bow|Input")
	TSoftObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditAnywhere, Category="Spirit Bow|Input")
	TSoftObjectPtr<UInputMappingContext> BowMappingContext;

	UPROPERTY(EditAnywhere, Category="Spirit Bow|Input")
	int32 MappingPriority = 1;

	// ---- Look

	UPROPERTY(EditAnywhere, Category="Spirit Bow|Look")
	TSoftObjectPtr<UMaterialInterface> BowMaterial;

	/** Where the bow sits relative to the character (capsule centre) when not aiming: slung across the back */
	UPROPERTY(EditAnywhere, Category="Spirit Bow|Look")
	FTransform BackPlacement = FTransform(FRotator(0.0f, 180.0f, 35.0f), FVector(-24.0f, 0.0f, 38.0f));

	/** ...and while aiming: held upright in front of the chest (the template has no aim animation to raise an arm) */
	UPROPERTY(EditAnywhere, Category="Spirit Bow|Look")
	FTransform AimPlacement = FTransform(FRotator(0.0f, 0.0f, 0.0f), FVector(42.0f, -8.0f, 40.0f));

public:

	UEchoSpiritBowComponent();

	/** True for a Living-realm owner (Bat) */
	UFUNCTION(BlueprintPure, Category="Spirit Bow")
	bool CanUseBow() const;

	UFUNCTION(BlueprintPure, Category="Spirit Bow")
	bool IsAiming() const { return bAiming; }

	/** 0..1, 1 = ready to fire (for the HUD) */
	UFUNCTION(BlueprintPure, Category="Spirit Bow")
	float GetCooldownReadiness() const;

	/** Starts/stops aiming. Call on the owning client (or the listen server's own character) */
	UFUNCTION(BlueprintCallable, Category="Spirit Bow")
	void SetAiming(bool bNewAiming);

	/** Aims through the camera and asks the server to fire. Only works while aiming. Returns false if it didn't fire */
	UFUNCTION(BlueprintCallable, Category="Spirit Bow")
	bool Fire();

	/** Server: fires at a world point from the character's muzzle, ignoring aim state (used by Fire and by tests). Returns the arrow or null */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Spirit Bow")
	AEchoArrow* FireAt(FVector TargetPoint);

	/** Server: turns an arrow hit on an anchorable surface into an anchor point, removing the oldest if there are too many */
	AEchoAnchorPoint* CreateAnchor(const FHitResult& Hit, const FVector& ArrowDirection);

	/** Server: removes all of this bow's anchor points */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Spirit Bow|Anchors")
	void ClearAnchors();

	/** Server: this bow's anchor points, oldest first */
	UFUNCTION(BlueprintPure, Category="Spirit Bow|Anchors")
	TArray<AEchoAnchorPoint*> GetActiveAnchors() const;

	/** Called by the character from SetupPlayerInputComponent */
	void SetupPlayerInput(UEnhancedInputComponent* Input);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bNewAiming);

	UFUNCTION(Server, Reliable)
	void ServerFire(FVector_NetQuantize TargetPoint);

	UFUNCTION()
	void OnRep_Aiming();

	void OnAimStarted();
	void OnAimCompleted();
	void OnFirePressed();

	/** Movement/rotation rules and bow placement for the current bAiming (every machine) */
	void ApplyAimState();

	/** Builds the placeholder bow from engine shapes (every machine, Living owners only) */
	void BuildBowMesh();

	AOurLastEchoCharacter* GetCharacter() const;

	UPROPERTY(ReplicatedUsing = OnRep_Aiming, BlueprintReadOnly, Category="Spirit Bow")
	bool bAiming = false;

	/** World time of the last shot (server for authority, owner for the local check) */
	float LastFireTime = -1000.0f;

	/** Camera and movement values to return to when aiming stops */
	float DefaultArmLength = 400.0f;
	FVector DefaultSocketOffset = FVector::ZeroVector;
	float DefaultFieldOfView = 90.0f;
	float DefaultWalkSpeed = 500.0f;
	bool bDefaultsCaptured = false;

	/** Server: anchor points made by this bow, oldest first */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AEchoAnchorPoint>> ActiveAnchors;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> BowRoot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BowParts;
};
