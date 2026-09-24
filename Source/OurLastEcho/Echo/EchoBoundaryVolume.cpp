// Our Last Echo

#include "EchoBoundaryVolume.h"
#include "Components/BoxComponent.h"
#include "EchoTypes.h"

AEchoBoundaryVolume::AEchoBoundaryVolume()
{
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	RootComponent = Box;

	// Blocks Bat (Pawn) and Saraa (SpiritPawn) only
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Box->SetCollisionResponseToChannel(ECC_SpiritPawn, ECR_Block);
	Box->SetGenerateOverlapEvents(false);
	Box->SetCanEverAffectNavigation(false);
	Box->ShapeColor = FColor(255, 120, 40);
}

void AEchoBoundaryVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Box->SetBoxExtent(BoxSize * 0.5f);
}
