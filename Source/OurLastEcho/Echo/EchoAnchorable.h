// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EchoAnchorable.generated.h"

/**
 *  Marks an actor whose surfaces can hold one of Bat's anchor arrows (AEchoAnchorTarget has one).
 *  Add it to any actor in the editor to make it anchorable.
 */
UCLASS(ClassGroup=(Echo), meta=(BlueprintSpawnableComponent))
class UEchoAnchorableComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UEchoAnchorableComponent();

	/** Switch off to make the actor temporarily unanchorable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Anchor")
	bool bCanHoldAnchor = true;
};

/**
 *  THE one place that decides whether an arrow hit can become an anchor point. The arrow and the whip
 *  never check anything else, so switching the rule later (e.g. to a physical material such as a certain
 *  rock or wood) only means changing CanHoldAnchor. Arrow sweeps already return the physical material
 *  (bReturnMaterialOnMove on the arrow's collision), so Hit.PhysMaterial is filled in and ready to test.
 */
UCLASS()
class UEchoAnchorRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** True if an arrow that made this hit should stick and become an anchor point */
	UFUNCTION(BlueprintPure, Category="Echo|Anchor")
	static bool CanHoldAnchor(const FHitResult& Hit);
};
