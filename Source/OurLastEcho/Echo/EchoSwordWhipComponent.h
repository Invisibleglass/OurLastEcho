// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EchoSwordWhipComponent.generated.h"

class AEchoAnchorPoint;
class AOurLastEchoCharacter;
class UEchoCharacterMovementComponent;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class UMaterialInterface;
class USceneComponent;
class USoundBase;
class UStaticMeshComponent;

/**
 *  Saraa's sword whip. Every AOurLastEchoCharacter has this component, but it only works for a Spirit-realm
 *  character (Saraa): Bat gets no whip mesh, his input isn't bound and his movement refuses to swing.
 *
 *  Targeting (owning player, every frame): the nearest anchor point (Bat's stuck arrows, AEchoAnchorPoint)
 *  within WhipRange and within TargetingAngle of the camera's view, with a clear line of sight, is
 *  highlighted on her screen.
 *  Latch (press the whip button: LMB / E / right trigger): the whip lashes out to the highlighted anchor
 *  with a snap, and she swings under it like a pendulum (UEchoCharacterMovementComponent's swing mode).
 *  Release (let go of the whip button, or press jump): she launches forward with her momentum plus a
 *  small boost, and can latch onto another anchor mid-air to chain swings.
 *  Lash (the whip button with no anchor targeted): the whip cracks forward; the server sweeps along it and a
 *  training dummy it meets takes a hit (AEchoTrainingDummy). Groundwork for combat.
 *
 *  The swing itself is predicted movement (see UEchoCharacterMovementComponent). This component handles
 *  input, targeting, the look (a placeholder sword on her hip, and the whip line while swinging) and cues.
 *  All swing tuning is here: select the SwordWhip component on BP_Saraa.
 */
UCLASS(ClassGroup=(Echo), meta=(BlueprintSpawnableComponent))
class UEchoSwordWhipComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	// ---- Targeting

	/** How far the whip reaches, in cm (character centre to anchor). Also the longest possible rope */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sword Whip|Targeting", meta = (ClampMin = 100))
	float WhipRange = 1800.0f;

	/** Anchors further than this many degrees from the centre of the camera's view aren't targeted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sword Whip|Targeting", meta = (ClampMin = 1, ClampMax = 180))
	float TargetingAngle = 40.0f;

	/** After letting go, the anchor she just left can't be re-targeted for this long (so chaining picks the next one) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sword Whip|Targeting", meta = (ClampMin = 0))
	float RelatchDelay = 0.5f;

	// ---- Swing

	/** Swing speed cap in cm/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sword Whip|Swing", meta = (ClampMin = 100))
	float MaxSwingSpeed = 1600.0f;

	/** Swing speed: a push along the swing (across the whip, towards the anchor's side) when the whip latches, in cm/s,
	 *  so even a slow latch swings. It never slackens the whip */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sword Whip|Swing", meta = (ClampMin = 0))
	float LatchBoost = 250.0f;

	/** Fraction of world gravity while swinging. Higher = faster, snappier swings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sword Whip|Swing", meta = (ClampMin = 0))
	float SwingGravityScale = 1.4f;

	/** Steering acceleration from the move input while swinging, in cm/s^2 (across the rope) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sword Whip|Swing", meta = (ClampMin = 0))
	float SwingAirControl = 300.0f;

	/** Upward speed when latching while standing on the ground, so she leaves it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sword Whip|Swing", meta = (ClampMin = 0))
	float GroundLatchHop = 400.0f;

	/** Extra speed along the swing when letting go, in cm/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sword Whip|Release", meta = (ClampMin = 0))
	float ReleaseBoost = 250.0f;

	/** Extra upward speed when letting go, in cm/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sword Whip|Release", meta = (ClampMin = 0))
	float ReleaseUpBoost = 250.0f;

	// ---- Input (Enhanced Input assets made by Scripts/build_sword_whip.py)

	UPROPERTY(EditAnywhere, Category="Sword Whip|Input")
	TSoftObjectPtr<UInputAction> WhipAction;

	UPROPERTY(EditAnywhere, Category="Sword Whip|Input")
	TSoftObjectPtr<UInputMappingContext> WhipMappingContext;

	UPROPERTY(EditAnywhere, Category="Sword Whip|Input")
	int32 MappingPriority = 1;

	// ---- Look and sound

	/** Whip line thickness in cm */
	UPROPERTY(EditAnywhere, Category="Sword Whip|Look", meta = (ClampMin = 0.5))
	float WhipLineThickness = 3.0f;

	UPROPERTY(EditAnywhere, Category="Sword Whip|Look")
	TSoftObjectPtr<UMaterialInterface> SwordMaterial;

	UPROPERTY(EditAnywhere, Category="Sword Whip|Look")
	TSoftObjectPtr<UMaterialInterface> WhipLineMaterial;

	/** Where the sword hangs relative to the character (capsule centre) when not swinging: at the left hip */
	UPROPERTY(EditAnywhere, Category="Sword Whip|Look")
	FTransform HolsterPlacement = FTransform(FRotator(-150.0f, 0.0f, 0.0f), FVector(8.0f, -26.0f, 0.0f));

	/** Where the sword is held while swinging; it points at the anchor from here */
	UPROPERTY(EditAnywhere, Category="Sword Whip|Look")
	FVector HeldLocation = FVector(10.0f, 22.0f, 70.0f);

	UPROPERTY(EditAnywhere, Category="Sword Whip|Sound")
	TSoftObjectPtr<USoundBase> ReleaseSound;
	/** Played with the whip lash (the whip button with no anchor to latch onto) */
	UPROPERTY(EditAnywhere, Category="Sword Whip|Sound")
	TSoftObjectPtr<USoundBase> LashSound;

	// ---- Lash (groundwork for combat): the whip button with no anchor targeted lashes forward

	/** How far the lash reaches, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sword Whip|Lash", meta = (ClampMin = 50))
	float LashRange = 450.0f;

	/** Thickness of the lash's hit sweep, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sword Whip|Lash", meta = (ClampMin = 1))
	float LashRadius = 35.0f;

	/** Seconds the lash takes to crack out and back */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sword Whip|Lash", meta = (ClampMin = 0.05))
	float LashDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sword Whip|Lash", meta = (ClampMin = 0))
	float LashCooldown = 0.6f;

public:

	UEchoSwordWhipComponent();

	/** True for a Spirit-realm owner (Saraa) */
	UFUNCTION(BlueprintPure, Category="Sword Whip")
	bool CanUseWhip() const;

	/** The anchor the whip would latch onto right now (owning player only) */
	UFUNCTION(BlueprintPure, Category="Sword Whip")
	AEchoAnchorPoint* GetHighlightedAnchor() const { return HighlightedAnchor.Get(); }

	/** Searches for the best anchor now: nearest in range, in front of the camera, in sight */
	UFUNCTION(BlueprintPure, Category="Sword Whip")
	AEchoAnchorPoint* FindBestAnchor() const;

	UFUNCTION(BlueprintPure, Category="Sword Whip")
	bool IsSwinging() const;

	/** Is the whip line drawn on this machine (for tests) */
	UFUNCTION(BlueprintPure, Category="Sword Whip")
	bool IsWhipLineVisible() const;

	/** Whip button pressed: latch onto the highlighted anchor. Call on the owning client (or the host's own character). False if there was nothing to latch onto */
	UFUNCTION(BlueprintCallable, Category="Sword Whip")
	bool PressWhip();

	/** Whip button released: let go */
	UFUNCTION(BlueprintCallable, Category="Sword Whip")
	void ReleaseWhip();

	/** Lashes the whip forward (where the camera looks). Call on the owning client (or the host's own character). False while cooling down */
	UFUNCTION(BlueprintCallable, Category="Sword Whip|Lash")
	bool Lash();

	/** Lashes in a given direction (Lash uses the camera's). On the server it also does the hit */
	UFUNCTION(BlueprintCallable, Category="Sword Whip|Lash")
	bool LashInDirection(FVector Direction);

	/** Lashes the server has done for this character (for tests) */
	UFUNCTION(BlueprintPure, Category="Sword Whip|Lash")
	int32 GetLashCount() const { return LashCount; }

	/** Called by the character from SetupPlayerInputComponent */
	void SetupPlayerInput(UEnhancedInputComponent* Input);

	/** Called by the movement component when a swing really starts / ends (never for replayed moves) */
	void NotifySwingStarted(AEchoAnchorPoint* Anchor);
	void NotifySwingEnded(bool bLaunched);

	/** The EchoWhipDebug console command: draws whip range, targeting cone, anchor targets, anchors and swing arcs on this machine */
	static void ToggleDebugDraw();

	UFUNCTION(BlueprintPure, Category="Sword Whip|Debug")
	static bool IsDebugDrawOn() { return bDebugDraw; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	void OnWhipPressed();
	void OnWhipReleased();

	UFUNCTION()
	void OnRep_LatchCount();

	AOurLastEchoCharacter* GetCharacter() const;
	UEchoCharacterMovementComponent* GetMovement() const;

	/** The player's view (camera), for targeting */
	void GetView(FVector& OutLocation, FVector& OutDirection) const;

	void UpdateTargeting();

	/** Whip line and sword placement from the swing state (every machine) */
	void UpdateWhipLook(float DeltaTime);

	void BuildWhipMesh();

	void DrawDebug();

	/** Where the swing would go from here with no input, ignoring collisions: for the debug swing arc */
	void PredictSwingPath(FVector Location, FVector Velocity, const FVector& Anchor, float Rope, float Duration, TArray<FVector>& OutPoints) const;

	/** Replicated to the other player's machine (the owner and server know from movement) */
	UPROPERTY(Replicated)
	bool bLatched = false;

	UPROPERTY(Replicated)
	FVector_NetQuantize10 LatchedAnchor;

	/** Bumped by the server on every latch, so the other player's machine plays the snap too */
	UPROPERTY(ReplicatedUsing = OnRep_LatchCount)
	int32 LatchCount = 0;

	UFUNCTION(Server, Reliable)
	void ServerLash(FVector_NetQuantizeNormal Direction);

	/** Server: counts the lash, replicates it for the look, and sweeps for things to hit */
	void DoLash(const FVector& Direction);

	/** The lash's look on this machine */
	void StartLashLook(const FVector& Direction);

	UFUNCTION()
	void OnRep_LashCount();

	UPROPERTY(ReplicatedUsing = OnRep_LashCount)
	int32 LashCount = 0;

	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal LashDirection;

	float LashRemaining = 0.0f;
	FVector LashLookDirection = FVector::ForwardVector;
	float LastLashTime = -1000.0f;

	TWeakObjectPtr<AEchoAnchorPoint> HighlightedAnchor;
	TWeakObjectPtr<AEchoAnchorPoint> LastReleasedAnchor;
	float LastReleaseTime = -1000.0f;

	bool bLineShown = false;

	/** Recent positions while swinging (debug arc) */
	TArray<FVector> SwingTrail;

	static bool bDebugDraw;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> SwordRoot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SwordParts;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WhipLine;
};
