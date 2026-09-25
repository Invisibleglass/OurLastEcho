// Our Last Echo

#include "EchoAnchorable.h"
#include "GameFramework/Actor.h"

UEchoAnchorableComponent::UEchoAnchorableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UEchoAnchorRules::CanHoldAnchor(const FHitResult& Hit)
{
	// Today: the hit actor carries an enabled UEchoAnchorableComponent.
	// Later, to use a physical material instead, e.g.:
	//   return Hit.PhysMaterial.IsValid() && Hit.PhysMaterial->SurfaceType == SurfaceType1;   // "AnchorRock"
	const AActor* Actor = Hit.GetActor();
	const UEchoAnchorableComponent* Anchorable = Actor ? Actor->FindComponentByClass<UEchoAnchorableComponent>() : nullptr;
	return Anchorable && Anchorable->bCanHoldAnchor;
}
