// Our Last Echo

#include "EchoInstancedProps.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

AEchoInstancedProps::AEchoInstancedProps()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Props = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Props"));
	RootComponent = Props;
	// Visual only: everything anyone can touch lives in the shared layer
	Props->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Props->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Props->SetGenerateOverlapEvents(false);
	Props->SetCanEverAffectNavigation(false);
	Props->SetMobility(EComponentMobility::Static);
}

void AEchoInstancedProps::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Props->SetStaticMesh(Mesh);
	if (Material)
	{
		Props->SetMaterial(0, Material);
	}
	Props->SetCastShadow(bCastShadow);
	Props->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Props->ClearInstances();
	Props->AddInstances(Instances, false, false);
}

int32 AEchoInstancedProps::GetNumInstances() const
{
	return Props->GetInstanceCount();
}
