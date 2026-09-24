// Our Last Echo

#include "EchoVisibility.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "EchoGameState.h"
#include "OurLastEchoCharacter.h"

bool EchoVisibility::GetLocalViewerRealm(const UWorld* World, EEchoRealm& OutRealm)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	const APlayerController* PC = GEngine->GetFirstLocalPlayerController(World);
	const AOurLastEchoCharacter* Character = PC ? Cast<AOurLastEchoCharacter>(PC->GetPawn()) : nullptr;
	if (!Character)
	{
		return false;
	}

	OutRealm = Character->GetRealm();
	return true;
}

bool EchoVisibility::IsDebugShowAll(const UWorld* World)
{
	const AEchoGameState* GameState = World ? World->GetGameState<AEchoGameState>() : nullptr;
	return GameState && GameState->IsDebugShowAllPlatforms();
}
