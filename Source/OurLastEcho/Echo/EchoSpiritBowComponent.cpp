// Our Last Echo

#include "EchoSpiritBowComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "EchoAnchorPoint.h"
#include "EchoArrow.h"
#include "EchoTypes.h"
#include "OurLastEcho.h"
#include "OurLastEchoCharacter.h"

namespace
{
	/** The server accepts a shot slightly early, to allow for network jitter between the client's and server's clocks */
	constexpr float ServerCooldownTolerance = 0.8f;

	struct FBowPart
	{
		const TCHAR* Mesh;
		FVector Location;
		FRotator Rotation;
		FVector Scale;
	};

	/**
	 *  Placeholder bow from engine shapes, in the bow's own space: Z = along the bow, -X = towards the archer.
	 *  Two limbs angled back from a grip, joined by a thin string.
	 */
	const FBowPart BowShape[] =
	{
		{ TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(-4.0f, 0.0f,  30.0f), FRotator(-14.0f, 0.0f, 0.0f), FVector(0.035f, 0.035f, 0.60f) },   // upper limb
		{ TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(-4.0f, 0.0f, -30.0f), FRotator( 14.0f, 0.0f, 0.0f), FVector(0.035f, 0.035f, 0.60f) },   // lower limb
		{ TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector( 0.0f, 0.0f,   0.0f), FRotator::ZeroRotator,        FVector(0.055f, 0.055f, 0.16f) },   // grip
		{ TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(-12.5f, 0.0f,  0.0f), FRotator::ZeroRotator,        FVector(0.006f, 0.006f, 1.15f) },   // string
		{ TEXT("/Engine/BasicShapes/Sphere.Sphere"),     FVector(-11.0f, 0.0f,  58.0f), FRotator::ZeroRotator,       FVector(0.05f, 0.05f, 0.05f) },     // upper tip
		{ TEXT("/Engine/BasicShapes/Sphere.Sphere"),     FVector(-11.0f, 0.0f, -58.0f), FRotator::ZeroRotator,       FVector(0.05f, 0.05f, 0.05f) },     // lower tip
	};

}

UEchoSpiritBowComponent::UEchoSpiritBowComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	ArrowClass = AEchoArrow::StaticClass();
	AnchorClass = AEchoAnchorPoint::StaticClass();

	AimAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_Aim.IA_Aim")));
	FireAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_Fire.IA_Fire")));
	BowMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Input/IMC_Bow.IMC_Bow")));
	BowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Materials/MI_Bow.MI_Bow")));
}

AOurLastEchoCharacter* UEchoSpiritBowComponent::GetCharacter() const
{
	return Cast<AOurLastEchoCharacter>(GetOwner());
}

bool UEchoSpiritBowComponent::CanUseBow() const
{
	const AOurLastEchoCharacter* Character = GetCharacter();
	return Character && Character->GetRealm() == EEchoRealm::Living;
}

void UEchoSpiritBowComponent::BeginPlay()
{
	Super::BeginPlay();

	AOurLastEchoCharacter* Character = GetCharacter();
	if (!Character || !CanUseBow())
	{
		SetComponentTickEnabled(false);
		return;
	}

	DefaultArmLength = Character->GetCameraBoom()->TargetArmLength;
	DefaultSocketOffset = Character->GetCameraBoom()->SocketOffset;
	DefaultFieldOfView = Character->GetFollowCamera()->FieldOfView;
	DefaultWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
	bDefaultsCaptured = true;

	if (GetNetMode() != NM_DedicatedServer)
	{
		BuildBowMesh();
	}
	ApplyAimState();
}

void UEchoSpiritBowComponent::BuildBowMesh()
{
	AOurLastEchoCharacter* Character = GetCharacter();

	BowRoot = NewObject<USceneComponent>(Character, TEXT("SpiritBowRoot"));
	BowRoot->SetupAttachment(Character->GetRootComponent());
	BowRoot->RegisterComponent();

	UMaterialInterface* Material = BowMaterial.LoadSynchronous();
	for (const FBowPart& Part : BowShape)
	{
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Character);
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Part.Mesh));
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetRelativeTransform(FTransform(Part.Rotation, Part.Location, Part.Scale));
		if (Material)
		{
			Mesh->SetMaterial(0, Material);
		}
		Mesh->SetupAttachment(BowRoot);
		Mesh->RegisterComponent();
		BowParts.Add(Mesh);
	}
}

void UEchoSpiritBowComponent::SetupPlayerInput(UEnhancedInputComponent* Input)
{
	if (!Input || !CanUseBow())
	{
		return;
	}

	UInputAction* Aim = AimAction.LoadSynchronous();
	UInputAction* Shoot = FireAction.LoadSynchronous();
	if (!Aim || !Shoot)
	{
		UE_LOG(LogOurLastEcho, Warning, TEXT("Spirit bow: input actions missing (run Scripts/build_spirit_bow.py)"));
		return;
	}

	Input->BindAction(Aim, ETriggerEvent::Started, this, &UEchoSpiritBowComponent::OnAimStarted);
	Input->BindAction(Aim, ETriggerEvent::Completed, this, &UEchoSpiritBowComponent::OnAimCompleted);
	Input->BindAction(Aim, ETriggerEvent::Canceled, this, &UEchoSpiritBowComponent::OnAimCompleted);
	Input->BindAction(Shoot, ETriggerEvent::Started, this, &UEchoSpiritBowComponent::OnFirePressed);

	const APawn* Pawn = GetCharacter();
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr)
	{
		if (UInputMappingContext* Context = BowMappingContext.LoadSynchronous())
		{
			Subsystem->AddMappingContext(Context, MappingPriority);
		}
	}
}

void UEchoSpiritBowComponent::OnAimStarted()
{
	SetAiming(true);
}

void UEchoSpiritBowComponent::OnAimCompleted()
{
	SetAiming(false);
}

void UEchoSpiritBowComponent::OnFirePressed()
{
	Fire();
}

void UEchoSpiritBowComponent::SetAiming(bool bNewAiming)
{
	if (!CanUseBow() || bAiming == bNewAiming)
	{
		return;
	}

	bAiming = bNewAiming;
	ApplyAimState();

	if (!GetOwner()->HasAuthority())
	{
		ServerSetAiming(bNewAiming);
	}
}

void UEchoSpiritBowComponent::ServerSetAiming_Implementation(bool bNewAiming)
{
	if (CanUseBow() && bAiming != bNewAiming)
	{
		bAiming = bNewAiming;
		ApplyAimState();
	}
}

void UEchoSpiritBowComponent::OnRep_Aiming()
{
	ApplyAimState();
}

void UEchoSpiritBowComponent::ApplyAimState()
{
	AOurLastEchoCharacter* Character = GetCharacter();
	if (!Character || !bDefaultsCaptured)
	{
		return;
	}

	// Aiming: face where the camera looks and walk slower. Set on the server and the owner alike so movement prediction agrees
	Character->bUseControllerRotationYaw = bAiming;
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Movement->bOrientRotationToMovement = !bAiming;
	Movement->MaxWalkSpeed = bAiming ? AimWalkSpeed : DefaultWalkSpeed;

	if (BowRoot)
	{
		BowRoot->SetRelativeTransform(bAiming ? AimPlacement : BackPlacement);
	}
}

void UEchoSpiritBowComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AOurLastEchoCharacter* Character = GetCharacter();
	if (!Character || !Character->IsLocallyControlled() || !bDefaultsCaptured)
	{
		return;
	}

	// Blend the camera over the shoulder and back (local only: cameras don't replicate)
	USpringArmComponent* Boom = Character->GetCameraBoom();
	UCameraComponent* Camera = Character->GetFollowCamera();
	Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, bAiming ? AimCameraDistance : DefaultArmLength, DeltaTime, AimBlendSpeed);
	Boom->SocketOffset = FMath::VInterpTo(Boom->SocketOffset, bAiming ? AimCameraOffset : DefaultSocketOffset, DeltaTime, AimBlendSpeed);
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, bAiming ? AimFieldOfView : DefaultFieldOfView, DeltaTime, AimBlendSpeed));
}

float UEchoSpiritBowComponent::GetCooldownReadiness() const
{
	if (Cooldown <= 0.0f)
	{
		return 1.0f;
	}
	return FMath::Clamp((GetWorld()->GetTimeSeconds() - LastFireTime) / Cooldown, 0.0f, 1.0f);
}

bool UEchoSpiritBowComponent::Fire()
{
	AOurLastEchoCharacter* Character = GetCharacter();
	if (!Character || !CanUseBow() || !bAiming || GetCooldownReadiness() < 1.0f)
	{
		return false;
	}

	// Aim through the camera: whatever the reticle is on, up to Range away
	FVector ViewLocation;
	FRotator ViewRotation;
	if (const APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else
	{
		ViewLocation = Character->GetFollowCamera()->GetComponentLocation();
		ViewRotation = Character->GetFollowCamera()->GetComponentRotation();
	}

	const FVector Direction = ViewRotation.Vector();
	// Start level with the character, so nothing between the camera and Bat can catch the trace
	const FVector TraceStart = ViewLocation + Direction * FMath::Max(0.0f, FVector::DotProduct(Character->GetActorLocation() - ViewLocation, Direction));
	const FVector TraceEnd = TraceStart + Direction * Range;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SpiritBowAim), false, Character);
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_EchoArrow, Params);
	const FVector Target = bHit ? Hit.ImpactPoint : TraceEnd;

	LastFireTime = GetWorld()->GetTimeSeconds();

	if (Character->HasAuthority())
	{
		FireAt(Target);
	}
	else
	{
		ServerFire(Target);
	}
	return true;
}

void UEchoSpiritBowComponent::ServerFire_Implementation(FVector_NetQuantize TargetPoint)
{
	const float SinceLast = GetWorld()->GetTimeSeconds() - LastFireTime;
	if (!bAiming || SinceLast < Cooldown * ServerCooldownTolerance)
	{
		return;
	}
	FireAt(TargetPoint);
}

AEchoArrow* UEchoSpiritBowComponent::FireAt(FVector TargetPoint)
{
	AOurLastEchoCharacter* Character = GetCharacter();
	if (!Character || !Character->HasAuthority() || !CanUseBow() || !ArrowClass)
	{
		return nullptr;
	}

	// Leave from in front of the chest, facing where the player is looking
	const FRotator Facing(0.0f, Character->GetControlRotation().Yaw, 0.0f);
	const FVector Start = Character->GetActorLocation() + Facing.RotateVector(MuzzleOffset);

	// Correct the launch angle for the arc so the arrow still lands where the reticle was
	FVector Velocity = (TargetPoint - Start).GetSafeNormal() * ArrowSpeed;
	if (ArrowGravityScale > KINDA_SMALL_NUMBER)
	{
		const float GravityZ = GetWorld()->GetGravityZ() * ArrowGravityScale;
		UGameplayStatics::FSuggestProjectileVelocityParameters Params(this, Start, TargetPoint, ArrowSpeed);
		Params.OverrideGravityZ = GravityZ;
		Params.TraceOption = ESuggestProjVelocityTraceOption::DoNotTrace;
		FVector Suggested;
		if (UGameplayStatics::SuggestProjectileVelocity(Params, Suggested))
		{
			Velocity = Suggested;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEchoArrow* Arrow = GetWorld()->SpawnActor<AEchoArrow>(ArrowClass, Start, Velocity.Rotation(), SpawnParams);
	if (Arrow)
	{
		Arrow->Launch(Velocity, ArrowGravityScale);
	}

	LastFireTime = GetWorld()->GetTimeSeconds();
	return Arrow;
}

AEchoAnchorPoint* UEchoSpiritBowComponent::CreateAnchor(const FHitResult& Hit, const FVector& ArrowDirection)
{
	AOurLastEchoCharacter* Character = GetCharacter();
	if (!Character || !Character->HasAuthority() || !AnchorClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEchoAnchorPoint* Anchor = GetWorld()->SpawnActor<AEchoAnchorPoint>(AnchorClass, Hit.ImpactPoint, ArrowDirection.Rotation(), SpawnParams);
	if (!Anchor)
	{
		return nullptr;
	}
	Anchor->InitSurface(Hit.ImpactNormal);

	// Oldest first: past the limit, the oldest anchor goes (and Saraa lets go if she was hanging from it)
	ActiveAnchors.RemoveAll([](const AEchoAnchorPoint* Existing) { return !IsValid(Existing); });
	ActiveAnchors.Add(Anchor);
	while (ActiveAnchors.Num() > FMath::Max(1, MaxActiveAnchors))
	{
		AEchoAnchorPoint* Oldest = ActiveAnchors[0];
		ActiveAnchors.RemoveAt(0);
		UE_LOG(LogOurLastEcho, Log, TEXT("Spirit bow: too many anchors, removing the oldest (%s)"), *GetNameSafe(Oldest));
		if (Oldest)
		{
			Oldest->Destroy();
		}
	}

	UE_LOG(LogOurLastEcho, Log, TEXT("Spirit bow: anchor %s on %s (%d active)"), *Anchor->GetName(), *GetNameSafe(Hit.GetActor()), ActiveAnchors.Num());
	return Anchor;
}

void UEchoSpiritBowComponent::ClearAnchors()
{
	for (AEchoAnchorPoint* Anchor : ActiveAnchors)
	{
		if (IsValid(Anchor))
		{
			Anchor->Destroy();
		}
	}
	ActiveAnchors.Reset();
}

TArray<AEchoAnchorPoint*> UEchoSpiritBowComponent::GetActiveAnchors() const
{
	TArray<AEchoAnchorPoint*> Result;
	for (AEchoAnchorPoint* Anchor : ActiveAnchors)
	{
		if (IsValid(Anchor))
		{
			Result.Add(Anchor);
		}
	}
	return Result;
}

void UEchoSpiritBowComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UEchoSpiritBowComponent, bAiming, COND_SkipOwner);
}
