// Our Last Echo

#include "EchoCheckpoint.h"
#include "Components/BoxComponent.h"
#include "EchoTypes.h"
#include "OurLastEcho.h"
#include "OurLastEchoCharacter.h"

AEchoCheckpoint::AEchoCheckpoint()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	Volume->SetupAttachment(RootComponent);
	Volume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Volume->SetCollisionResponseToAllChannels(ECR_Ignore);
	Volume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Volume->SetCollisionResponseToChannel(ECC_SpiritPawn, ECR_Overlap);
	Volume->SetGenerateOverlapEvents(true);
	Volume->SetCanEverAffectNavigation(false);
	Volume->ShapeColor = FColor(80, 220, 255);
	Volume->OnComponentBeginOverlap.AddDynamic(this, &AEchoCheckpoint::OnVolumeBeginOverlap);
}

void AEchoCheckpoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Volume->SetBoxExtent(VolumeSize * 0.5f);
	Volume->SetRelativeLocation(FVector(0.0f, 0.0f, VolumeSize.Z * 0.5f));
}

FTransform AEchoCheckpoint::GetRespawnTransform() const
{
	return FTransform(FRotator(0.0f, GetActorRotation().Yaw, 0.0f), GetActorTransform().TransformPosition(RespawnPoint));
}

void AEchoCheckpoint::OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	AOurLastEchoCharacter* Character = Cast<AOurLastEchoCharacter>(OtherActor);
	if (Character && (!bOnlyOneRealm || Character->GetRealm() == OnlyRealm))
	{
		UE_LOG(LogOurLastEcho, Log, TEXT("%s reached checkpoint %s"), *Character->GetName(), *GetName());
		Character->SetRespawnTransform(GetRespawnTransform());
	}
}
