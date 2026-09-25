// Our Last Echo

#include "EchoCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "EchoAnchorPoint.h"
#include "EchoSwordWhipComponent.h"
#include "OurLastEchoCharacter.h"

namespace
{
	/** How far the requested anchor may be from a real anchor's swing point (network quantisation, spawn rounding) */
	constexpr float AnchorMatchTolerance = 50.0f;

	/** The server accepts a latch from slightly beyond whip range, for client/server position differences */
	constexpr float RangeTolerance = 100.0f;

	/** The server uses the client's rope length if it's within this of its own measurement (so both simulate the same rope) */
	constexpr float RopeTolerance = 150.0f;
}

// ------------------------------------------------------------------ network move data

void FEchoNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType)
{
	FCharacterNetworkMoveData::ClientFillNetworkMoveData(ClientMove, MoveType);

	const FEchoSavedMove& EchoMove = static_cast<const FEchoSavedMove&>(ClientMove);
	SwingAnchor = EchoMove.SavedSwingAnchor;
	SwingRopeLength = EchoMove.SavedSwingRopeLength;
}

bool FEchoNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	FCharacterNetworkMoveData::Serialize(CharacterMovement, Ar, PackageMap, MoveType);

	// Only sent while the whip wants to swing (FLAG_Custom_0); one bit otherwise
	uint8 bHasSwing = (CompressedMoveFlags & FSavedMove_Character::FLAG_Custom_0) != 0 ? 1 : 0;
	Ar.SerializeBits(&bHasSwing, 1);
	if (bHasSwing)
	{
		bool bSuccess = true;
		SwingAnchor.NetSerialize(Ar, PackageMap, bSuccess);
		Ar << SwingRopeLength;
	}
	else if (Ar.IsLoading())
	{
		SwingAnchor = FVector::ZeroVector;
		SwingRopeLength = 0.0f;
	}
	return !Ar.IsError();
}

FEchoNetworkMoveDataContainer::FEchoNetworkMoveDataContainer()
{
	NewMoveData = &EchoMoves[0];
	PendingMoveData = &EchoMoves[1];
	OldMoveData = &EchoMoves[2];
}

// ------------------------------------------------------------------ saved moves

void FEchoSavedMove::Clear()
{
	Super::Clear();

	bSavedWantsToSwing = false;
	SavedSwingAnchor = FVector::ZeroVector;
	SavedSwingRopeLength = 0.0f;
}

void FEchoSavedMove::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UEchoCharacterMovementComponent* Movement = Cast<UEchoCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		bSavedWantsToSwing = Movement->bWantsToSwing;
		SavedSwingAnchor = Movement->SwingAnchorRequest;
		SavedSwingRopeLength = Movement->SwingRopeRequest;
	}
}

void FEchoSavedMove::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	if (UEchoCharacterMovementComponent* Movement = Cast<UEchoCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		Movement->bWantsToSwing = bSavedWantsToSwing;
		Movement->SwingAnchorRequest = SavedSwingAnchor;
		Movement->SwingRopeRequest = SavedSwingRopeLength;
	}
}

uint8 FEchoSavedMove::GetCompressedFlags() const
{
	uint8 Flags = Super::GetCompressedFlags();
	if (bSavedWantsToSwing)
	{
		Flags |= FLAG_Custom_0;
	}
	return Flags;
}

bool FEchoSavedMove::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FEchoSavedMove* Other = static_cast<const FEchoSavedMove*>(NewMove.Get());
	if (bSavedWantsToSwing != Other->bSavedWantsToSwing
		|| !SavedSwingAnchor.Equals(Other->SavedSwingAnchor, 0.1f)
		|| !FMath::IsNearlyEqual(SavedSwingRopeLength, Other->SavedSwingRopeLength, 0.1f))
	{
		return false;
	}
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

bool FEchoSavedMove::IsImportantMove(const FSavedMovePtr& LastAckedMove) const
{
	// Latching and letting go must reach the server even if the packet carrying them is lost
	const FEchoSavedMove* Acked = static_cast<const FEchoSavedMove*>(LastAckedMove.Get());
	if (Acked && Acked->bSavedWantsToSwing != bSavedWantsToSwing)
	{
		return true;
	}
	return Super::IsImportantMove(LastAckedMove);
}

FSavedMovePtr FEchoNetworkPredictionData_Client::AllocateNewMove()
{
	return FSavedMovePtr(new FEchoSavedMove());
}

// ------------------------------------------------------------------ component

UEchoCharacterMovementComponent::UEchoCharacterMovementComponent()
{
	SetNetworkMoveDataContainer(EchoMoveDataContainer);
}

FNetworkPredictionData_Client* UEchoCharacterMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		UEchoCharacterMovementComponent* MutableThis = const_cast<UEchoCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FEchoNetworkPredictionData_Client(*this);
	}
	return ClientPredictionData;
}

void UEchoCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToSwing = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

void UEchoCharacterMovementComponent::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel)
{
	// Server: pick up the swing request that came with this move before simulating it
	if (const FEchoNetworkMoveData* MoveData = static_cast<const FEchoNetworkMoveData*>(GetCurrentNetworkMoveData()))
	{
		SwingAnchorRequest = MoveData->SwingAnchor;
		SwingRopeRequest = MoveData->SwingRopeLength;
	}

	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);
}

UEchoSwordWhipComponent* UEchoCharacterMovementComponent::GetWhip() const
{
	const AOurLastEchoCharacter* Character = Cast<AOurLastEchoCharacter>(CharacterOwner);
	return Character ? Character->GetSwordWhip() : nullptr;
}

bool UEchoCharacterMovementComponent::IsRealMove() const
{
	return CharacterOwner && !CharacterOwner->bClientUpdating;
}

bool UEchoCharacterMovementComponent::IsSwinging() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(EEchoCustomMovement::Swing);
}

void UEchoCharacterMovementComponent::RequestSwing(const FVector& AnchorPoint, float RopeLength)
{
	// Rounded the way the network move rounds it, so client and server simulate exactly the same rope
	SwingAnchorRequest = FVector(FMath::RoundToDouble(AnchorPoint.X * 10.0) / 10.0, FMath::RoundToDouble(AnchorPoint.Y * 10.0) / 10.0, FMath::RoundToDouble(AnchorPoint.Z * 10.0) / 10.0);
	SwingRopeRequest = RopeLength;
	bWantsToSwing = true;
}

void UEchoCharacterMovementComponent::StopSwingRequest()
{
	bWantsToSwing = false;
}

void UEchoCharacterMovementComponent::CancelSwing()
{
	bWantsToSwing = false;
	if (IsSwinging())
	{
		EndSwing(false);
	}
}

AEchoAnchorPoint* UEchoCharacterMovementComponent::FindAnchorNear(const FVector& Point) const
{
	for (TActorIterator<AEchoAnchorPoint> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && FVector::DistSquared(It->GetSwingPoint(), Point) <= FMath::Square(AnchorMatchTolerance))
		{
			return *It;
		}
	}
	return nullptr;
}

void UEchoCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	if (!CharacterOwner || CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}

	if (IsSwinging())
	{
		if (!CharacterOwner->bPressedJump)
		{
			bJumpHeldAtLatch = false;
		}

		if (!SwingAnchorActor.IsValid())
		{
			// Bat's third arrow removed the anchor we were hanging from
			EndSwing(false);
		}
		else if (!bWantsToSwing || (CharacterOwner->bPressedJump && !bJumpHeldAtLatch))
		{
			EndSwing(true);
		}
	}
	else if (bWantsToSwing && !TryStartSwing())
	{
		// A request that can't be honoured is dropped, so it can't fire later when the anchor comes into range
		bWantsToSwing = false;
	}
}

bool UEchoCharacterMovementComponent::TryStartSwing()
{
	UEchoSwordWhipComponent* Whip = GetWhip();
	if (!Whip || !Whip->CanUseWhip() || !UpdatedComponent)
	{
		return false;
	}

	AEchoAnchorPoint* Anchor = FindAnchorNear(SwingAnchorRequest);
	if (!Anchor)
	{
		return false;
	}

	const FVector Location = UpdatedComponent->GetComponentLocation();
	const float Distance = FVector::Dist(Location, SwingAnchorRequest);
	if (Distance > Whip->WhipRange + RangeTolerance)
	{
		return false;
	}

	float Rope = SwingRopeRequest;
	if (FMath::Abs(Rope - Distance) > RopeTolerance)
	{
		Rope = Distance;
	}

	SwingAnchor = SwingAnchorRequest;
	SwingRopeLength = FMath::Clamp(Rope, Whip->MinRopeLength, Whip->WhipRange + RangeTolerance);
	SwingAnchorActor = Anchor;
	bJumpHeldAtLatch = CharacterOwner->bPressedJump;

	// A tug towards the anchor gets the swing going; from the ground, a hop gets her off it
	Velocity += (SwingAnchor - Location).GetSafeNormal2D() * Whip->LatchBoost;
	if (IsMovingOnGround())
	{
		Velocity.Z = FMath::Max(Velocity.Z, Whip->GroundLatchHop);
	}

	SetMovementMode(MOVE_Custom, static_cast<uint8>(EEchoCustomMovement::Swing));

	if (IsRealMove())
	{
		++SwingsStarted;
		Whip->NotifySwingStarted(Anchor);
	}
	return true;
}

void UEchoCharacterMovementComponent::EndSwing(bool bLaunch)
{
	UEchoSwordWhipComponent* Whip = GetWhip();
	bWantsToSwing = false;

	if (bLaunch && Whip)
	{
		// Momentum plus a small boost forward (along the swing) and up
		FVector Forward = Velocity.GetSafeNormal2D();
		if (Forward.IsNearlyZero())
		{
			Forward = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
		}
		Velocity += Forward * Whip->ReleaseBoost + FVector(0.0f, 0.0f, Whip->ReleaseUpBoost);
	}

	SwingAnchorActor = nullptr;
	SetMovementMode(MOVE_Falling);

	if (IsRealMove())
	{
		++SwingsReleased;
		if (Whip)
		{
			Whip->NotifySwingEnded(bLaunch);
		}
	}
}

void UEchoCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	// Left the swing some other way (teleport, correction): forget the anchor
	if (!IsSwinging() && PreviousMovementMode == MOVE_Custom && PreviousCustomMode == static_cast<uint8>(EEchoCustomMovement::Swing))
	{
		SwingAnchorActor = nullptr;
	}
}

void UEchoCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	if (CustomMovementMode == static_cast<uint8>(EEchoCustomMovement::Swing))
	{
		PhysSwing(DeltaTime, Iterations);
		return;
	}
	Super::PhysCustom(DeltaTime, Iterations);
}

void UEchoCharacterMovementComponent::PhysSwing(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	const UEchoSwordWhipComponent* Whip = GetWhip();
	if (!Whip)
	{
		EndSwing(false);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	float RemainingTime = DeltaTime;
	while (RemainingTime >= MIN_TICK_TIME && Iterations < MaxSimulationIterations && CharacterOwner && IsSwinging())
	{
		++Iterations;
		const float TimeTick = GetSimulationTimeStep(RemainingTime, Iterations);
		RemainingTime -= TimeTick;

		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FVector RopeDir = (OldLocation - SwingAnchor).GetSafeNormal();

		// Gravity, plus steering from the move input across the rope (a little air control)
		FVector Accel(0.0f, 0.0f, GetGravityZ() * Whip->SwingGravityScale);
		if (MaxAcceleration > KINDA_SMALL_NUMBER)
		{
			const FVector Input = (Acceleration / MaxAcceleration).GetClampedToMaxSize(1.0f);
			Accel += FVector::VectorPlaneProject(Input, RopeDir) * Whip->SwingAirControl;
		}
		Velocity = (Velocity + Accel * TimeTick).GetClampedToMaxSize(Whip->MaxSwingSpeed);

		// The rope: never further than its length from the anchor, and no speed outwards along it once taut
		FVector Desired = OldLocation + Velocity * TimeTick;
		const FVector FromAnchor = Desired - SwingAnchor;
		const float Distance = FromAnchor.Size();
		if (Distance > SwingRopeLength && Distance > KINDA_SMALL_NUMBER)
		{
			const FVector Dir = FromAnchor / Distance;
			Desired = SwingAnchor + Dir * SwingRopeLength;
			const float Outward = FVector::DotProduct(Velocity, Dir);
			if (Outward > 0.0f)
			{
				Velocity -= Dir * Outward;
			}
		}

		const FVector Delta = Desired - OldLocation;
		FHitResult Hit(1.0f);
		SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

		if (Hit.IsValidBlockingHit())
		{
			if (Velocity.Z <= 0.0f && IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
			{
				// Touched down on walkable ground: let go (no boost) and let falling physics land her
				EndSwing(false);
				RemainingTime += TimeTick * (1.0f - Hit.Time);
				StartNewPhysics(RemainingTime, Iterations);
				return;
			}

			// Scrape along walls and ceilings, losing the speed going into them
			HandleImpact(Hit, TimeTick, Delta);
			SlideAlongSurface(Delta, 1.0f - Hit.Time, Hit.Normal, Hit, true);
			Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal);
		}
	}
}

FRotator UEchoCharacterMovementComponent::ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const
{
	// Face the way she's swinging, not the way the stick points
	if (IsSwinging())
	{
		const FVector Direction = Velocity.GetSafeNormal2D();
		return Direction.IsNearlyZero() ? CurrentRotation : Direction.Rotation();
	}
	return Super::ComputeOrientToMovementRotation(CurrentRotation, DeltaTime, DeltaRotation);
}

void UEchoCharacterMovementComponent::OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp, FVector NewLocation, FVector NewVelocity, FMovementBaseInterfaceData* NewMovementBaseInterfaceData, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection)
{
	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewMovementBaseInterfaceData, NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode, ServerGravityDirection);

	++NumClientCorrections;
	if (!bBaseRelativePosition && UpdatedComponent)
	{
		const float Error = FVector::Dist(NewLocation, UpdatedComponent->GetComponentLocation());
		MaxCorrectionDistance = FMath::Max(MaxCorrectionDistance, Error);
		TotalCorrectionDistance += Error;
	}
}

void UEchoCharacterMovementComponent::ResetCorrectionStats()
{
	NumClientCorrections = 0;
	MaxCorrectionDistance = 0.0f;
	TotalCorrectionDistance = 0.0f;
}
