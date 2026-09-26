// Our Last Echo

#include "EchoAnchorTarget.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "EchoAnchorable.h"
#include "EchoAnchorPoint.h"
#include "OurLastEchoCharacter.h"
#include "EchoTypes.h"
#include "EchoVisibility.h"

namespace
{
	void SetupDisc(UStaticMeshComponent* Disc, UStaticMesh* Mesh)
	{
		Disc->SetStaticMesh(Mesh);
		Disc->SetMobility(EComponentMobility::Movable);
		Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Disc->SetGenerateOverlapEvents(false);
		Disc->SetCanEverAffectNavigation(false);
		// The engine cylinder's axis is Z; pitch -90 turns it to face +X
		Disc->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	}
}

AEchoAnchorTarget::AEchoAnchorTarget()
{
	// Every frame, for a smooth turn towards Bat
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Board = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Board"));
	Board->SetupAttachment(RootComponent);
	SetupDisc(Board, CylinderMesh.Object);
	// Arrows (and the bow's aim trace) hit the board; nothing else does
	Board->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Board->SetCollisionObjectType(ECC_WorldStatic);
	Board->SetCollisionResponseToAllChannels(ECR_Ignore);
	Board->SetCollisionResponseToChannel(ECC_EchoArrow, ECR_Block);

	Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ring"));
	Ring->SetupAttachment(RootComponent);
	SetupDisc(Ring, CylinderMesh.Object);

	Bullseye = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bullseye"));
	Bullseye->SetupAttachment(RootComponent);
	SetupDisc(Bullseye, CylinderMesh.Object);

	Anchorable = CreateDefaultSubobject<UEchoAnchorableComponent>(TEXT("Anchorable"));

	BoardMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Materials/MI_AnchorTargetBoard.MI_AnchorTargetBoard")));
	RingMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Materials/MI_AnchorTargetRing.MI_AnchorTargetRing")));
}

void AEchoAnchorTarget::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Discs stacked out of the rock: the board, then a ring and a bullseye standing slightly proud of it
	const float BoardScale = Diameter / 100.0f;
	Board->SetRelativeScale3D(FVector(BoardScale, BoardScale, Thickness / 100.0f));
	Board->SetRelativeLocation(FVector(Thickness * 0.5f, 0.0f, 0.0f));
	Ring->SetRelativeScale3D(FVector(BoardScale * 0.68f, BoardScale * 0.68f, 0.02f));
	Ring->SetRelativeLocation(FVector(Thickness + 0.5f, 0.0f, 0.0f));
	Bullseye->SetRelativeScale3D(FVector(BoardScale * 0.3f, BoardScale * 0.3f, 0.03f));
	Bullseye->SetRelativeLocation(FVector(Thickness + 1.0f, 0.0f, 0.0f));

	UMaterialInterface* BoardMat = BoardMaterial.LoadSynchronous();
	UMaterialInterface* RingMat = RingMaterial.LoadSynchronous();
	if (BoardMat)
	{
		Board->SetMaterial(0, BoardMat);
		Bullseye->SetMaterial(0, BoardMat);
	}
	if (RingMat)
	{
		Ring->SetMaterial(0, RingMat);
	}
}

void AEchoAnchorTarget::BeginPlay()
{
	Super::BeginPlay();

	RestRotation = GetActorQuat();
	UpdateLocalVisibility();
}

void AEchoAnchorTarget::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateLocalVisibility();
	UpdateFacing(DeltaSeconds);
}

void AEchoAnchorTarget::UpdateFacing(float DeltaSeconds)
{
	if (!bFaceBat || HasAnchorOnFace())
	{
		return;
	}

	if (!Bat.IsValid())
	{
		for (AOurLastEchoCharacter* Character : TActorRange<AOurLastEchoCharacter>(GetWorld()))
		{
			if (Character->GetRealm() == EEchoRealm::Living)
			{
				Bat = Character;
				break;
			}
		}
		if (!Bat.IsValid())
		{
			return;
		}
	}

	// Towards his chest, but no further than MaxTurnAngle from the placed facing
	const FVector RestForward = RestRotation.GetForwardVector();
	FVector Desired = (Bat->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f) - GetActorLocation()).GetSafeNormal();
	if (Desired.IsNearlyZero())
	{
		return;
	}
	const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(RestForward, Desired), -1.0f, 1.0f)));
	if (Angle > MaxTurnAngle)
	{
		const FQuat Full = FQuat::FindBetweenNormals(RestForward, Desired);
		Desired = FQuat::Slerp(FQuat::Identity, Full, MaxTurnAngle / Angle).RotateVector(RestForward);
	}

	const FRotator Target = FRotationMatrix::MakeFromX(Desired).Rotator();
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Target, DeltaSeconds, TurnSpeed));
}

bool AEchoAnchorTarget::HasAnchorOnFace() const
{
	const FVector Face = GetFaceCenter();
	for (const AEchoAnchorPoint* Anchor : TActorRange<AEchoAnchorPoint>(GetWorld()))
	{
		if (FVector::DistSquared(Anchor->GetActorLocation(), Face) < FMath::Square(Diameter))
		{
			return true;
		}
	}
	return false;
}

FVector AEchoAnchorTarget::GetFaceCenter() const
{
	return GetActorLocation() + GetActorForwardVector() * Thickness;
}

void AEchoAnchorTarget::UpdateLocalVisibility()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	EEchoRealm Viewer = EEchoRealm::Living;
	const bool bShouldShow = !EchoVisibility::GetLocalViewerRealm(GetWorld(), Viewer) || Viewer == EEchoRealm::Living
		|| EchoVisibility::IsDebugShowAll(GetWorld());

	if (bShouldShow != bLocallyVisible)
	{
		bLocallyVisible = bShouldShow;

		// Local only; the board keeps its collision for arrows either way
		Board->SetVisibility(bShouldShow);
		Ring->SetVisibility(bShouldShow);
		Bullseye->SetVisibility(bShouldShow);
	}
}
