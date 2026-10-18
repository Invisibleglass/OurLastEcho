// Our Last Echo

#include "EchoArrow.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "EchoAnchorable.h"
#include "EchoHealthComponent.h"
#include "EchoPlatform.h"
#include "EchoSpiritBowComponent.h"
#include "OurLastEchoCharacter.h"
#include "EchoTypes.h"
#include "EchoVisibility.h"
#include "OurLastEcho.h"

namespace
{
	UStaticMeshComponent* MakeMeshPart(AActor* Owner, UStaticMeshComponent* Part, UStaticMesh* Mesh)
	{
		Part->SetStaticMesh(Mesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		Part->SetGenerateOverlapEvents(false);
		return Part;
	}
}

AEchoArrow::AEchoArrow()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	// Every machine flies the arrow itself from the replicated launch velocity; streaming positions would only add jitter
	SetReplicatingMovement(false);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(5.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_EchoArrow);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_SpiritPawn, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_EchoArrow, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Collision->SetGenerateOverlapEvents(false);
	Collision->SetCanEverAffectNavigation(false);
	// Hits carry the surface's physical material, so UEchoAnchorRules can switch to checking it later
	Collision->bReturnMaterialOnMove = true;
	RootComponent = Collision;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));

	// The engine cylinder and cone are 100 cm tall along Z; pitch -90 lays them along +X (the flight direction)
	Shaft = MakeMeshPart(this, CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shaft")), CylinderMesh.Object);
	Shaft->SetupAttachment(Collision);
	Shaft->SetRelativeLocation(FVector(-32.0f, 0.0f, 0.0f));
	Shaft->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	Shaft->SetRelativeScale3D(FVector(0.025f, 0.025f, 0.75f));

	Tip = MakeMeshPart(this, CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tip")), ConeMesh.Object);
	Tip->SetupAttachment(Collision);
	Tip->SetRelativeLocation(FVector(10.0f, 0.0f, 0.0f));
	Tip->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	Tip->SetRelativeScale3D(FVector(0.06f, 0.06f, 0.14f));

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Collision);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(8.0f);
	Glow->SetAttenuationRadius(450.0f);
	Glow->SetLightColor(FLinearColor(1.0f, 0.78f, 0.4f));
	Glow->SetCastShadows(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->InitialSpeed = 0.0f;
	Movement->MaxSpeed = 0.0f;
	// Zero until the launch arrives, so a client can't start falling before it knows the real arc
	Movement->ProjectileGravityScale = 0.0f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;

	ArrowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Materials/MI_Arrow.MI_Arrow")));
	TrailMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Materials/MI_ArrowTrail.MI_ArrowTrail")));
	PastArrowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Eras/MI_ArrowPast.MI_ArrowPast")));
	PastTrailMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Eras/MI_ArrowTrailPast.MI_ArrowTrailPast")));
	ReleaseSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Engine/VREditor/Sounds/VR_click2_Cue.VR_click2_Cue")));
	ImpactSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Engine/VREditor/Sounds/VR_click1_Cue.VR_click1_Cue")));
}

void AEchoArrow::BeginPlay()
{
	Super::BeginPlay();

	Movement->OnProjectileStop.AddDynamic(this, &AEchoArrow::OnArrowStop);

	// Each machine draws it in its own era's style
	EEchoEra ViewerEra = EEchoEra::Present;
	const bool bPastView = EchoVisibility::GetLocalViewerEra(GetWorld(), ViewerEra) && ViewerEra == EEchoEra::Past;
	UMaterialInterface* ArrowLook = bPastView && !PastArrowMaterial.IsNull() ? PastArrowMaterial.LoadSynchronous() : ArrowMaterial.LoadSynchronous();
	if (ArrowLook)
	{
		Shaft->SetMaterial(0, ArrowLook);
		Tip->SetMaterial(0, ArrowLook);
	}

	if (HasAuthority())
	{
		SetLifeSpan(FlightLifetime);
	}

	// A charged arrow is bigger and blazes; a quick one is slimmer
	if (ShotKind == EEchoAttackKind::ChargedShot)
	{
		Shaft->SetRelativeScale3D(Shaft->GetRelativeScale3D() * FVector(1.8f, 1.8f, 1.15f));
		Tip->SetRelativeScale3D(Tip->GetRelativeScale3D() * 1.7f);
		Glow->SetIntensity(Glow->Intensity * 3.0f);
	}
	else if (ShotKind == EEchoAttackKind::QuickShot)
	{
		Shaft->SetRelativeScale3D(Shaft->GetRelativeScale3D() * FVector(0.8f, 0.8f, 0.85f));
		Glow->SetIntensity(Glow->Intensity * 0.6f);
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		UMaterialInterface* Material = bPastView && !PastTrailMaterial.IsNull() ? PastTrailMaterial.LoadSynchronous() : TrailMaterial.LoadSynchronous();
		for (int32 Index = 0; Index < TrailLength; ++Index)
		{
			UStaticMeshComponent* Segment = NewObject<UStaticMeshComponent>(this);
			MakeMeshPart(this, Segment, CylinderMesh);
			Segment->SetUsingAbsoluteLocation(true);
			Segment->SetUsingAbsoluteRotation(true);
			Segment->SetUsingAbsoluteScale(true);
			Segment->SetupAttachment(Collision);
			Segment->SetVisibility(false);
			if (Material)
			{
				Segment->SetMaterial(0, Material);
			}
			Segment->RegisterComponent();
			TrailSegments.Add(Segment);
		}

		if (USoundBase* Sound = ReleaseSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), 0.6f, 1.4f);
		}
	}
}

void AEchoArrow::Launch(const FVector& Velocity, float GravityScale)
{
	check(HasAuthority());

	LaunchVelocity = Velocity;
	LaunchGravityScale = GravityScale;
	SetActorRotation(Velocity.Rotation());
	OnRep_Launch();
}

void AEchoArrow::OnRep_Launch()
{
	if (bStuck)
	{
		return;
	}

	Movement->ProjectileGravityScale = LaunchGravityScale;
	Movement->Velocity = LaunchVelocity;
	Movement->UpdateComponentVelocity();
}

void AEchoArrow::SetShot(EEchoAttackKind Kind, float Damage, bool bHeavy)
{
	ShotKind = Kind;
	ShotDamage = Damage;
	bShotHeavy = bHeavy;
}

void AEchoArrow::OnArrowStop(const FHitResult& ImpactResult)
{
	const FVector Direction = GetActorForwardVector();
	Stick();

	// An arrow that bounced off a shell just drops; it doesn't wake platforms or make anchors
	if (bDeflected)
	{
		if (HasAuthority())
		{
			bStuck = true;
			SetLifeSpan(1.0f);
		}
		return;
	}

	// Hit an enemy (Milestone 8): it doesn't stay stuck in mid-air; the server decides what the hit did
	AActor* HitActor = ImpactResult.GetActor();
	if (HitActor && HitActor != GetInstigator() && UEchoHealthComponent::FindOn(HitActor))
	{
		Shaft->SetVisibility(false);
		Tip->SetVisibility(false);
		if (HasAuthority())
		{
			bStuck = true;
			HitHealthActor(HitActor, ImpactResult, Direction);
		}
		return;
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		if (USoundBase* Sound = ImpactSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, ImpactResult.ImpactPoint, 0.8f);
		}
	}

	if (!HasAuthority())
	{
		return;
	}

	bStuck = true;
	SetLifeSpan(StuckLifetime);

	if (AEchoPlatform* Platform = Cast<AEchoPlatform>(ImpactResult.GetActor()))
	{
		UE_LOG(LogOurLastEcho, Log, TEXT("%s hit %s"), *GetName(), *Platform->GetName());
		Platform->Awaken();
	}
	else if (UEchoAnchorRules::CanHoldAnchor(ImpactResult))
	{
		// The arrow sticks and becomes an anchor point; the anchor shows the stuck arrow from now on
		const AOurLastEchoCharacter* Archer = Cast<AOurLastEchoCharacter>(GetInstigator());
		if (UEchoSpiritBowComponent* Bow = Archer ? Archer->GetSpiritBow() : nullptr)
		{
			if (Bow->CreateAnchor(ImpactResult, Direction))
			{
				SetLifeSpan(AnchoredLifetime);
			}
		}
	}
}

void AEchoArrow::HitHealthActor(AActor* Target, const FHitResult& Impact, const FVector& Direction)
{
	UEchoHealthComponent* Health = UEchoHealthComponent::FindOn(Target);
	FEchoDamageInfo Info;
	Info.Amount = ShotDamage;
	Info.Kind = ShotKind;
	Info.bHeavy = bShotHeavy;
	Info.Instigator = GetInstigator();
	Info.HitLocation = Impact.ImpactPoint;
	Info.Direction = Direction;
	const EEchoDamageResult Result = Health->ApplyDamage(Info);
	UE_LOG(LogOurLastEcho, Log, TEXT("%s (%s) hit %s: result %d"), *GetName(), *UEnum::GetValueAsString(ShotKind), *Target->GetName(), static_cast<int32>(Result));

	if (Result == EEchoDamageResult::Blocked)
	{
		// Bounces off (a Stone Guardian's shell): back and up, falling
		bStuck = false;
		bDeflected = true;
		DeflectVelocity = (-Direction * 700.0f + FVector(FMath::FRandRange(-250.0f, 250.0f), FMath::FRandRange(-250.0f, 250.0f), 450.0f));
		OnRep_Deflected();
		SetLifeSpan(1.6f);
	}
	else
	{
		SetLifeSpan(0.1f);
	}
	ForceNetUpdate();
}

void AEchoArrow::OnRep_Deflected()
{
	if (!bDeflected)
	{
		return;
	}
	Shaft->SetVisibility(true);
	Tip->SetVisibility(true);
	// Don't catch the same hit box again on the way out
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	Movement->SetActive(true);
	Movement->ProjectileGravityScale = 1.0f;
	Movement->Velocity = DeflectVelocity;
	Movement->UpdateComponentVelocity();
}

void AEchoArrow::OnRep_Stuck()
{
	if (bStuck)
	{
		Stick();
	}
}

void AEchoArrow::Stick()
{
	Movement->StopMovementImmediately();
	Movement->ProjectileGravityScale = 0.0f;
	Movement->SetActive(false);
	Glow->SetIntensity(Glow->Intensity * 0.4f);
}

void AEchoArrow::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (TrailSegments.Num() > 0)
	{
		UpdateTrail(DeltaSeconds);
	}
}

void AEchoArrow::UpdateTrail(float DeltaSeconds)
{
	TrailTimer += DeltaSeconds;
	if (TrailTimer >= TrailInterval || TrailPoints.Num() == 0)
	{
		TrailTimer = 0.0f;
		// Newest point first; once stuck, the tail keeps collapsing into the arrow and disappears
		TrailPoints.Insert(GetActorLocation(), 0);
		if (TrailPoints.Num() > TrailSegments.Num() + 1)
		{
			TrailPoints.SetNum(TrailSegments.Num() + 1);
		}
	}

	for (int32 Index = 0; Index < TrailSegments.Num(); ++Index)
	{
		UStaticMeshComponent* Segment = TrailSegments[Index];
		if (Index + 1 >= TrailPoints.Num())
		{
			Segment->SetVisibility(false);
			continue;
		}

		// The first segment runs from the arrow's current position, so the streak is attached to it
		const FVector Head = Index == 0 ? GetActorLocation() : TrailPoints[Index];
		const FVector Tail = TrailPoints[Index + 1];
		const FVector Span = Head - Tail;
		const float Length = Span.Size();
		if (Length < 1.0f)
		{
			Segment->SetVisibility(false);
			continue;
		}

		// Thin and tapering: 3 cm wide at the arrow down to almost nothing at the end
		const float Width = 0.03f * (1.0f - float(Index) / TrailSegments.Num());
		Segment->SetWorldLocationAndRotation((Head + Tail) * 0.5f, FRotationMatrix::MakeFromZ(Span / Length).Rotator());
		Segment->SetWorldScale3D(FVector(Width, Width, Length / 100.0f));
		Segment->SetVisibility(true);
	}
}

void AEchoArrow::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AEchoArrow, LaunchVelocity, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AEchoArrow, LaunchGravityScale, COND_InitialOnly);
	DOREPLIFETIME(AEchoArrow, bStuck);
	DOREPLIFETIME_CONDITION(AEchoArrow, ShotKind, COND_InitialOnly);
	DOREPLIFETIME(AEchoArrow, DeflectVelocity);
	DOREPLIFETIME(AEchoArrow, bDeflected);
}
