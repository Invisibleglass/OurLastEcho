// Our Last Echo

#include "EchoRespawnVolume.h"
#include "Components/BoxComponent.h"
#include "EchoTypes.h"
#include "OurLastEchoCharacter.h"

AEchoRespawnVolume::AEchoRespawnVolume()
{
	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	RootComponent = Volume;
	Volume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Volume->SetCollisionResponseToAllChannels(ECR_Ignore);
	Volume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Volume->SetCollisionResponseToChannel(ECC_SpiritPawn, ECR_Overlap);
	Volume->SetGenerateOverlapEvents(true);
	Volume->OnComponentBeginOverlap.AddDynamic(this, &AEchoRespawnVolume::OnVolumeBeginOverlap);
}

void AEchoRespawnVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Volume->SetBoxExtent(VolumeSize * 0.5f);
}

void AEchoRespawnVolume::OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AOurLastEchoCharacter* Character = Cast<AOurLastEchoCharacter>(OtherActor))
	{
		Character->RespawnAtStart();
	}
}
