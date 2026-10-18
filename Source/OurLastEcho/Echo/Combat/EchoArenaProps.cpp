// Our Last Echo

#include "EchoArenaProps.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "EchoArenaManager.h"
#include "EchoGameMode.h"
#include "EchoTypes.h"
#include "OurLastEcho.h"
#include "OurLastEchoCharacter.h"

namespace
{
	UStaticMeshComponent* MakeProp(AActor* Owner, const FName Name, USceneComponent* Parent, UStaticMesh* Mesh, bool bSolid,
		const FVector& Location, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* Part = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent);
		Part->SetStaticMesh(Mesh);
		Part->SetCanEverAffectNavigation(false);
		Part->SetGenerateOverlapEvents(false);
		if (bSolid)
		{
			// Both players bump into it; arrows stop on it
			Part->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		}
		else
		{
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Part->SetRelativeTransform(FTransform(Rotation, Location, Scale));
		return Part;
	}

	UBoxComponent* MakePlayerTrigger(AActor* Owner, USceneComponent* Parent, const FVector& Location, const FVector& Extent)
	{
		UBoxComponent* Trigger = Owner->CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
		Trigger->SetupAttachment(Parent);
		Trigger->SetRelativeLocation(Location);
		Trigger->InitBoxExtent(Extent);
		Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
		// Both players: Bat's channel and Saraa's
		Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Trigger->SetCollisionResponseToChannel(ECC_SpiritPawn, ECR_Overlap);
		Trigger->SetGenerateOverlapEvents(true);
		Trigger->SetCanEverAffectNavigation(false);
		return Trigger;
	}

	void ApplyMaterial(const TSoftObjectPtr<UMaterialInterface>& Material, std::initializer_list<UStaticMeshComponent*> Parts)
	{
		if (UMaterialInterface* Loaded = Material.LoadSynchronous())
		{
			for (UStaticMeshComponent* Part : Parts)
			{
				Part->SetMaterial(0, Loaded);
			}
		}
	}
}

// ------------------------------------------------------------------ travel door

AEchoTravelDoor::AEchoTravelDoor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// A frame 380 wide x 440 tall. The actor origin is the floor at the middle of the doorway; it faces +X
	LeftPost = MakeProp(this, TEXT("LeftPost"), Root, CubeMesh.Object, true, FVector(0.0f, -190.0f, 200.0f), FVector(0.4f, 0.4f, 4.0f));
	RightPost = MakeProp(this, TEXT("RightPost"), Root, CubeMesh.Object, true, FVector(0.0f, 190.0f, 200.0f), FVector(0.4f, 0.4f, 4.0f));
	Lintel = MakeProp(this, TEXT("Lintel"), Root, CubeMesh.Object, true, FVector(0.0f, 0.0f, 420.0f), FVector(0.4f, 4.2f, 0.4f));
	// The engine plane lies flat facing +Z: roll it upright to face +X
	Portal = MakeProp(this, TEXT("Portal"), Root, PlaneMesh.Object, false, FVector(0.0f, 0.0f, 200.0f), FVector(4.0f, 3.4f, 1.0f), FRotator(90.0f, 0.0f, 0.0f));
	Portal->SetCastShadow(false);

	LabelText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelText->SetupAttachment(Root);
	LabelText->SetRelativeLocation(FVector(30.0f, 0.0f, 480.0f));
	LabelText->SetHorizontalAlignment(EHTA_Center);
	LabelText->SetVerticalAlignment(EVRTA_TextCenter);
	LabelText->SetWorldSize(60.0f);
	LabelText->SetTextRenderColor(FColor(255, 200, 120));

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(120.0f, 0.0f, 220.0f));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(60.0f);
	Light->SetAttenuationRadius(900.0f);
	Light->SetCastShadows(false);

	Trigger = MakePlayerTrigger(this, Root, FVector(0.0f, 0.0f, 150.0f), FVector(60.0f, 150.0f, 150.0f));

	Label = NSLOCTEXT("EchoDoor", "CombatTest", "Combat Test");
	OpenSound = EchoCombat::EngineSound(TEXT("VR_teleport"));
	FrameMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Combat/MI_DoorFrame.MI_DoorFrame")));
	PortalMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Combat/MI_DoorPortal.MI_DoorPortal")));
}

void AEchoTravelDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	LabelText->SetText(Label);
	Light->SetLightColor(GlowColor);
	ApplyMaterial(FrameMaterial, { LeftPost, RightPost, Lintel });
	ApplyMaterial(PortalMaterial, { Portal });
}

void AEchoTravelDoor::BeginPlay()
{
	Super::BeginPlay();
	LabelText->SetText(Label);
	ArmedAt = GetWorld()->GetTimeSeconds() + ArmDelay;
	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &AEchoTravelDoor::OnEnter);
	}
}

void AEchoTravelDoor::OnEnter(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Cast<AOurLastEchoCharacter>(OtherActor))
	{
		Open(OtherActor);
	}
}

bool AEchoTravelDoor::Open(AActor* OpenedBy)
{
	if (!HasAuthority() || GetWorld()->GetTimeSeconds() < ArmedAt || TargetMap.IsEmpty())
	{
		return false;
	}
	AEchoGameMode* GameMode = GetWorld()->GetAuthGameMode<AEchoGameMode>();
	if (!GameMode)
	{
		return false;
	}
	UE_LOG(LogOurLastEcho, Log, TEXT("Door %s opened by %s: both players to %s"), *GetName(), *GetNameSafe(OpenedBy), *TargetMap);
	if (OpenSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, OpenSound, GetActorLocation());
	}
	return GameMode->TravelBothTo(TargetMap, ArrivalTag);
}

// ------------------------------------------------------------------ restart pad

AEchoArenaRestartPad::AEchoArenaRestartPad()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	USceneComponent* PadRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = PadRoot;

	// A low plinth (the actor origin is the floor) with a glowing button on top
	Base = MakeProp(this, TEXT("Base"), PadRoot, CylinderMesh.Object, true, FVector(0.0f, 0.0f, 10.0f), FVector(1.6f, 1.6f, 0.2f));
	Button = MakeProp(this, TEXT("Button"), PadRoot, CylinderMesh.Object, false, FVector(0.0f, 0.0f, 25.0f), FVector(0.9f, 0.9f, 0.1f));

	LabelText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelText->SetupAttachment(PadRoot);
	LabelText->SetRelativeLocation(FVector(0.0f, 0.0f, 180.0f));
	LabelText->SetHorizontalAlignment(EHTA_Center);
	LabelText->SetVerticalAlignment(EVRTA_TextCenter);
	LabelText->SetWorldSize(40.0f);
	LabelText->SetText(NSLOCTEXT("EchoArena", "RestartPad", "Restart arena"));
	LabelText->SetTextRenderColor(FColor(255, 120, 100));

	Trigger = MakePlayerTrigger(this, PadRoot, FVector(0.0f, 0.0f, 80.0f), FVector(70.0f, 70.0f, 60.0f));
	PressSound = EchoCombat::EngineSound(TEXT("VR_confirm"));

	FrameMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Combat/MI_DoorFrame.MI_DoorFrame")));
	ButtonMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Combat/MI_RestartButton.MI_RestartButton")));
}

void AEchoArenaRestartPad::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMaterial(FrameMaterial, { Base });
	ApplyMaterial(ButtonMaterial, { Button });
}

void AEchoArenaRestartPad::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &AEchoArenaRestartPad::OnEnter);
	}
}

void AEchoArenaRestartPad::OnEnter(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!Cast<AOurLastEchoCharacter>(OtherActor) || GetWorld()->GetTimeSeconds() - LastPressed < Cooldown)
	{
		return;
	}
	if (AEchoArenaManager* Arena = AEchoArenaManager::Get(GetWorld()))
	{
		LastPressed = GetWorld()->GetTimeSeconds();
		++PressCount;
		UE_LOG(LogOurLastEcho, Log, TEXT("Restart pad pressed by %s"), *GetNameSafe(OtherActor));
		MulticastPressed();
		Arena->RestartArena();
	}
}

void AEchoArenaRestartPad::MulticastPressed_Implementation()
{
	if (PressSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PressSound, GetActorLocation());
	}
}
