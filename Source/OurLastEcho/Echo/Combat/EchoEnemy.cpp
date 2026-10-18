// Our Last Echo

#include "EchoEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "EchoEnemyController.h"
#include "EchoGameState.h"
#include "EchoVisibility.h"
#include "OurLastEcho.h"
#include "OurLastEchoCharacter.h"

namespace
{
	/** A hit flashes the body white this long */
	constexpr float HitFlashSeconds = 0.12f;
}

AEchoEnemy::AEchoEnemy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	AIControllerClass = AEchoEnemyController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 90.0f);
	GetCapsuleComponent()->SetCanEverAffectNavigation(false);

	// No skeletal mesh: the body is built from engine shapes
	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	Movement->MaxWalkSpeed = MoveSpeed;
	Movement->bUseRVOAvoidance = false;

	Health = CreateDefaultSubobject<UEchoHealthComponent>(TEXT("Health"));
	Health->Team = EEchoTeam::Enemies;
	Health->MaxHealth = 60.0f;

	BodyRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BodyRoot"));
	BodyRoot->SetupAttachment(GetCapsuleComponent());

	ArrowHitBox = CreateDefaultSubobject<UCapsuleComponent>(TEXT("ArrowHitBox"));
	ArrowHitBox->SetupAttachment(GetCapsuleComponent());
	ArrowHitBox->InitCapsuleSize(46.0f, 92.0f);
	ArrowHitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ArrowHitBox->SetCollisionObjectType(ECC_WorldDynamic);
	ArrowHitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	ArrowHitBox->SetCollisionResponseToChannel(ECC_EchoArrow, ECR_Block);
	ArrowHitBox->SetGenerateOverlapEvents(false);
	ArrowHitBox->SetCanEverAffectNavigation(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(BodyRoot);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(0.0f);
	Glow->SetAttenuationRadius(600.0f);
	Glow->SetCastShadows(false);

	BodyMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Combat/M_EnemyBody.M_EnemyBody")));
	TelegraphSound = EchoCombat::EngineSound(TEXT("VR_shep_scale_up_01"));
	StrikeSound = EchoCombat::EngineSound(TEXT("VR_close"));
	DisplayName = NSLOCTEXT("EchoEnemy", "Enemy", "Enemy");
}

void AEchoEnemy::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Collision from presence, the same on every machine (so it agrees with movement):
	// - Bat only: a Pawn that ignores Saraa's channel; Saraa only: on Saraa's channel, ignoring Bat's;
	// - both: WorldDynamic, which both players' capsules block.
	// The capsule never blocks arrows, the camera or sight traces: arrows hit ArrowHitBox (only if Bat can affect it)
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->SetCollisionResponseToAllChannels(ECR_Block);
	Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_EchoArrow, ECR_Ignore);
	switch (Presence)
	{
	case EEchoPresence::LivingOnly:
		Capsule->SetCollisionObjectType(ECC_Pawn);
		Capsule->SetCollisionResponseToChannel(ECC_SpiritPawn, ECR_Ignore);
		break;
	case EEchoPresence::SpiritOnly:
		Capsule->SetCollisionObjectType(ECC_SpiritPawn);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		break;
	default:
		Capsule->SetCollisionObjectType(ECC_WorldDynamic);
		break;
	}
	ArrowHitBox->SetCollisionEnabled(IsInWorldOf(EEchoRealm::Living) ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void AEchoEnemy::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	GetCharacterMovement()->MaxFlySpeed = MoveSpeed;
	Health->OnHitCue.AddUObject(this, &AEchoEnemy::HandleHitCue);
	if (HasAuthority())
	{
		Health->OnDepleted.AddDynamic(this, &AEchoEnemy::HandleDepleted);
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		BuildBody();
		BodyBaseScale = BodyRoot->GetRelativeScale3D();
		Glow->SetLightColor(TelegraphColor);
		UpdatePresence();
	}
}

void AEchoEnemy::BuildBody()
{
	// Default: a rounded column; subclasses replace this
	AddPart(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(0.0f, 0.0f, -10.0f), FVector(0.7f, 0.7f, 1.6f));
	AddPart(TEXT("/Engine/BasicShapes/Sphere.Sphere"), FVector(0.0f, 0.0f, 70.0f), FVector(0.6f));
}

UStaticMeshComponent* AEchoEnemy::AddPart(const TCHAR* MeshPath, const FVector& Location, const FVector& Scale, const FRotator& Rotation, bool bUsesBodyMaterial)
{
	if (!BodyMID)
	{
		if (UMaterialInterface* Base = BodyMaterial.LoadSynchronous())
		{
			BodyMID = UMaterialInstanceDynamic::Create(Base, this);
			BodyMID->SetVectorParameterValue(TEXT("Color"), BodyColor);
			BodyMID->SetScalarParameterValue(TEXT("Glow"), 0.0f);
		}
	}
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
	Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath));
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	Part->SetCastShadow(true);
	Part->SetRelativeTransform(FTransform(Rotation, Location, Scale));
	if (bUsesBodyMaterial && BodyMID)
	{
		Part->SetMaterial(0, BodyMID);
	}
	Part->SetupAttachment(BodyRoot);
	Part->RegisterComponent();
	BodyParts.Add(Part);
	return Part;
}

float AEchoEnemy::ServerNow() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	return GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : GetWorld()->GetTimeSeconds();
}

bool AEchoEnemy::CanTarget(const AOurLastEchoCharacter* Player) const
{
	return Player && !bDefeated && IsInWorldOf(Player->GetRealm()) && !Player->IsDowned();
}

FVector AEchoEnemy::GetHealthBarLocation() const
{
	return GetActorLocation() + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 45.0f);
}

void AEchoEnemy::SetTarget(AOurLastEchoCharacter* NewTarget)
{
	if (Target != NewTarget)
	{
		Target = NewTarget;
		if (!Target && AttackPhase != EEchoAttackPhase::None)
		{
			CancelAttack();
		}
	}
}

// ------------------------------------------------------------------ brain

bool AEchoEnemy::ChaseTick(float DeltaTime)
{
	if (!Target || bDefeated)
	{
		return false;
	}
	const float Distance = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());
	AEchoEnemyController* Brain = Cast<AEchoEnemyController>(GetController());
	if (Distance <= AttackRange)
	{
		if (Brain)
		{
			Brain->StopMovement();
		}
		return true;
	}
	if (Brain)
	{
		Brain->ChaseActor(Target, AttackRange * 0.7f);
	}
	return false;
}

void AEchoEnemy::IdleTick(float DeltaTime)
{
	if (AEchoEnemyController* Brain = Cast<AEchoEnemyController>(GetController()))
	{
		Brain->StopMovement();
	}
}

bool AEchoEnemy::CanStartAttack() const
{
	return Target && !bDefeated && !IsStunned() && AttackPhase == EEchoAttackPhase::None
		&& GetWorld()->GetTimeSeconds() - LastAttackEndTime >= AttackCooldown
		&& FVector::Dist2D(GetActorLocation(), Target->GetActorLocation()) <= AttackRange * 1.15f
		&& CanTarget(Target);
}

void AEchoEnemy::StartAttack()
{
	if (AEchoEnemyController* Brain = Cast<AEchoEnemyController>(GetController()))
	{
		Brain->StopMovement();
	}
	SetPhase(EEchoAttackPhase::WindUp);
}

bool AEchoEnemy::AttackTick(float DeltaTime)
{
	const float Elapsed = ServerNow() - PhaseStartTime;
	switch (AttackPhase)
	{
	case EEchoAttackPhase::WindUp:
		// Turn to face the target for most of the wind-up, then commit (so a late dodge works)
		if (Target && Elapsed < AttackWindUp * 0.7f)
		{
			const FVector ToTarget = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			if (!ToTarget.IsNearlyZero())
			{
				SetActorRotation(FMath::RInterpTo(GetActorRotation(), ToTarget.Rotation(), DeltaTime, 8.0f));
			}
		}
		if (Elapsed >= AttackWindUp)
		{
			SetPhase(EEchoAttackPhase::Strike);
			PerformStrike();
		}
		return false;
	case EEchoAttackPhase::Strike:
		if (Elapsed >= AttackStrikeTime)
		{
			SetPhase(EEchoAttackPhase::Recover);
		}
		return false;
	case EEchoAttackPhase::Recover:
		if (Elapsed >= AttackRecover)
		{
			SetPhase(EEchoAttackPhase::None);
			LastAttackEndTime = GetWorld()->GetTimeSeconds();
			return true;
		}
		return false;
	default:
		return true;
	}
}

void AEchoEnemy::CancelAttack()
{
	if (AttackPhase != EEchoAttackPhase::None)
	{
		SetPhase(EEchoAttackPhase::None);
		LastAttackEndTime = GetWorld()->GetTimeSeconds();
	}
}

void AEchoEnemy::PerformStrike()
{
	const FVector Center = GetActorLocation() + GetActorForwardVector() * AttackReach;
	for (TActorIterator<AOurLastEchoCharacter> It(GetWorld()); It; ++It)
	{
		AOurLastEchoCharacter* Player = *It;
		if (!CanTarget(Player))
		{
			continue;
		}
		const float Reach = AttackRadius + Player->GetCapsuleComponent()->GetScaledCapsuleRadius();
		if (FVector::DistSquared(Center, Player->GetActorLocation()) <= FMath::Square(Reach))
		{
			FEchoDamageInfo Info;
			Info.Amount = AttackDamage;
			Info.Kind = EEchoAttackKind::Enemy;
			Info.bHeavy = bHeavyAttack;
			Info.Instigator = this;
			Info.HitLocation = Player->GetActorLocation();
			Info.Direction = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			Player->GetHealth()->ApplyDamage(Info);
		}
	}
}

void AEchoEnemy::SetPhase(EEchoAttackPhase NewPhase)
{
	AttackPhase = NewPhase;
	PhaseStartTime = ServerNow();
	ForceNetUpdate();
	PlayPhaseCue();
}

void AEchoEnemy::OnRep_AttackPhase()
{
	PlayPhaseCue();
}

void AEchoEnemy::PlayPhaseCue()
{
	if (GetNetMode() == NM_DedicatedServer || !bShownLocally)
	{
		return;
	}
	USoundBase* Sound = AttackPhase == EEchoAttackPhase::WindUp ? TelegraphSound.Get() : AttackPhase == EEchoAttackPhase::Strike ? StrikeSound.Get() : nullptr;
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
}

// ------------------------------------------------------------------ damage

EEchoDamageResult AEchoEnemy::FilterIncomingDamage(FEchoDamageInfo& Info)
{
	if (bDefeated)
	{
		return EEchoDamageResult::Ignored;
	}
	if (const AOurLastEchoCharacter* Player = Cast<AOurLastEchoCharacter>(Info.Instigator))
	{
		// Not in this player's world: they can't affect it at all
		if (!IsInWorldOf(Player->GetRealm()))
		{
			return EEchoDamageResult::Ignored;
		}
	}
	return EEchoDamageResult::Applied;
}

void AEchoEnemy::HandleDepleted(UEchoHealthComponent* InHealth, const FEchoDamageInfo& Info)
{
	if (bDefeated)
	{
		return;
	}
	bDefeated = true;
	CancelAttack();
	if (AEchoEnemyController* Brain = Cast<AEchoEnemyController>(GetController()))
	{
		Brain->StopBrain();
	}
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArrowHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OnRep_Defeated();
	ForceNetUpdate();
	UE_LOG(LogOurLastEcho, Log, TEXT("Combat: %s defeated by %s"), *GetName(), *GetNameSafe(Info.Instigator));
	OnDefeated.Broadcast(this);
	SetLifeSpan(DefeatRemoveDelay);
}

void AEchoEnemy::RemoveQuietly()
{
	if (HasAuthority())
	{
		bDefeated = true;
		OnDefeated.Clear();
		Destroy();
	}
}

void AEchoEnemy::OnRep_Defeated()
{
	if (bDefeated && DefeatedAtLocal < 0.0f)
	{
		DefeatedAtLocal = GetWorld()->GetTimeSeconds();
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ArrowHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AEchoEnemy::HandleHitCue(UEchoHealthComponent* InHealth, const FEchoHitCue& Cue)
{
	// The look reads the cue time from the health component (flash); nothing else to do here
}

// ------------------------------------------------------------------ look

void AEchoEnemy::UpdatePresence()
{
	EEchoRealm Realm;
	const AEchoGameState* GameState = GetWorld()->GetGameState<AEchoGameState>();
	const bool bShowAll = GameState && GameState->IsDebugShowAllEnemies();
	bShownLocally = bShowAll || !EchoVisibility::GetLocalViewerRealm(GetWorld(), Realm) || IsInWorldOf(Realm);
	if (bShownLocally != bLastShown)
	{
		bLastShown = bShownLocally;
		BodyRoot->SetVisibility(bShownLocally, true);
	}
}

float AEchoEnemy::GetGlowAmount() const
{
	const float Elapsed = ServerNow() - PhaseStartTime;
	switch (AttackPhase)
	{
	case EEchoAttackPhase::WindUp: return FMath::Clamp(Elapsed / AttackWindUp, 0.0f, 1.0f);
	case EEchoAttackPhase::Strike: return 1.0f;
	case EEchoAttackPhase::Recover: return FMath::Clamp(1.0f - Elapsed / FMath::Max(AttackRecover, 0.05f), 0.0f, 1.0f) * 0.5f;
	default: return 0.0f;
	}
}

void AEchoEnemy::UpdateLook(float DeltaSeconds)
{
	if (!BodyMID)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const float SinceHit = Now - Health->GetLastCueTime();
	const EEchoDamageResult LastResult = Health->GetLastHitCue().Result;
	const bool bFlash = SinceHit < HitFlashSeconds && (LastResult == EEchoDamageResult::Applied || LastResult == EEchoDamageResult::Defeated);
	const bool bSpark = SinceHit < HitFlashSeconds * 1.5f && (LastResult == EEchoDamageResult::Blocked || LastResult == EEchoDamageResult::Opened);

	const float GlowAmount = GetGlowAmount();
	BodyMID->SetVectorParameterValue(TEXT("Color"), bFlash ? FLinearColor::White : BodyColor);
	BodyMID->SetVectorParameterValue(TEXT("GlowColor"), bSpark ? FLinearColor(1.0f, 0.9f, 0.5f) : TelegraphColor);
	// (M_EnemyBody triples Glow; above ~1.5 the canyon's exposure turns it white)
	BodyMID->SetScalarParameterValue(TEXT("Glow"), bFlash ? 0.6f : bSpark ? 0.5f : GlowAmount * 0.45f);
	Glow->SetLightColor(bSpark ? FLinearColor(1.0f, 0.9f, 0.5f) : TelegraphColor);
	Glow->SetIntensity(bSpark ? 60.0f : GlowAmount * 40.0f);

	// Swell during the wind-up; the defeat pop: a quick swell, then shrink to nothing
	float Scale = 1.0f + (AttackPhase == EEchoAttackPhase::WindUp ? TelegraphSwell * GlowAmount : 0.0f);
	if (DefeatedAtLocal >= 0.0f)
	{
		const float T = FMath::Clamp((Now - DefeatedAtLocal) / FMath::Max(DefeatRemoveDelay * 0.8f, 0.1f), 0.0f, 1.0f);
		Scale = T < 0.25f ? FMath::Lerp(1.0f, 1.3f, T / 0.25f) : FMath::Lerp(1.3f, 0.01f, (T - 0.25f) / 0.75f);
		BodyMID->SetScalarParameterValue(TEXT("Glow"), 0.6f * (1.0f - T));
	}
	BodyRoot->SetRelativeScale3D(BodyBaseScale * Scale);
}

void AEchoEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdatePresence();
		UpdateLook(DeltaSeconds);
	}
}

void AEchoEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEchoEnemy, Target);
	DOREPLIFETIME(AEchoEnemy, AttackPhase);
	DOREPLIFETIME(AEchoEnemy, PhaseStartTime);
	DOREPLIFETIME(AEchoEnemy, bDefeated);
}
