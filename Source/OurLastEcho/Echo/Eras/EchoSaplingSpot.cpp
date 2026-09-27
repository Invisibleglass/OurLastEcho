// Our Last Echo

#include "EchoSaplingSpot.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "EchoTypes.h"
#include "EchoVisibility.h"
#include "OurLastEchoCharacter.h"
#include "OurLastEcho.h"

namespace
{
	// 90 cm apart for a 900 cm climb: comfortably inside Bat's jump (it peaks at about 128 cm)
	constexpr int32 NumBranchSteps = 10;

	void MakeVisualPart(UStaticMeshComponent* Part, USceneComponent* Parent, UStaticMesh* Mesh)
	{
		Part->SetupAttachment(Parent);
		Part->SetStaticMesh(Mesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
	}

	/** The grown tree: solid for Bat (ECC_Pawn) only, and only once planted */
	void MakeTreePart(UStaticMeshComponent* Part, USceneComponent* Parent, UStaticMesh* Mesh)
	{
		MakeVisualPart(Part, Parent, Mesh);
		Part->SetCollisionObjectType(ECC_WorldStatic);
		Part->SetCollisionResponseToAllChannels(ECR_Ignore);
		Part->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}
}

AEchoSaplingSpot::AEchoSaplingSpot()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Saraa steps in to plant (her capsule is on the SpiritPawn channel); nothing else notices it
	PlantZone = CreateDefaultSubobject<USphereComponent>(TEXT("PlantZone"));
	PlantZone->SetupAttachment(RootComponent);
	PlantZone->SetSphereRadius(140.0f);
	PlantZone->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
	PlantZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PlantZone->SetCollisionObjectType(ECC_WorldDynamic);
	PlantZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	PlantZone->SetCollisionResponseToChannel(ECC_SpiritPawn, ECR_Overlap);
	PlantZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PlantZone->SetGenerateOverlapEvents(true);

	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	MakeVisualPart(Marker, RootComponent, Cylinder.Object);
	Marker->SetRelativeScale3D(FVector(1.6f, 1.6f, 0.03f));
	Marker->SetRelativeLocation(FVector(0.0f, 0.0f, 2.0f));
	Marker->SetCastShadow(false);

	SaplingStem = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SaplingStem"));
	MakeVisualPart(SaplingStem, RootComponent, Cylinder.Object);
	SaplingStem->SetRelativeScale3D(FVector(0.07f, 0.07f, 0.8f));
	SaplingStem->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));

	SaplingLeaves = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SaplingLeaves"));
	MakeVisualPart(SaplingLeaves, RootComponent, Sphere.Object);
	SaplingLeaves->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.4f));
	SaplingLeaves->SetRelativeLocation(FVector(0.0f, 0.0f, 85.0f));

	Trunk = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Trunk"));
	MakeTreePart(Trunk, RootComponent, Cylinder.Object);

	Crown = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Crown"));
	MakeVisualPart(Crown, RootComponent, Sphere.Object);

	for (int32 Index = 0; Index < NumBranchSteps; ++Index)
	{
		UStaticMeshComponent* Branch = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Branch%d"), Index));
		MakeTreePart(Branch, RootComponent, Cube.Object);
		Branches.Add(Branch);
	}

	MarkerMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Eras/MI_SaplingMarker.MI_SaplingMarker")));
	BarkMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Eras/MI_Bark.MI_Bark")));
	LeavesMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Eras/MI_Leaves.MI_Leaves")));
}

void AEchoSaplingSpot::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Trunk (60 cm across), a crown on top, and branch steps spiralling up to the top step
	Trunk->SetRelativeScale3D(FVector(1.2f, 1.2f, TreeHeight / 100.0f));
	Trunk->SetRelativeLocation(FVector(0.0f, 0.0f, TreeHeight * 0.5f));
	Crown->SetRelativeScale3D(FVector(7.0f, 7.0f, 4.5f));
	Crown->SetRelativeLocation(FVector(0.0f, 0.0f, TreeHeight + 120.0f));

	// Steps evenly spaced up to TopStepHeight (a jump each), each a flat branch sticking out from the trunk, turning
	// 80 degrees per step so the last one points along TopStepYaw
	const float StepRise = TopStepHeight / NumBranchSteps;
	for (int32 Index = 0; Index < Branches.Num(); ++Index)
	{
		const float Height = StepRise * (Index + 1);
		const float Yaw = TopStepYaw - 80.0f * (Branches.Num() - 1 - Index);
		const FVector Out = FRotator(0.0f, Yaw, 0.0f).Vector();
		UStaticMeshComponent* Branch = Branches[Index];
		Branch->SetRelativeLocation(Out * 130.0f + FVector(0.0f, 0.0f, Height - 12.0f));
		Branch->SetRelativeRotation(FRotator(0.0f, Yaw, 0.0f));
		Branch->SetRelativeScale3D(FVector(1.8f, 1.1f, 0.24f));
	}

	if (UMaterialInterface* Mat = MarkerMaterial.LoadSynchronous())
	{
		Marker->SetMaterial(0, Mat);
	}
	if (UMaterialInterface* Mat = BarkMaterial.LoadSynchronous())
	{
		Trunk->SetMaterial(0, Mat);
		SaplingStem->SetMaterial(0, Mat);
		for (UStaticMeshComponent* Branch : Branches)
		{
			Branch->SetMaterial(0, Mat);
		}
	}
	if (UMaterialInterface* Mat = LeavesMaterial.LoadSynchronous())
	{
		Crown->SetMaterial(0, Mat);
		SaplingLeaves->SetMaterial(0, Mat);
	}
	UpdateState();
}

void AEchoSaplingSpot::BeginPlay()
{
	Super::BeginPlay();

	PlantZone->OnComponentBeginOverlap.AddDynamic(this, &AEchoSaplingSpot::OnPlantZoneOverlap);
	UpdateState();
	// The era shown here can change (debug swap, the local character arriving), so re-check what's drawn
	GetWorldTimerManager().SetTimer(LookTimer, this, &AEchoSaplingSpot::UpdateState, 0.25f, true);
}

void AEchoSaplingSpot::OnPlantZoneOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const AOurLastEchoCharacter* Character = Cast<AOurLastEchoCharacter>(OtherActor);
	if (HasAuthority() && Character && Character->GetRealm() == EEchoRealm::Spirit)
	{
		Plant();
	}
}

void AEchoSaplingSpot::Plant()
{
	if (!HasAuthority() || bPlanted)
	{
		return;
	}
	bPlanted = true;
	UE_LOG(LogOurLastEcho, Log, TEXT("Sapling: Saraa planted it; a tree now stands in the present"));
	ForceNetUpdate();
	UpdateState();
}

void AEchoSaplingSpot::OnRep_Planted()
{
	UpdateState();
}

void AEchoSaplingSpot::UpdateState()
{
	// Collision: the same on every machine, from the replicated flag
	const ECollisionEnabled::Type TreeCollision = bPlanted ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
	if (Trunk->GetCollisionEnabled() != TreeCollision)
	{
		Trunk->SetCollisionEnabled(TreeCollision);
		for (UStaticMeshComponent* Branch : Branches)
		{
			Branch->SetCollisionEnabled(TreeCollision);
		}
	}

	// What's drawn: this machine's era (in the editor, everything, so it can be placed)
	EEchoEra Era = EEchoEra::Past;
	const bool bKnown = GetWorld() && GetWorld()->IsGameWorld() && EchoVisibility::GetLocalViewerEra(GetWorld(), Era);
	const bool bEditorPreview = !GetWorld() || !GetWorld()->IsGameWorld();
	const bool bPast = bEditorPreview || (bKnown && Era == EEchoEra::Past);
	const bool bPresent = bEditorPreview || (bKnown && Era == EEchoEra::Present);

	Marker->SetVisibility(bPast && !bPlanted);
	SaplingStem->SetVisibility(bPast && (bPlanted || bEditorPreview));
	SaplingLeaves->SetVisibility(bPast && (bPlanted || bEditorPreview));
	const bool bTree = bPresent && (bPlanted || bEditorPreview);
	Trunk->SetVisibility(bTree);
	Crown->SetVisibility(bTree);
	for (UStaticMeshComponent* Branch : Branches)
	{
		Branch->SetVisibility(bTree);
	}
}

bool AEchoSaplingSpot::IsTreeSolid() const
{
	return Trunk->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
}

bool AEchoSaplingSpot::IsTreeVisible() const
{
	return Trunk->IsVisible();
}

bool AEchoSaplingSpot::IsSaplingVisible() const
{
	return SaplingLeaves->IsVisible();
}

FVector AEchoSaplingSpot::GetTopStepLocation() const
{
	return GetStepLocation(Branches.Num() - 1);
}

FVector AEchoSaplingSpot::GetStepLocation(int32 Index) const
{
	const UStaticMeshComponent* Step = Branches.IsValidIndex(Index) ? Branches[Index].Get() : nullptr;
	return Step ? Step->GetComponentLocation() + FVector(0.0f, 0.0f, 12.0f) : GetActorLocation();
}

int32 AEchoSaplingSpot::GetNumSteps() const
{
	return Branches.Num();
}

void AEchoSaplingSpot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEchoSaplingSpot, bPlanted);
}
