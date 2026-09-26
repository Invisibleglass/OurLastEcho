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

	// A leaver's open menu no longer holds the pause (this also has to run before the timer below: a paused world's
	// timers don't tick)
	PlayersInPauseMenu.Remove(Cast<APlayerController>(Exiting));
	RefreshMenuPause();

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

void AEchoGameMode::SetPlayerInPauseMenu(APlayerController* PlayerController, bool bOpen)
{
	if (!PlayerController)
	{
		return;
	}
	if (bOpen)
	{
		PlayersInPauseMenu.AddUnique(PlayerController);
	}
	else
	{
		PlayersInPauseMenu.Remove(PlayerController);
	}
	RefreshMenuPause();
}

bool AEchoGameMode::CanUnpauseMenu() const
{
	for (const TWeakObjectPtr<APlayerController>& Player : PlayersInPauseMenu)
	{
		if (Player.IsValid())
		{
			return false;
		}
	}
	return true;
}

void AEchoGameMode::RefreshMenuPause()
{
	PlayersInPauseMenu.RemoveAll([](const TWeakObjectPtr<APlayerController>& Player) { return !Player.IsValid(); });

	TArray<APlayerState*> InMenu;
	for (const TWeakObjectPtr<APlayerController>& Player : PlayersInPauseMenu)
	{
		InMenu.Add(Player->PlayerState);
	}
	if (AEchoGameState* EchoGameState = GetGameState<AEchoGameState>())
	{
		EchoGameState->SetPlayersInPauseMenu(InMenu);
	}

	// The pause itself is the engine's: the pauser replicates through AWorldSettings, so every machine freezes
	if (PlayersInPauseMenu.Num() > 0 && !IsPaused())
	{
		UE_LOG(LogOurLastEcho, Log, TEXT("Paused: %s opened the menu"), *GetNameSafe(PlayersInPauseMenu[0].Get()));
		SetPause(PlayersInPauseMenu[0].Get(), FCanUnpause::CreateUObject(this, &AEchoGameMode::CanUnpauseMenu));
	}
	else if (PlayersInPauseMenu.Num() == 0 && IsPaused())
	{
		UE_LOG(LogOurLastEcho, Log, TEXT("Unpaused: nobody has the menu open"));
		ClearPause();
	}

	// Paused, the server's clock stops, so actors whose next network update isn't due never replicate: push the
	// world settings (which carry the pause) out now so the other player's world pauses and resumes too
	if (AWorldSettings* WorldSettings = GetWorldSettings())
	{
		WorldSettings->ForceNetUpdate();
	}
}
