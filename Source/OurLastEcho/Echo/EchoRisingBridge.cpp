// Our Last Echo

#include "EchoRisingBridge.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "OurLastEcho.h"

AEchoRisingBridge::AEchoRisingBridge()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	bAlwaysRelevant = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	BridgeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BridgeMesh"));
	BridgeMesh->SetupAttachment(RootComponent);
	BridgeMesh->SetMobility(EComponentMobility::Movable);
	BridgeMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BridgeMesh->SetStaticMesh(CubeMesh.Object);
	}

	BridgeMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray.MI_PrototypeGrid_Gray")));
}

void AEchoRisingBridge::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	BridgeMesh->SetRelativeScale3D(BridgeSize / 100.0f);
	ApplyRaiseAlpha(1.0f);

	if (UMaterialInterface* Material = BridgeMaterial.LoadSynchronous())
	{
		BridgeMesh->SetMaterial(0, Material);
	}
}

void AEchoRisingBridge::BeginPlay()
{
	Super::BeginPlay();

	RaiseAlpha = bRaised ? 1.0f : 0.0f;
	ApplyRaiseAlpha(RaiseAlpha);
}

void AEchoRisingBridge::Raise()
{
	if (!HasAuthority() || bRaised)
	{
		return;
	}

	UE_LOG(LogOurLastEcho, Log, TEXT("%s raising"), *GetName());

	bRaised = true;
	OnRep_Raised();
}

void AEchoRisingBridge::OnRep_Raised()
{
	if (bRaised)
	{
		SetActorTickEnabled(true);
	}
}

void AEchoRisingBridge::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	RaiseAlpha = FMath::Min(1.0f, RaiseAlpha + DeltaSeconds / RaiseDuration);
	ApplyRaiseAlpha(RaiseAlpha);

	if (RaiseAlpha >= 1.0f)
	{
		SetActorTickEnabled(false);
	}
}

void AEchoRisingBridge::ApplyRaiseAlpha(float Alpha)
{
	const float Eased = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);
	const FQuat Swing = FQuat::Slerp(StartRotationOffset.Quaternion(), FQuat::Identity, Eased);
	const FVector MeshCentre(bHingeAtStart ? BridgeSize.X * 0.5f : 0.0f, 0.0f, -BridgeSize.Z * 0.5f);
	BridgeMesh->SetRelativeLocationAndRotation(Swing.RotateVector(MeshCentre) + LoweredOffset * (1.0f - Eased), Swing);
}

void AEchoRisingBridge::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEchoRisingBridge, bRaised);
}
