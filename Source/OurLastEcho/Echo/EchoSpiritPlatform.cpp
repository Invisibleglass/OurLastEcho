// Our Last Echo

#include "EchoSpiritPlatform.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "EchoTypes.h"
#include "OurLastEchoCharacter.h"

AEchoSpiritPlatform::AEchoSpiritPlatform()
{
	PrimaryActorTick.bCanEverTick = true;
	// Visibility only needs to catch possession changes; no need to check every frame
	PrimaryActorTick.TickInterval = 0.1f;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}

	// Solid for Spirit characters only
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_SpiritPawn, ECR_Block);
	Mesh->SetGenerateOverlapEvents(false);

	PlatformMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Materials/MI_SpiritPlatform.MI_SpiritPlatform")));
}

void AEchoSpiritPlatform::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// The engine cube is 100cm and centred, so scale it to size and drop it so the actor origin is the top surface
	Mesh->SetRelativeScale3D(PlatformSize / 100.0f);
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -PlatformSize.Z * 0.5f));

	if (UMaterialInterface* Material = PlatformMaterial.LoadSynchronous())
	{
		Mesh->SetMaterial(0, Material);
	}
}

void AEchoSpiritPlatform::BeginPlay()
{
	Super::BeginPlay();

	UpdateLocalVisibility();
}

void AEchoSpiritPlatform::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateLocalVisibility();
}

void AEchoSpiritPlatform::UpdateLocalVisibility()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	bool bShouldShow = false;
	if (const APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld()))
	{
		if (const AOurLastEchoCharacter* Character = Cast<AOurLastEchoCharacter>(PC->GetPawn()))
		{
			bShouldShow = Character->GetRealm() == EEchoRealm::Spirit;
		}
	}

	if (bShouldShow != bLocallyVisible)
	{
		bLocallyVisible = bShouldShow;

		// Component visibility is local-only (unlike SetActorHiddenInGame, which replicates)
		Mesh->SetVisibility(bShouldShow);
	}
}
