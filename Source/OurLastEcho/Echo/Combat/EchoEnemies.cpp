// Our Last Echo

#include "EchoEnemies.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "EchoEnemyController.h"
#include "OurLastEcho.h"
#include "OurLastEchoCharacter.h"

namespace
{
	const TCHAR* const EnemySphereMesh = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* const EnemyCubeMesh = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* const EnemyConeMesh = TEXT("/Engine/BasicShapes/Cone.Cone");
	const TCHAR* const EnemyCylinderMesh = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

	/** A second flat-colour material for glowing details (eyes, cores) */
	UMaterialInstanceDynamic* MakeGlowMaterial(AActor* Owner, UMaterialInterface* Base, const FLinearColor& Color, float GlowAmount)
	{
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Owner);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetVectorParameterValue(TEXT("GlowColor"), Color);
		MID->SetScalarParameterValue(TEXT("Glow"), GlowAmount);
		return MID;
	}

	void PlayAt(const AActor* Actor, USoundBase* Sound)
	{
		if (Sound && Actor && Actor->GetNetMode() != NM_DedicatedServer)
		{
			UGameplayStatics::PlaySoundAtLocation(Actor, Sound, Actor->GetActorLocation());
		}
	}
}

// ------------------------------------------------------------------ Dust Wraith (Bat only)

AEchoDustWraith::AEchoDustWraith(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Presence = EEchoPresence::LivingOnly;
	DisplayName = NSLOCTEXT("EchoEnemy", "DustWraith", "Dust Wraith");
	BodyColor = FLinearColor(0.55f, 0.36f, 0.17f);
	TelegraphColor = FLinearColor(1.0f, 0.55f, 0.12f);
	Health->MaxHealth = 60.0f;
	MoveSpeed = 360.0f;
	AttackRange = 200.0f;
	AttackWindUp = 0.75f;
	AttackDamage = 12.0f;
	AttackRadius = 130.0f;
	AttackReach = 100.0f;
}

void AEchoDustWraith::BuildBody()
{
	// A dust devil: an upturned swirl of cones, a hunched body and two claws
	AddPart(EnemyConeMesh, FVector(0.0f, 0.0f, -45.0f), FVector(0.9f, 0.9f, 0.9f), FRotator(180.0f, 0.0f, 0.0f));
	AddPart(EnemyConeMesh, FVector(0.0f, 0.0f, -10.0f), FVector(0.7f, 0.7f, 0.7f), FRotator(180.0f, 40.0f, 0.0f));
	AddPart(EnemySphereMesh, FVector(0.0f, 0.0f, 40.0f), FVector(0.75f, 0.75f, 0.85f));
	AddPart(EnemySphereMesh, FVector(18.0f, 0.0f, 85.0f), FVector(0.45f));
	AddPart(EnemyConeMesh, FVector(35.0f, 38.0f, 35.0f), FVector(0.18f, 0.18f, 0.55f), FRotator(-70.0f, 20.0f, 0.0f));
	AddPart(EnemyConeMesh, FVector(35.0f, -38.0f, 35.0f), FVector(0.18f, 0.18f, 0.55f), FRotator(-70.0f, -20.0f, 0.0f));
	if (UMaterialInstanceDynamic* Eyes = MakeGlowMaterial(this, BodyMaterial.LoadSynchronous(), FLinearColor(1.0f, 0.7f, 0.2f), 0.5f))
	{
		AddPart(EnemySphereMesh, FVector(40.0f, 10.0f, 92.0f), FVector(0.07f), FRotator::ZeroRotator, false)->SetMaterial(0, Eyes);
		AddPart(EnemySphereMesh, FVector(40.0f, -10.0f, 92.0f), FVector(0.07f), FRotator::ZeroRotator, false)->SetMaterial(0, Eyes);
	}
}

void AEchoDustWraith::UpdateLook(float DeltaSeconds)
{
	Super::UpdateLook(DeltaSeconds);
	// The swirl spins and the body bobs
	const float Time = GetWorld()->GetTimeSeconds();
	if (BodyParts.Num() >= 2)
	{
		BodyParts[0]->AddLocalRotation(FRotator(0.0f, 360.0f * DeltaSeconds, 0.0f));
		BodyParts[1]->AddLocalRotation(FRotator(0.0f, -500.0f * DeltaSeconds, 0.0f));
	}
	BodyRoot->SetRelativeLocation(FVector(0.0f, 0.0f, 8.0f * FMath::Sin(Time * 3.0f)));
}

// ------------------------------------------------------------------ Shade (Saraa only)

AEchoShade::AEchoShade(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Presence = EEchoPresence::SpiritOnly;
	DisplayName = NSLOCTEXT("EchoEnemy", "Shade", "Shade");
	BodyColor = FLinearColor(0.09f, 0.03f, 0.16f);
	TelegraphColor = FLinearColor(0.55f, 0.25f, 1.0f);
	Health->MaxHealth = 60.0f;
	MoveSpeed = 380.0f;
	AttackRange = 210.0f;
	AttackWindUp = 0.75f;
	AttackDamage = 12.0f;
	AttackRadius = 130.0f;
	AttackReach = 100.0f;
}

void AEchoShade::BuildBody()
{
	// A tall hooded shape: a long robe, a hood, pale eyes, and reaching arms
	AddPart(EnemyConeMesh, FVector(0.0f, 0.0f, -20.0f), FVector(0.85f, 0.85f, 1.5f));
	AddPart(EnemySphereMesh, FVector(5.0f, 0.0f, 70.0f), FVector(0.5f, 0.5f, 0.6f));
	AddPart(EnemyConeMesh, FVector(0.0f, 0.0f, 95.0f), FVector(0.45f, 0.45f, 0.35f));
	AddPart(EnemyCylinderMesh, FVector(30.0f, 35.0f, 30.0f), FVector(0.12f, 0.12f, 0.7f), FRotator(-65.0f, 15.0f, 0.0f));
	AddPart(EnemyCylinderMesh, FVector(30.0f, -35.0f, 30.0f), FVector(0.12f, 0.12f, 0.7f), FRotator(-65.0f, -15.0f, 0.0f));
	if (UMaterialInstanceDynamic* Eyes = MakeGlowMaterial(this, BodyMaterial.LoadSynchronous(), FLinearColor(0.75f, 0.9f, 1.0f), 0.6f))
	{
		AddPart(EnemySphereMesh, FVector(26.0f, 9.0f, 74.0f), FVector(0.07f), FRotator::ZeroRotator, false)->SetMaterial(0, Eyes);
		AddPart(EnemySphereMesh, FVector(26.0f, -9.0f, 74.0f), FVector(0.07f), FRotator::ZeroRotator, false)->SetMaterial(0, Eyes);
	}
}

void AEchoShade::UpdateLook(float DeltaSeconds)
{
	Super::UpdateLook(DeltaSeconds);
	// It floats, swaying
	const float Time = GetWorld()->GetTimeSeconds();
	BodyRoot->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f + 6.0f * FMath::Sin(Time * 2.2f)));
	BodyRoot->SetRelativeRotation(FRotator(0.0f, 0.0f, 4.0f * FMath::Sin(Time * 1.7f)));
}

// ------------------------------------------------------------------ Stone Guardian (Saraa first)

AEchoStoneGuardian::AEchoStoneGuardian(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Presence = EEchoPresence::Both;
	DisplayName = NSLOCTEXT("EchoEnemy", "StoneGuardian", "Stone Guardian");
	BodyColor = FLinearColor(0.26f, 0.24f, 0.22f);
	TelegraphColor = FLinearColor(1.0f, 0.35f, 0.05f);
	Health->MaxHealth = 160.0f;
	MoveSpeed = 200.0f;
	AttackRange = 260.0f;
	AttackWindUp = 1.3f;
	AttackStrikeTime = 0.3f;
	AttackRecover = 1.0f;
	AttackCooldown = 1.6f;
	AttackDamage = 25.0f;
	AttackRadius = 230.0f;
	AttackReach = 120.0f;
	bHeavyAttack = true;
	TelegraphSwell = 0.1f;
	CrackSound = EchoCombat::EngineSound(TEXT("VR_open"));
	ReformSound = EchoCombat::EngineSound(TEXT("VR_shep_scale_down_01"));
	GetCapsuleComponent()->InitCapsuleSize(75.0f, 120.0f);
	ArrowHitBox->InitCapsuleSize(85.0f, 125.0f);
}

bool AEchoStoneGuardian::IsOpen() const
{
	return OpenUntil > 0.0f && ServerNow() < OpenUntil;
}

float AEchoStoneGuardian::GetOpenFraction() const
{
	return IsOpen() ? FMath::Clamp((OpenUntil - ServerNow()) / FMath::Max(OpenUntil - OpenedAt, 0.1f), 0.0f, 1.0f) : 0.0f;
}

EEchoDamageResult AEchoStoneGuardian::FilterIncomingDamage(FEchoDamageInfo& Info)
{
	const EEchoDamageResult Base = Super::FilterIncomingDamage(Info);
	if (Base != EEchoDamageResult::Applied)
	{
		return Base;
	}
	if (Info.IsWhip())
	{
		// Saraa cracks the shell open (her whip never hurts it); another lash keeps it open
		const bool bWasClosed = !IsOpen();
		OpenedAt = ServerNow();
		OpenUntil = OpenedAt + OpenDuration;
		ForceNetUpdate();
		if (bWasClosed)
		{
			OnRep_OpenUntil();
		}
		UE_LOG(LogOurLastEcho, Log, TEXT("Combat: %s's shell cracked open for %.1f s"), *GetName(), OpenDuration);
		return EEchoDamageResult::Opened;
	}
	if (Info.IsArrow() && !IsOpen())
	{
		// Arrows bounce off the shell
		return EEchoDamageResult::Blocked;
	}
	return EEchoDamageResult::Applied;
}

void AEchoStoneGuardian::OnRep_OpenUntil()
{
	if (IsOpen() && !bWasOpen)
	{
		bWasOpen = true;
		PlayAt(this, CrackSound);
	}
}

void AEchoStoneGuardian::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bWasOpen && !IsOpen())
	{
		bWasOpen = false;
		PlayAt(this, ReformSound);
	}
}

float AEchoStoneGuardian::GetGlowAmount() const
{
	return Super::GetGlowAmount();
}

void AEchoStoneGuardian::BuildBody()
{
	// A glowing core inside a shell of stone slabs, with a blocky head and fists
	CoreMID = MakeGlowMaterial(this, BodyMaterial.LoadSynchronous(), FLinearColor(1.0f, 0.55f, 0.1f), 0.05f);
	Core = AddPart(EnemySphereMesh, FVector(0.0f, 0.0f, 10.0f), FVector(1.05f), FRotator::ZeroRotator, false);
	if (CoreMID)
	{
		Core->SetMaterial(0, CoreMID);
	}
	struct FSlab { FVector Location; FVector Scale; };
	const FSlab Slabs[] =
	{
		{ FVector( 62.0f,   0.0f,  10.0f), FVector(0.25f, 1.25f, 1.6f) },   // front
		{ FVector(-62.0f,   0.0f,  10.0f), FVector(0.25f, 1.25f, 1.6f) },   // back
		{ FVector(  0.0f,  62.0f,  10.0f), FVector(1.25f, 0.25f, 1.6f) },   // right
		{ FVector(  0.0f, -62.0f,  10.0f), FVector(1.25f, 0.25f, 1.6f) },   // left
		{ FVector(  0.0f,   0.0f,  92.0f), FVector(1.35f, 1.35f, 0.3f) },   // top
		{ FVector( 10.0f,   0.0f, 125.0f), FVector(0.6f, 0.6f, 0.5f) },     // head
	};
	for (const FSlab& Slab : Slabs)
	{
		ShellParts.Add(AddPart(EnemyCubeMesh, Slab.Location, Slab.Scale));
	}
	AddPart(EnemyCubeMesh, FVector(40.0f, 85.0f, -40.0f), FVector(0.5f, 0.45f, 0.6f));
	AddPart(EnemyCubeMesh, FVector(40.0f, -85.0f, -40.0f), FVector(0.5f, 0.45f, 0.6f));
}

void AEchoStoneGuardian::UpdateLook(float DeltaSeconds)
{
	Super::UpdateLook(DeltaSeconds);

	// Open: the slabs crack outwards and tilt, and the core blazes, fading as the shell is about to reform
	const float Open = GetOpenFraction();
	const float Crack = IsOpen() ? 1.0f : 0.0f;
	static const FVector Dirs[] = { FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0), FVector(0, 0, 1), FVector(0, 0, 1) };
	static const FVector Base[] = { FVector(62, 0, 10), FVector(-62, 0, 10), FVector(0, 62, 10), FVector(0, -62, 10), FVector(0, 0, 92), FVector(10, 0, 125) };
	for (int32 Index = 0; Index < ShellParts.Num() && Index < 6; ++Index)
	{
		ShellParts[Index]->SetRelativeLocation(Base[Index] + Dirs[Index] * 45.0f * Crack);
		ShellParts[Index]->SetRelativeRotation(FRotator(12.0f * Crack * (Index % 2 ? 1.0f : -1.0f), 0.0f, 10.0f * Crack));
	}
	if (CoreMID)
	{
		CoreMID->SetScalarParameterValue(TEXT("Glow"), IsOpen() ? 0.25f + 0.6f * Open : 0.05f);
	}
	if (IsOpen() && AttackPhase == EEchoAttackPhase::None)
	{
		Glow->SetLightColor(FLinearColor(1.0f, 0.6f, 0.15f));
		Glow->SetIntensity(20.0f + 60.0f * Open);
	}
}

void AEchoStoneGuardian::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEchoStoneGuardian, OpenUntil);
	DOREPLIFETIME(AEchoStoneGuardian, OpenedAt);
}

// ------------------------------------------------------------------ Sky Talon (Bat first)

AEchoSkyTalon::AEchoSkyTalon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Presence = EEchoPresence::Both;
	DisplayName = NSLOCTEXT("EchoEnemy", "SkyTalon", "Sky Talon");
	BodyColor = FLinearColor(0.08f, 0.38f, 0.72f);
	TelegraphColor = FLinearColor(0.75f, 0.95f, 1.0f);
	Health->MaxHealth = 70.0f;
	MoveSpeed = 650.0f;
	AttackRange = 900.0f;
	AttackWindUp = 0.9f;
	AttackStrikeTime = 0.6f;
	AttackRecover = 0.9f;
	AttackCooldown = 1.5f;
	AttackDamage = 15.0f;
	AttackRadius = 170.0f;
	SightRadius = 4500.0f;
	KnockdownSound = EchoCombat::EngineSound(TEXT("VR_grab"));
	GetCapsuleComponent()->InitCapsuleSize(48.0f, 48.0f);
	ArrowHitBox->InitCapsuleSize(70.0f, 70.0f);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->DefaultLandMovementMode = MOVE_Flying;
	Movement->MaxFlySpeed = MoveSpeed;
	Movement->BrakingDecelerationFlying = 2500.0f;
	Movement->MaxAcceleration = 3000.0f;
}

void AEchoSkyTalon::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		RefreshGround();
	}
}

bool AEchoSkyTalon::IsStunned() const
{
	return StunnedUntil > 0.0f && ServerNow() < StunnedUntil;
}

float AEchoSkyTalon::GetOpenFraction() const
{
	return IsStunned() ? FMath::Clamp((StunnedUntil - ServerNow()) / FMath::Max(StunnedUntil - StunnedAt, 0.1f), 0.0f, 1.0f) : 0.0f;
}

void AEchoSkyTalon::RefreshGround()
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SkyTalonGround), false, this);
	const FVector Start = GetActorLocation();
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.0f, 0.0f, 5000.0f), ECC_WorldStatic, Params))
	{
		GroundZ = Hit.ImpactPoint.Z;
	}
}

void AEchoSkyTalon::FlyTowards(const FVector& Point, float Scale)
{
	const FVector Offset = Point - GetActorLocation();
	const float Distance = Offset.Size();
	if (Distance > 10.0f)
	{
		AddMovementInput(Offset / Distance, Scale * FMath::Clamp(Distance / 300.0f, 0.2f, 1.0f));
	}
}

bool AEchoSkyTalon::ChaseTick(float DeltaTime)
{
	if (!Target || bDefeated)
	{
		return false;
	}
	// Circle in at cruising height, a few metres off the target, out of Saraa's reach
	const FVector TargetLocation = Target->GetActorLocation();
	FVector Away = (GetActorLocation() - TargetLocation).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = FVector::ForwardVector;
	}
	const float FeetZ = TargetLocation.Z - Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Desired = TargetLocation + Away * AttackRange * 0.6f + FVector(0.0f, 0.0f, FeetZ + HoverHeight - TargetLocation.Z);
	FlyTowards(Desired);
	return FVector::Dist2D(GetActorLocation(), TargetLocation) <= AttackRange && FMath::Abs(GetActorLocation().Z - (FeetZ + HoverHeight)) < 250.0f;
}

void AEchoSkyTalon::IdleTick(float DeltaTime)
{
	const FVector Location = GetActorLocation();
	FlyTowards(FVector(Location.X, Location.Y, GroundZ + HoverHeight), 0.6f);
}

bool AEchoSkyTalon::CanStartAttack() const
{
	return Super::CanStartAttack();
}

void AEchoSkyTalon::StartAttack()
{
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	SetPhase(EEchoAttackPhase::WindUp);
}

bool AEchoSkyTalon::AttackTick(float DeltaTime)
{
	const float Elapsed = ServerNow() - PhaseStartTime;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	switch (AttackPhase)
	{
	case EEchoAttackPhase::WindUp:
		// Hang in the air, screeching and glowing, facing the target
		if (Target)
		{
			const FVector ToTarget = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			if (!ToTarget.IsNearlyZero())
			{
				SetActorRotation(FMath::RInterpTo(GetActorRotation(), ToTarget.Rotation(), DeltaTime, 8.0f));
			}
		}
		Movement->Velocity *= 0.85f;
		if (Elapsed >= AttackWindUp)
		{
			// Dive at where the target is now (so moving or rolling away dodges it)
			DiveTarget = Target ? Target->GetActorLocation() - FVector(0.0f, 0.0f, 30.0f) : GetActorLocation() - FVector(0.0f, 0.0f, HoverHeight);
			bStruckThisDive = false;
			SetPhase(EEchoAttackPhase::Strike);
		}
		return false;
	case EEchoAttackPhase::Strike:
	{
		const FVector ToDive = DiveTarget - GetActorLocation();
		Movement->Velocity = ToDive.GetSafeNormal() * DiveSpeed;
		if (!bStruckThisDive)
		{
			PerformStrike();
		}
		if (ToDive.Size() < 90.0f || Elapsed >= AttackStrikeTime)
		{
			SetPhase(EEchoAttackPhase::Recover);
		}
		return false;
	}
	case EEchoAttackPhase::Recover:
	{
		// Climb back to cruising height
		const FVector Location = GetActorLocation();
		FlyTowards(FVector(Location.X, Location.Y, GroundZ + HoverHeight));
		if (Elapsed >= AttackRecover)
		{
			SetPhase(EEchoAttackPhase::None);
			LastAttackEndTime = GetWorld()->GetTimeSeconds();
			return true;
		}
		return false;
	}
	default:
		return true;
	}
}

void AEchoSkyTalon::PerformStrike()
{
	// The dive hits whoever it passes close to (once per dive)
	for (TActorIterator<AOurLastEchoCharacter> It(GetWorld()); It; ++It)
	{
		AOurLastEchoCharacter* Player = *It;
		if (!CanTarget(Player))
		{
			continue;
		}
		if (FVector::DistSquared(GetActorLocation(), Player->GetActorLocation()) <= FMath::Square(AttackRadius))
		{
			FEchoDamageInfo Info;
			Info.Amount = AttackDamage;
			Info.Kind = EEchoAttackKind::Enemy;
			Info.Instigator = this;
			Info.HitLocation = Player->GetActorLocation();
			Info.Direction = GetCharacterMovement()->Velocity.GetSafeNormal();
			Player->GetHealth()->ApplyDamage(Info);
			bStruckThisDive = true;
		}
	}
}

EEchoDamageResult AEchoSkyTalon::FilterIncomingDamage(FEchoDamageInfo& Info)
{
	const EEchoDamageResult Base = Super::FilterIncomingDamage(Info);
	if (Base != EEchoDamageResult::Applied)
	{
		return Base;
	}
	if (Info.IsArrow())
	{
		if (IsStunned())
		{
			// Already down: Bat can't hurt it, it's Saraa's turn
			return EEchoDamageResult::Blocked;
		}
		// Knocked out of the sky: it falls and lies stunned
		CancelAttack();
		StunnedAt = ServerNow();
		StunnedUntil = StunnedAt + StunDuration;
		bWasStunned = false;
		OnRep_StunnedUntil();
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		GetCharacterMovement()->Velocity = FVector(0.0f, 0.0f, -600.0f);
		if (AEchoEnemyController* Brain = Cast<AEchoEnemyController>(GetController()))
		{
			Brain->StopMovement();
		}
		ForceNetUpdate();
		UE_LOG(LogOurLastEcho, Log, TEXT("Combat: %s knocked down for %.1f s"), *GetName(), StunDuration);
		return EEchoDamageResult::Opened;
	}
	if (Info.IsWhip() && !IsStunned())
	{
		// Out of her reach in the air
		return EEchoDamageResult::Blocked;
	}
	return EEchoDamageResult::Applied;
}

void AEchoSkyTalon::OnRep_StunnedUntil()
{
	if (IsStunned() && !bWasStunned)
	{
		bWasStunned = true;
		PlayAt(this, KnockdownSound);
	}
}

bool AEchoSkyTalon::StunnedTick(float DeltaTime)
{
	return !IsStunned();
}

void AEchoSkyTalon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && !bDefeated)
	{
		GroundTimer -= DeltaSeconds;
		if (GroundTimer <= 0.0f && !IsStunned())
		{
			GroundTimer = 0.25f;
			RefreshGround();
		}
		// Stun over: back into the air
		if (StunnedUntil > 0.0f && !IsStunned())
		{
			StunnedUntil = 0.0f;
			bWasStunned = false;
			GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			GetCharacterMovement()->Velocity = FVector(0.0f, 0.0f, 300.0f);
			ForceNetUpdate();
		}
	}
	if (!IsStunned())
	{
		bWasStunned = false;
	}
}

FVector AEchoSkyTalon::GetHealthBarLocation() const
{
	return GetActorLocation() + FVector(0.0f, 0.0f, 90.0f);
}

float AEchoSkyTalon::GetGlowAmount() const
{
	return FMath::Max(Super::GetGlowAmount(), GetOpenFraction() * 0.8f);
}

void AEchoSkyTalon::BuildBody()
{
	// A sleek diving bird: a stretched body, a beak, two broad wings and a tail
	AddPart(EnemySphereMesh, FVector::ZeroVector, FVector(0.9f, 0.5f, 0.45f));
	AddPart(EnemyConeMesh, FVector(55.0f, 0.0f, 0.0f), FVector(0.2f, 0.2f, 0.4f), FRotator(-90.0f, 0.0f, 0.0f));
	Wings.Add(AddPart(EnemyCubeMesh, FVector(-5.0f, 70.0f, 5.0f), FVector(0.55f, 1.1f, 0.05f)));
	Wings.Add(AddPart(EnemyCubeMesh, FVector(-5.0f, -70.0f, 5.0f), FVector(0.55f, 1.1f, 0.05f)));
	AddPart(EnemyCubeMesh, FVector(-60.0f, 0.0f, 5.0f), FVector(0.4f, 0.35f, 0.04f), FRotator(0.0f, 0.0f, 0.0f));
	if (UMaterialInstanceDynamic* RingMID = MakeGlowMaterial(this, BodyMaterial.LoadSynchronous(), FLinearColor(1.0f, 0.95f, 0.5f), 0.5f))
	{
		// The stun ring: spins over its head while it's down
		StunRing = AddPart(EnemyCylinderMesh, FVector(0.0f, 0.0f, 70.0f), FVector(0.9f, 0.9f, 0.03f), FRotator::ZeroRotator, false);
		StunRing->SetMaterial(0, RingMID);
		StunRing->SetVisibility(false);
	}
}

void AEchoSkyTalon::UpdateLook(float DeltaSeconds)
{
	Super::UpdateLook(DeltaSeconds);

	const float Time = GetWorld()->GetTimeSeconds();
	const bool bStunned = IsStunned();
	// Wings flap in the air and droop when it's down
	const float Flap = bStunned ? -25.0f : 28.0f * FMath::Sin(Time * (AttackPhase == EEchoAttackPhase::WindUp ? 22.0f : 12.0f));
	if (Wings.Num() == 2)
	{
		Wings[0]->SetRelativeRotation(FRotator(0.0f, 0.0f, -Flap));
		Wings[1]->SetRelativeRotation(FRotator(0.0f, 0.0f, Flap));
	}
	if (StunRing)
	{
		StunRing->SetVisibility(bStunned && bShownLocally);
		StunRing->AddLocalRotation(FRotator(0.0f, 300.0f * DeltaSeconds, 0.0f));
		const float Fraction = GetOpenFraction();
		StunRing->SetRelativeScale3D(FVector(0.4f + 0.6f * Fraction, 0.4f + 0.6f * Fraction, 0.03f));
	}
}

void AEchoSkyTalon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEchoSkyTalon, StunnedUntil);
	DOREPLIFETIME(AEchoSkyTalon, StunnedAt);
}
