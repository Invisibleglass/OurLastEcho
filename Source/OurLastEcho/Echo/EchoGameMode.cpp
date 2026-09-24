// Our Last Echo

#include "EchoGameMode.h"
#include "GameFramework/Controller.h"
#include "EchoGameState.h"
#include "EchoHUD.h"
#include "OurLastEcho.h"

AEchoGameMode::AEchoGameMode()
{
	GameStateClass = AEchoGameState::StaticClass();
	HUDClass = AEchoHUD::StaticClass();
}

UClass* AEchoGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// Called more than once per player (start-spot selection and spawning), so this must be idempotent
	if (!BatController.IsValid())
	{
		BatController = InController;
	}

	const bool bIsBat = BatController.Get() == InController;
	const TSubclassOf<APawn> PawnClass = bIsBat ? BatPawnClass : SaraaPawnClass;

	if (!PawnClass)
	{
		UE_LOG(LogOurLastEcho, Warning, TEXT("EchoGameMode: %s pawn class not set, using default pawn"), bIsBat ? TEXT("Bat") : TEXT("Saraa"));
		return Super::GetDefaultPawnClassForController_Implementation(InController);
	}

	UE_LOG(LogOurLastEcho, Log, TEXT("EchoGameMode: %s plays %s (%s)"), *GetNameSafe(InController), bIsBat ? TEXT("Bat") : TEXT("Saraa"), *PawnClass->GetName());
	return PawnClass;
}
