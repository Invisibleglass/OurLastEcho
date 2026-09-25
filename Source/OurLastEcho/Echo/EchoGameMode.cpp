// Our Last Echo

#include "EchoGameMode.h"
#include "GameFramework/Controller.h"
#include "EchoGameState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
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
	if (!FirstController.IsValid())
	{
		FirstController = InController;
	}

	const bool bIsBat = (FirstController.Get() == InController) != bHostPlaysSaraa;
	const TSubclassOf<APawn> PawnClass = bIsBat ? BatPawnClass : SaraaPawnClass;

	if (!PawnClass)
	{
		UE_LOG(LogOurLastEcho, Warning, TEXT("EchoGameMode: %s pawn class not set, using default pawn"), bIsBat ? TEXT("Bat") : TEXT("Saraa"));
		return Super::GetDefaultPawnClassForController_Implementation(InController);
	}

	UE_LOG(LogOurLastEcho, Log, TEXT("EchoGameMode: %s plays %s (%s)"), *GetNameSafe(InController), bIsBat ? TEXT("Bat") : TEXT("Saraa"), *PawnClass->GetName());
	return PawnClass;
}

void AEchoGameMode::SetPlayerInSettingsMenu(APlayerController* PlayerController, bool bOpen)
{
	if (!PlayerController)
	{
		return;
	}

	if (bOpen)
	{
		PlayersInSettingsMenu.AddUnique(PlayerController);
	}
	else
	{
		PlayersInSettingsMenu.Remove(PlayerController);
	}
	RefreshSettingsMenuPause();
}

void AEchoGameMode::Logout(AController* Exiting)
{
	PlayersInSettingsMenu.Remove(Cast<APlayerController>(Exiting));
	Super::Logout(Exiting);
	RefreshSettingsMenuPause();
}

bool AEchoGameMode::CanUnpauseSettingsMenu() const
{
	for (const TWeakObjectPtr<APlayerController>& Player : PlayersInSettingsMenu)
	{
		if (Player.IsValid())
		{
			return false;
		}
	}
	return true;
}

void AEchoGameMode::RefreshSettingsMenuPause()
{
	PlayersInSettingsMenu.RemoveAll([](const TWeakObjectPtr<APlayerController>& Player) { return !Player.IsValid(); });

	TArray<APlayerState*> InMenu;
	for (const TWeakObjectPtr<APlayerController>& Player : PlayersInSettingsMenu)
	{
		InMenu.Add(Player->PlayerState);
	}
	if (AEchoGameState* EchoGameState = GetGameState<AEchoGameState>())
	{
		EchoGameState->SetPlayersInSettingsMenu(InMenu);
	}

	// The pause itself is the engine's: the pauser replicates through AWorldSettings, so every machine freezes
	if (PlayersInSettingsMenu.Num() > 0 && !IsPaused())
	{
		UE_LOG(LogOurLastEcho, Log, TEXT("Paused: %s opened the settings menu"), *GetNameSafe(PlayersInSettingsMenu[0].Get()));
		SetPause(PlayersInSettingsMenu[0].Get(), FCanUnpause::CreateUObject(this, &AEchoGameMode::CanUnpauseSettingsMenu));
	}
	else if (PlayersInSettingsMenu.Num() == 0 && IsPaused())
	{
		UE_LOG(LogOurLastEcho, Log, TEXT("Unpaused: nobody is in the settings menu"));
		ClearPause();
	}

	// Paused, the server's clock stops, so actors whose next network update isn't due yet never replicate:
	// push the world settings (which carry the pause) out now so the other player's world pauses/resumes too
	if (AWorldSettings* WorldSettings = GetWorldSettings())
	{
		WorldSettings->ForceNetUpdate();
	}
}
