// Our Last Echo

#include "EchoPlayerCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "EchoCameraShake.h"
#include "EchoCharacterMovementComponent.h"
#include "EchoGameMode.h"
#include "EchoSpiritBowComponent.h"
#include "OurLastEcho.h"
#include "OurLastEchoCharacter.h"

namespace
{
	constexpr float FlinchSeconds = 0.25f;
}

UEchoPlayerCombatComponent::UEchoPlayerCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	DodgeAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_Dodge.IA_Dodge")));
	CombatMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Input/IMC_Combat.IMC_Combat")));
}

AOurLastEchoCharacter* UEchoPlayerCombatComponent::GetCharacter() const
{
	return Cast<AOurLastEchoCharacter>(GetOwner());
}

UEchoHealthComponent* UEchoPlayerCombatComponent::GetHealth() const
{
	return UEchoHealthComponent::FindOn(GetOwner());
}

void UEchoPlayerCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UEchoHealthComponent* Health = GetHealth())
	{
		Health->Team = EEchoTeam::Players;
		Health->OnHitCue.AddUObject(this, &UEchoPlayerCombatComponent::HandleHitCue);
		if (GetOwner()->HasAuthority())
		{
			Health->OnDepleted.AddDynamic(this, &UEchoPlayerCombatComponent::HandleDepleted);
		}
	}
}

void UEchoPlayerCombatComponent::SetupPlayerInput(UEnhancedInputComponent* Input)
{
	UInputAction* Dodge = DodgeAction.LoadSynchronous();
	if (!Input || !Dodge)
	{
		UE_LOG(LogOurLastEcho, Warning, TEXT("Combat: IA_Dodge missing (run Scripts/build_combat.py)"));
		return;
	}
	Input->BindAction(Dodge, ETriggerEvent::Started, this, &UEchoPlayerCombatComponent::OnDodgePressed);

	const APawn* Pawn = GetCharacter();
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr)
	{
		if (UInputMappingContext* Context = CombatMappingContext.LoadSynchronous())
		{
			Subsystem->AddMappingContext(Context, MappingPriority);
		}
	}
}

void UEchoPlayerCombatComponent::OnDodgePressed()
{
	RequestDodge();
}

bool UEchoPlayerCombatComponent::RequestDodge()
{
	AOurLastEchoCharacter* Character = GetCharacter();
	UEchoCharacterMovementComponent* Movement = Character ? Character->GetEchoMovement() : nullptr;
	if (!Movement || bDowned || Movement->IsSwinging() || !Movement->IsMovingOnGround()
		|| GetWorld()->GetTimeSeconds() - LastDodgeTime < DodgeCooldown)
	{
		return false;
	}
	// The movement component rolls on its next update; the same request goes to the server with the move
	Movement->bWantsToDodge = true;
	LastDodgeTime = GetWorld()->GetTimeSeconds();
	return true;
}

bool UEchoPlayerCombatComponent::CanServerDodge() const
{
	// The server's own cooldown, with slack for network timing (a refused roll gets corrected)
	return !bDowned && GetWorld()->GetTimeSeconds() - LastDodgeTime >= DodgeCooldown * 0.6f;
}

void UEchoPlayerCombatComponent::NotifyDodged()
{
	LastDodgeTime = GetWorld()->GetTimeSeconds();
	DodgeLookRemaining = DodgeLookDuration;
	if (GetOwner()->HasAuthority())
	{
		++DodgeCount;
		if (UEchoHealthComponent* Health = GetHealth())
		{
			Health->SetInvulnerableFor(DodgeInvulnerability);
		}
	}
}

void UEchoPlayerCombatComponent::OnRep_DodgeCount()
{
	// The other player's machine: spin with the roll (the owner already did, from its own move)
	const AOurLastEchoCharacter* Character = GetCharacter();
	if (Character && !Character->IsLocallyControlled())
	{
		DodgeLookRemaining = DodgeLookDuration;
	}
}

void UEchoPlayerCombatComponent::HandleDepleted(UEchoHealthComponent* Health, const FEchoDamageInfo& Info)
{
	AOurLastEchoCharacter* Character = GetCharacter();
	if (!Character || bDowned)
	{
		return;
	}
	bDowned = true;
	DownedAt = GetWorld()->GetTimeSeconds();
	Character->GetEchoMovement()->CancelSwing();
	Character->GetCharacterMovement()->StopMovementImmediately();
	if (UEchoSpiritBowComponent* Bow = Character->GetSpiritBow())
	{
		Bow->SetAiming(false);
	}
	GetOwner()->ForceNetUpdate();
	UE_LOG(LogOurLastEcho, Log, TEXT("Combat: %s is down (respawn in %.1f s)"), *Character->GetName(), RespawnDelay);

	if (AEchoGameMode* GameMode = GetWorld()->GetAuthGameMode<AEchoGameMode>())
	{
		GameMode->HandlePlayerDowned(Character);
	}
	GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &UEchoPlayerCombatComponent::OnRespawnTimer, FMath::Max(0.1f, RespawnDelay), false);
}

void UEchoPlayerCombatComponent::OnRespawnTimer()
{
	AOurLastEchoCharacter* Character = GetCharacter();
	if (!bDowned || !Character)
	{
		return;
	}
	if (AEchoGameMode* GameMode = GetWorld()->GetAuthGameMode<AEchoGameMode>())
	{
		GameMode->RespawnDownedPlayer(Character);
	}
	else
	{
		Character->RespawnAtStart();
		ServerRevive();
	}
}

void UEchoPlayerCombatComponent::ServerRevive()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);
	bDowned = false;
	if (UEchoHealthComponent* Health = GetHealth())
	{
		Health->Revive();
		// A moment to get your bearings
		Health->SetInvulnerableFor(1.5f);
	}
	GetOwner()->ForceNetUpdate();
}

void UEchoPlayerCombatComponent::OnRep_Downed()
{
}

float UEchoPlayerCombatComponent::GetRespawnRemaining() const
{
	if (!bDowned)
	{
		return 0.0f;
	}
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	return FMath::Max(0.0f, RespawnDelay - static_cast<float>(Now - DownedAt));
}

void UEchoPlayerCombatComponent::HandleHitCue(UEchoHealthComponent* Health, const FEchoHitCue& Cue)
{
	if (Cue.Result != EEchoDamageResult::Applied && Cue.Result != EEchoDamageResult::Defeated)
	{
		return;
	}
	// Everyone sees the flinch; the hurt player also gets a shake and the HUD's red flash
	FlinchRemaining = FlinchSeconds;
	AOurLastEchoCharacter* Character = GetCharacter();
	if (Character && Character->IsLocallyControlled())
	{
		LastHurtTime = GetWorld()->GetTimeSeconds();
		UEchoCameraShake::Play(Cast<APlayerController>(Character->GetController()), HurtShakeScale * (Cue.bHeavy ? 1.6f : 1.0f));
	}
}

void UEchoPlayerCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AOurLastEchoCharacter* Character = GetCharacter();
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!bMeshBaseCaptured)
	{
		BaseMeshRotation = Mesh->GetRelativeRotation();
		BaseMeshLocation = Mesh->GetRelativeLocation();
		bMeshBaseCaptured = true;
	}

	// The look of rolling, flinching and lying down, layered on the mesh's own placement (no animations yet)
	DodgeLookRemaining = FMath::Max(0.0f, DodgeLookRemaining - DeltaTime);
	FlinchRemaining = FMath::Max(0.0f, FlinchRemaining - DeltaTime);
	DownedBlend = FMath::FInterpTo(DownedBlend, bDowned ? 1.0f : 0.0f, DeltaTime, 8.0f);

	// The template mesh faces +Y in its own space (yaw -90): its "roll" axis for a forward somersault is pitch here
	const float Spin = DodgeLookRemaining > 0.0f ? 360.0f * (1.0f - DodgeLookRemaining / DodgeLookDuration) : 0.0f;
	const float Flinch = FlinchRemaining > 0.0f ? FlinchAngle * FMath::Sin(PI * FlinchRemaining / FlinchSeconds) : 0.0f;
	const float Lie = 80.0f * DownedBlend;
	const FQuat Extra = FQuat(FVector::RightVector, FMath::DegreesToRadians(Spin)) * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(-Flinch + Lie));
	// Pivot the roll around the body's middle rather than the feet
	const FVector Pivot(0.0f, 0.0f, 90.0f);
	const FVector Offset = BaseMeshLocation + Pivot - Extra.RotateVector(Pivot) + FVector(0.0f, 0.0f, -40.0f * DownedBlend);
	Mesh->SetRelativeLocationAndRotation(Offset, Extra * BaseMeshRotation.Quaternion());
}

void UEchoPlayerCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEchoPlayerCombatComponent, bDowned);
	DOREPLIFETIME(UEchoPlayerCombatComponent, DownedAt);
	DOREPLIFETIME_CONDITION(UEchoPlayerCombatComponent, DodgeCount, COND_SkipOwner);
}
