// Our Last Echo

#include "EchoFrontEndGameMode.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "EchoFrontEndPlayerController.h"
#include "EchoSessionSubsystem.h"
#include "OurLastEcho.h"

#define LOCTEXT_NAMESPACE "EchoLobby"

FText AEchoLobbyPlayerState::GetRoleName() const
{
	return bHostPlayer ? LOCTEXT("Bat", "Bat") : LOCTEXT("Saraa", "Saraa");
}

void AEchoLobbyPlayerState::SetReady(bool bNewReady)
{
	bReady = bNewReady;
	ForceNetUpdate();
}

void AEchoLobbyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEchoLobbyPlayerState, bReady);
	DOREPLIFETIME(AEchoLobbyPlayerState, bHostPlayer);
}

AEchoFrontEndGameMode::AEchoFrontEndGameMode()
{
	PlayerControllerClass = AEchoFrontEndPlayerController::StaticClass();
	PlayerStateClass = AEchoLobbyPlayerState::StaticClass();
	DefaultPawnClass = nullptr;
	// Non-seamless travel into the game: simple and works in Play-In-Editor
	bUseSeamlessTravel = false;
}

void AEchoFrontEndGameMode::RestartPlayer(AController* NewPlayer)
{
	// No pawns on the title screen or in the lobby
}

bool AEchoFrontEndGameMode::IsLobby() const
{
	return GetNetMode() == NM_ListenServer || GetNetMode() == NM_DedicatedServer;
}

void AEchoFrontEndGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	if (ErrorMessage.IsEmpty() && GetNumPlayers() >= UEchoSessionSubsystem::MaxPlayers)
	{
		ErrorMessage = TEXT("This game is full.");
	}
	else if (ErrorMessage.IsEmpty() && bStarting)
	{
		ErrorMessage = TEXT("This game is already starting.");
	}
}

void AEchoFrontEndGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// The host is the first player (the listen server's own); they play Bat in the game
	if (AEchoLobbyPlayerState* State = NewPlayer->GetPlayerState<AEchoLobbyPlayerState>())
	{
		State->SetHostPlayer(NewPlayer->IsLocalController() && GetNetMode() != NM_Client);
	}

	if (IsLobby() && !NewPlayer->IsLocalController())
	{
		// Tell the host someone arrived
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (AEchoFrontEndPlayerController* Host = Cast<AEchoFrontEndPlayerController>(It->Get()); Host && Host->IsLocalController())
			{
				Host->ClientShowToast(LOCTEXT("Joined", "Saraa joined the lobby."));
			}
		}
	}
}

void AEchoFrontEndGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	if (!IsLobby() || bStarting || !GetWorld() || GetWorld()->bIsTearingDown)
	{
		return;
	}

	// The other player left the lobby: the host stays, un-readies, and gets a message
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AEchoFrontEndPlayerController* PC = Cast<AEchoFrontEndPlayerController>(It->Get());
		if (PC && PC != Exiting)
		{
			if (AEchoLobbyPlayerState* State = PC->GetPlayerState<AEchoLobbyPlayerState>())
			{
				State->SetReady(false);
			}
			PC->ClientShowToast(LOCTEXT("Left", "Saraa left the lobby."));
		}
	}
}

bool AEchoFrontEndGameMode::CanStartGame() const
{
	const AGameStateBase* State = GetGameState<AGameStateBase>();
	if (!State || State->PlayerArray.Num() != UEchoSessionSubsystem::MaxPlayers)
	{
		return false;
	}
	for (const APlayerState* Player : State->PlayerArray)
	{
		const AEchoLobbyPlayerState* LobbyPlayer = Cast<AEchoLobbyPlayerState>(Player);
		if (!LobbyPlayer || !LobbyPlayer->IsReady())
		{
			return false;
		}
	}
	return true;
}

bool AEchoFrontEndGameMode::StartGame(APlayerController* RequestedBy)
{
	const AEchoLobbyPlayerState* Requester = RequestedBy ? RequestedBy->GetPlayerState<AEchoLobbyPlayerState>() : nullptr;
	if (bStarting || !IsLobby() || !Requester || !Requester->IsHostPlayer() || !CanStartGame())
	{
		return false;
	}

	bStarting = true;
	UE_LOG(LogOurLastEcho, Log, TEXT("Lobby: starting the game (%s)"), *GameplayMap);
	// The host's controller is recreated first on the new map, so the gameplay game mode makes the host Bat again
	GetWorld()->ServerTravel(GameplayMap + TEXT("?listen"), true);
	return true;
}

#undef LOCTEXT_NAMESPACE
