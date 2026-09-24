// Our Last Echo

#include "EchoSpiritSwitch.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "EchoRisingBridge.h"
#include "OurLastEcho.h"
#include "OurLastEchoCharacter.h"

namespace
{
	/** Pad footprint and height in cm */
	constexpr float PadDiameter = 150.0f;
	constexpr float PadHeight = 10.0f;
}

AEchoSpiritSwitch::AEchoSpiritSwitch()
{
	bReplicates = true;
	bAlwaysRelevant = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(RootComponent);
	PadMesh->SetMobility(EComponentMobility::Movable);
	PadMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	PadMesh->SetRelativeScale3D(FVector(PadDiameter / 100.0f, PadDiameter / 100.0f, PadHeight / 100.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		PadMesh->SetStaticMesh(CylinderMesh.Object);
	}

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(RootComponent);
	Trigger->SetBoxExtent(FVector(PadDiameter * 0.5f, PadDiameter * 0.5f, 60.0f));
	Trigger->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetCollisionResponseToChannel(ECC_SpiritPawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AEchoSpiritSwitch::OnTriggerBeginOverlap);

	PadMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Materials/MI_Switch.MI_Switch")));
}

void AEchoSpiritSwitch::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UpdatePadPosition();

	if (UMaterialInterface* Material = PadMaterial.LoadSynchronous())
	{
		PadMesh->SetMaterial(0, Material);
	}
}

void AEchoSpiritSwitch::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || bActivated)
	{
		return;
	}

	const AOurLastEchoCharacter* Character = Cast<AOurLastEchoCharacter>(OtherActor);
	if (!Character || Character->GetRealm() != RequiredRealm)
	{
		return;
	}

	UE_LOG(LogOurLastEcho, Log, TEXT("%s pressed by %s"), *GetName(), *Character->GetName());

	bActivated = true;
	OnRep_Activated();

	if (TargetBridge)
	{
		TargetBridge->Raise();
	}
	else
	{
		UE_LOG(LogOurLastEcho, Warning, TEXT("%s has no TargetBridge set"), *GetName());
	}
}

void AEchoSpiritSwitch::OnRep_Activated()
{
	UpdatePadPosition();
}

void AEchoSpiritSwitch::UpdatePadPosition()
{
	// The engine cylinder is centred, so lift it by half its height to sit on the actor origin
	const float PadZ = PadHeight * 0.5f - (bActivated ? PressedDepth : 0.0f);
	PadMesh->SetRelativeLocation(FVector(0.0f, 0.0f, PadZ));
}

void AEchoSpiritSwitch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEchoSpiritSwitch, bActivated);
}
