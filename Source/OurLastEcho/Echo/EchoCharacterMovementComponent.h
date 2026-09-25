// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/CharacterMovementReplication.h"
#include "EchoCharacterMovementComponent.generated.h"

class AEchoAnchorPoint;
class UEchoSwordWhipComponent;

/** Our custom movement modes (MOVE_Custom sub-modes) */
UENUM(BlueprintType)
enum class EEchoCustomMovement : uint8
{
	None = 0,
	/** Hanging from an anchor point on Saraa's sword whip */
	Swing = 1
};

/**
 *  Client-to-server move data with the whip's swing request added, so the server replays exactly the
 *  latch the client predicted: which anchor point (by location) and how long the rope is.
 */
struct FEchoNetworkMoveData : public FCharacterNetworkMoveData
{
	FVector_NetQuantize10 SwingAnchor = FVector::ZeroVector;
	float SwingRopeLength = 0.0f;

	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;
};

struct FEchoNetworkMoveDataContainer : public FCharacterNetworkMoveDataContainer
{
	FEchoNetworkMoveDataContainer();

	FEchoNetworkMoveData EchoMoves[3];
};

/** A saved (predicted) move, plus the swing input: whether the whip button wants to swing, and at what */
class FEchoSavedMove : public FSavedMove_Character
{
public:

	typedef FSavedMove_Character Super;

	bool bSavedWantsToSwing = false;
	FVector SavedSwingAnchor = FVector::ZeroVector;
	float SavedSwingRopeLength = 0.0f;

	virtual void Clear() override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual bool IsImportantMove(const FSavedMovePtr& LastAckedMove) const override;
};

class FEchoNetworkPredictionData_Client : public FNetworkPredictionData_Client_Character
{
public:

	typedef FNetworkPredictionData_Client_Character Super;

	FEchoNetworkPredictionData_Client(const UCharacterMovementComponent& ClientMovement) : Super(ClientMovement) {}

	virtual FSavedMovePtr AllocateNewMove() override;
};

/**
 *  Character movement with Saraa's whip swing as a custom movement mode, so the swing uses the engine's
 *  normal server-authoritative movement with client prediction: the owning client simulates the swing
 *  immediately from its own input, sends the same input (whip held + anchor + rope length) with each move,
 *  and the server replays it and only corrects the client if they disagree.
 *
 *  The swing is a pendulum: gravity (scaled) plus a little air control across the rope, with the rope as a
 *  maximum-length constraint (slack rope = free fall). Momentum is kept; letting go (whip button released,
 *  or a new jump press) launches with a small boost. Touching walkable ground lets go without a boost.
 *
 *  Tuning lives on the owner's UEchoSwordWhipComponent (so it's all in one place in BP_Saraa).
 */
UCLASS()
class UEchoCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:

	UEchoCharacterMovementComponent();

	/** Whip input (owning client / listen-server host only): swing from this anchor point with this rope length */
	void RequestSwing(const FVector& AnchorPoint, float RopeLength);

	/** Whip input: let go (with the release boost) */
	void StopSwingRequest();

	/** Server: drop out of the swing with no boost (e.g. respawn) */
	void CancelSwing();

	UFUNCTION(BlueprintPure, Category="Echo|Swing")
	bool IsSwinging() const;

	/** Is the whip input asking to swing (for tests) */
	UFUNCTION(BlueprintPure, Category="Echo|Swing")
	bool WantsToSwing() const { return bWantsToSwing; }

	UFUNCTION(BlueprintPure, Category="Echo|Swing")
	FVector GetSwingAnchor() const { return SwingAnchor; }

	UFUNCTION(BlueprintPure, Category="Echo|Swing")
	float GetSwingRopeLength() const { return SwingRopeLength; }

	UFUNCTION(BlueprintPure, Category="Echo|Swing")
	AEchoAnchorPoint* GetSwingAnchorActor() const { return SwingAnchorActor.Get(); }

	/** Swings started / released on this machine, counting only real (not replayed) moves. For tests */
	UFUNCTION(BlueprintPure, Category="Echo|Swing")
	int32 GetSwingsStarted() const { return SwingsStarted; }

	UFUNCTION(BlueprintPure, Category="Echo|Swing")
	int32 GetSwingsReleased() const { return SwingsReleased; }

	// ---- Network correction stats (owning client), for judging how smooth networked swinging is

	UFUNCTION(BlueprintPure, Category="Echo|Swing|Network")
	int32 GetNumClientCorrections() const { return NumClientCorrections; }

	/** Largest single position correction, in cm */
	UFUNCTION(BlueprintPure, Category="Echo|Swing|Network")
	float GetMaxCorrectionDistance() const { return MaxCorrectionDistance; }

	/** Sum of all position corrections, in cm */
	UFUNCTION(BlueprintPure, Category="Echo|Swing|Network")
	float GetTotalCorrectionDistance() const { return TotalCorrectionDistance; }

	UFUNCTION(BlueprintCallable, Category="Echo|Swing|Network")
	void ResetCorrectionStats();

	// ---- UCharacterMovementComponent

	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const override;

	// Saved-move input state (see FEchoSavedMove)
	bool bWantsToSwing = false;
	FVector SwingAnchorRequest = FVector::ZeroVector;
	float SwingRopeRequest = 0.0f;

protected:

	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp, FVector NewLocation, FVector NewVelocity, FMovementBaseInterfaceData* NewMovementBaseInterfaceData, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection) override;

	void PhysSwing(float DeltaTime, int32 Iterations);

	/** Validates the request and enters the swing (same rules on client and server) */
	bool TryStartSwing();

	/** Leaves the swing into falling, optionally with the release boost */
	void EndSwing(bool bLaunch);

	/** The anchor point whose swing point is within a small tolerance of Point */
	AEchoAnchorPoint* FindAnchorNear(const FVector& Point) const;

	UEchoSwordWhipComponent* GetWhip() const;

	/** Is this a real move (not a client replaying saved moves after a correction)? Cues and counters only run then */
	bool IsRealMove() const;

	FVector SwingAnchor = FVector::ZeroVector;
	float SwingRopeLength = 0.0f;
	TWeakObjectPtr<AEchoAnchorPoint> SwingAnchorActor;

	/** Jump was already held when the swing started, so it doesn't count as "jump to let go" until pressed again */
	bool bJumpHeldAtLatch = false;

	int32 SwingsStarted = 0;
	int32 SwingsReleased = 0;
	int32 NumClientCorrections = 0;
	float MaxCorrectionDistance = 0.0f;
	float TotalCorrectionDistance = 0.0f;

	FEchoNetworkMoveDataContainer EchoMoveDataContainer;
};
