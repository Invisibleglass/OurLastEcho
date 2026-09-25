// Our Last Echo

#include "EchoTrainingDummy.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "OurLastEcho.h"

namespace
{
	UStaticMeshComponent* DummyPart(UStaticMeshComponent* Part, UStaticMesh* Mesh, USceneComponent* Parent, const FVector& Location, const FVector& Scale)
	{
		Part->SetStaticMesh(Mesh);
		Part->SetupAttachment(Parent);
		Part->SetRelativeLocation(Location);
		Part->SetRelativeScale3D(Scale);
		// Solid for both realms (BlockAll blocks the SpiritPawn channel by default) and for traces
		Part->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Part->SetCanEverAffectNavigation(false);
		return Part;
	}
}

AEchoTrainingDummy::AEchoTrainingDummy()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// The actor origin is the ground; a low base, a 1.6 m post, a cross-bar for arms and a head
	Base = DummyPart(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base")), CylinderMesh.Object, Root, FVector(0.0f, 0.0f, 5.0f), FVector(0.7f, 0.7f, 0.1f));
	Post = DummyPart(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Post")), CylinderMesh.Object, Pivot, FVector(0.0f, 0.0f, 90.0f), FVector(0.3f, 0.3f, 1.6f));
	Arms = DummyPart(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Arms")), CubeMesh.Object, Pivot, FVector(0.0f, 0.0f, 130.0f), FVector(0.15f, 1.1f, 0.15f));
	Head = DummyPart(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head")), SphereMesh.Object, Pivot, FVector(0.0f, 0.0f, 185.0f), FVector(0.4f, 0.4f, 0.4f));

	DummyMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Materials/MI_TrainingDummy.MI_TrainingDummy")));
	HitSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Engine/VREditor/Sounds/VR_close_Cue.VR_close_Cue")));
}

void AEchoTrainingDummy::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInterface* Material = DummyMaterial.LoadSynchronous())
	{
		for (UStaticMeshComponent* Part : { Base.Get(), Post.Get(), Arms.Get(), Head.Get() })
		{
			Part->SetMaterial(0, Material);
		}
	}
}

void AEchoTrainingDummy::ReceiveLash(AActor* Attacker, FVector Direction)
{
	if (!HasAuthority())
	{
		return;
	}

	++HitCount;
	LastHitDirection = Direction.GetSafeNormal2D(UE_SMALL_NUMBER, GetActorForwardVector());
	UE_LOG(LogOurLastEcho, Log, TEXT("%s lashed by %s (%d hits)"), *GetName(), *GetNameSafe(Attacker), HitCount);
	ForceNetUpdate();
	PlayHitReaction();
}

void AEchoTrainingDummy::OnRep_HitCount()
{
	PlayHitReaction();
}

void AEchoTrainingDummy::PlayHitReaction()
{
	++LocalHitReactions;
	WobbleRemaining = WobbleDuration;

	if (GetNetMode() != NM_DedicatedServer)
	{
		if (USoundBase* Sound = HitSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation() + FVector(0.0f, 0.0f, 130.0f), 1.0f, 0.7f);
		}
	}
}

void AEchoTrainingDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (WobbleRemaining <= 0.0f)
	{
		return;
	}

	// A damped rock away from the hit, about the base
	WobbleRemaining = FMath::Max(0.0f, WobbleRemaining - DeltaSeconds);
	const float T = 1.0f - WobbleRemaining / WobbleDuration;
	const float Angle = WobbleAngle * FMath::Sin(T * PI * 3.0f) * (1.0f - T);
	const FVector Axis = FVector::CrossProduct(FVector::UpVector, FVector(LastHitDirection)).GetSafeNormal();
	const FQuat Tilt(GetActorTransform().InverseTransformVectorNoScale(Axis), FMath::DegreesToRadians(Angle));
	Pivot->SetRelativeRotation(Tilt);
}

void AEchoTrainingDummy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEchoTrainingDummy, HitCount);
	DOREPLIFETIME(AEchoTrainingDummy, LastHitDirection);
}
