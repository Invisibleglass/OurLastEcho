// Our Last Echo

#include "EchoGameMode.h"
#include "GameFramework/Controller.h"
#include "EchoGameState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "EchoGameInstance.h"
#include "OurLastEchoCharacter.h"
#include "EchoHUD.h"
#include "OurLastEcho.h"

namespace
{
	/** Testing: the host plays Saraa (same as BP_EchoGameMode.bHostPlaysSaraa, but settable from the console before starting PIE) */
	TAutoConsoleVariable<bool> CVarEchoHostPlaysSaraa(TEXT("Echo.HostPlaysSaraa"), false, TEXT("If true, the first player (the listen-server host) plays Saraa and the joiner plays Bat."));
}

AEchoGameMode::AEchoGameMode()
{
	GameStateClass = AEchoGameState::StaticClass();
	HUDClass = AEchoHUD::StaticClass();
}

UClass* AEchoGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// Called more than once per player (start-spot selection and spawning), so this must be idempotent
	if (!FirstController.IsValid())
	{
		FirstController = InController;
	}

	const bool bSwapRoles = bHostPlaysSaraa || CVarEchoHostPlaysSaraa.GetValueOnGameThread();
	const bool bIsBat = (FirstController.Get() == InController) != bSwapRoles;
	const TSubclassOf<APawn> PawnClass = bIsBat ? BatPawnClass : SaraaPawnClass;

	if (!PawnClass)
	{
		UE_LOG(LogOurLastEcho, Warning, TEXT("EchoGameMode: %s pawn class not set, using default pawn"), bIsBat ? TEXT("Bat") : TEXT("Saraa"));
		return Super::GetDefaultPawnClassForController_Implementation(InController);
	}

	UE_LOG(LogOurLastEcho, Log, TEXT("EchoGameMode: %s plays %s (%s)"), *GetNameSafe(InController), bIsBat ? TEXT("Bat") : TEXT("Saraa"), *PawnClass->GetName());
	return PawnClass;
}


void AEchoGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	// The other player left an online game: the host goes back to the title screen too, and says why.
	// (Not while the world is being torn down, e.g. the host leaving or Play-In-Editor stopping.)
	UWorld* World = GetWorld();
	const APlayerController* Leaver = Cast<APlayerController>(Exiting);
	if (!World || World->bIsTearingDown || !HasActorBegunPlay() || GetNetMode() != NM_ListenServer || !Leaver || Leaver->IsLocalController())
	{
		return;
	}

	// The leaver's pawn is already gone; they played whichever role the host doesn't
	const APlayerController* Host = World->GetFirstPlayerController();
	const AOurLastEchoCharacter* HostCharacter = Host ? Cast<AOurLastEchoCharacter>(Host->GetPawn()) : nullptr;
	const FText Message = HostCharacter && HostCharacter->GetRealm() == EEchoRealm::Spirit
		? NSLOCTEXT("EchoGame", "BatLeft", "Bat left the game.")
		: NSLOCTEXT("EchoGame", "SaraaLeft", "Saraa left the game.");
	UE_LOG(LogOurLastEcho, Log, TEXT("EchoGameMode: %s left, returning the host to the title screen"), *GetNameSafe(Exiting));

	// Next tick: loading a map from inside Logout isn't safe
	TWeakObjectPtr<UWorld> WeakWorld(World);
	World->GetTimerManager().SetTimerForNextTick([WeakWorld, Message]()
	{
		if (UEchoGameInstance* GameInstance = WeakWorld.IsValid() ? WeakWorld->GetGameInstance<UEchoGameInstance>() : nullptr)
		{
			GameInstance->ReturnToTitle(Message);
		}
	});
}
