// Our Last Echo

#include "EchoEndZone.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "EchoGameState.h"
#include "EchoTypes.h"
#include "OurLastEchoCharacter.h"

AEchoEndZone::AEchoEndZone()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	Zone->SetupAttachment(RootComponent);
	Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Zone->SetCollisionResponseToAllChannels(ECR_Ignore);
	Zone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Zone->SetCollisionResponseToChannel(ECC_SpiritPawn, ECR_Overlap);
	Zone->SetGenerateOverlapEvents(true);
	Zone->OnComponentBeginOverlap.AddDynamic(this, &AEchoEndZone::OnZoneBeginOverlap);

	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	Marker->SetupAttachment(RootComponent);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Marker->SetStaticMesh(CubeMesh.Object);
	}

	MarkerMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Materials/MI_EndZone.MI_EndZone")));
}

void AEchoEndZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Zone->SetBoxExtent(ZoneSize * 0.5f);
	Zone->SetRelativeLocation(FVector(0.0f, 0.0f, ZoneSize.Z * 0.5f));

	// 2cm-thick plate resting just on top of the floor
	Marker->SetRelativeScale3D(FVector(ZoneSize.X / 100.0f, ZoneSize.Y / 100.0f, 0.02f));
	Marker->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));

	if (UMaterialInterface* Material = MarkerMaterial.LoadSynchronous())
	{
		Marker->SetMaterial(0, Material);
	}
}

void AEchoEndZone::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	EvaluateOccupants();
}

void AEchoEndZone::EvaluateOccupants()
{
	if (!HasAuthority())
	{
		return;
	}

	AEchoGameState* GameState = GetWorld()->GetGameState<AEchoGameState>();
	if (!GameState || GameState->IsMilestoneComplete())
	{
		return;
	}

	TArray<AActor*> Occupants;
	Zone->GetOverlappingActors(Occupants, AOurLastEchoCharacter::StaticClass());

	bool bLivingPresent = false;
	bool bSpiritPresent = false;
	for (const AActor* Occupant : Occupants)
	{
		const EEchoRealm Realm = CastChecked<AOurLastEchoCharacter>(Occupant)->GetRealm();
		bLivingPresent |= Realm == EEchoRealm::Living;
		bSpiritPresent |= Realm == EEchoRealm::Spirit;
	}

	if (bLivingPresent && bSpiritPresent)
	{
		GameState->SetMilestoneComplete();
	}
}
