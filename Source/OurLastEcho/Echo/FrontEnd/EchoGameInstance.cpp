// Our Last Echo

#include "EchoGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "EchoNetDiagnostics.h"
#include "EchoOnline.h"
#include "Kismet/GameplayStatics.h"
#include "EchoFrontEndPlayerController.h"
#include "EchoGameUserSettings.h"
#include "EchoSessionSubsystem.h"
#include "OurLastEcho.h"

#define LOCTEXT_NAMESPACE "EchoFrontEnd"

const TCHAR* UEchoGameInstance::TitleMap = TEXT("/Game/Echo/Maps/TitleScreen");

void UEchoGameInstance::Init()
{
	Super::Init();

	NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UEchoGameInstance::HandleNetworkFailure);
	TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UEchoGameInstance::HandleTravelFailure);

	// Saved settings (graphics are applied by the engine at start-up; ours need the audio device and engine)
	if (UEchoGameUserSettings* Settings = UEchoGameUserSettings::GetEchoSettings())
	{
		Settings->ApplyDisplayAndAccessibility();
	}
}

void UEchoGameInstance::Shutdown()
{
	GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	Super::Shutdown();
}

FText UEchoGameInstance::ConsumePendingMessage()
{
	FText Message = PendingMessage;
	PendingMessage = FText::GetEmpty();
	return Message;
}

void UEchoGameInstance::ReturnToTitle(const FText& Message)
{
	if (bReturningToTitle)
	{
		return;
	}
	TGuardValue<bool> Guard(bReturningToTitle, true);
	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Returning to the title screen%s%s"), Message.IsEmpty() ? TEXT("") : TEXT(": "), *Message.ToString()));
	SetPendingMessage(Message);

	// A host tells the other player first, so they get a clear message instead of a dropped connection
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() == NM_ListenServer)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && !PC->IsLocalController())
			{
				PC->ClientReturnToMainMenuWithTextReason(LOCTEXT("HostLeftReason", "The host left the game."));
				if (UNetConnection* Connection = PC->GetNetConnection())
				{
					Connection->FlushNet();
				}
			}
		}
	}

	GetSubsystem<UEchoSessionSubsystem>()->LeaveSession();

	// Opening a map without ?listen also closes our listen server, which disconnects the other player
	UGameplayStatics::OpenLevel(World, FName(TitleMap), true);
}

void UEchoGameInstance::ReturnToLobby(const FText& Message)
{
	UEchoSessionSubsystem* Sessions = GetSubsystem<UEchoSessionSubsystem>();
	if (!Sessions->IsInSession())
	{
		ReturnToTitle(Message);
		return;
	}
	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Returning to the lobby: %s"), *Message.ToString()));
	SetPendingMessage(Message);
	UGameplayStatics::OpenLevel(GetWorld(), FName(TitleMap), true, EchoOnline::IsEOS(this) ? TEXT("listen") : TEXT("listen?bUseIPSockets"));
}

void UEchoGameInstance::ReturnToMainMenu()
{
	// The host is leaving (see ReturnToTitle); the engine's reason text doesn't reach us, but this is the only reason.
	// Only a guest acts on it: an RPC that ran on the host itself (editor Python runs RPCs locally) must not send
	// the host round ReturnToTitle again (that recursed until the stack overflowed)
	const UWorld* World = GetWorld();
	if (bReturningToTitle || !World || World->GetNetMode() != NM_Client)
	{
		return;
	}
	ReturnToTitle(LOCTEXT("HostLeft", "The host left the game."));
}

void UEchoGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// Only our own world's connection (in PIE every player's failures come through the same engine delegate)
	if (World != GetWorld())
	{
		return;
	}

	const bool bWasClient = NetDriver && NetDriver->ServerConnection != nullptr;
	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Network failure (%s, as %s): %s"), ENetworkFailure::ToString(FailureType), bWasClient ? TEXT("guest") : TEXT("host"), *ErrorString));
	if (PendingMessage.IsEmpty())
	{
		FText Message;
		if (!bWasClient)
		{
			Message = LOCTEXT("ConnectionLost", "The connection to the other player was lost.");
		}
		else if (FailureType == ENetworkFailure::ConnectionTimeout)
		{
			Message = LOCTEXT("HostTimedOut", "Lost the connection to the host (no answer for a while). Check your internet connection.");
		}
		else if (FailureType == ENetworkFailure::OutdatedClient || FailureType == ENetworkFailure::OutdatedServer)
		{
			Message = LOCTEXT("Outdated", "You and the host have different versions of the game.");
		}
		else
		{
			Message = LOCTEXT("HostLeft2", "The host left the game.");
		}
		SetPendingMessage(Message);
	}
	GetSubsystem<UEchoSessionSubsystem>()->LeaveSession();
	// The engine now returns this machine to the default map: the title screen
}

void UEchoGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (World && World != GetWorld())
	{
		return;
	}

	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Travel failure (%s): %s"), ETravelFailure::ToString(FailureType), *ErrorString));
	SetPendingMessage(ErrorString.Contains(TEXT("full"))
		? LOCTEXT("JoinTravelFull", "That game is full.")
		: LOCTEXT("JoinTravelFailed", "Couldn't connect to that game."));
	GetSubsystem<UEchoSessionSubsystem>()->LeaveSession();
}

void UEchoGameInstance::EchoHost()
{
	GetSubsystem<UEchoSessionSubsystem>()->HostSession();
}

void UEchoGameInstance::EchoFindGames()
{
	UEchoSessionSubsystem* Sessions = GetSubsystem<UEchoSessionSubsystem>();
	Sessions->OnFindComplete.RemoveAll(this);
	Sessions->OnFindComplete.AddWeakLambda(this, [Sessions](bool bSuccess, const FText&)
	{
		for (const FEchoSessionInfo& Info : Sessions->GetSearchResults())
		{
			UE_LOG(LogOurLastEcho, Log, TEXT("Sessions: game %d hosted by %s (%d ms, %d open)"), Info.Index, *Info.HostName, Info.PingMs, Info.OpenSlots);
		}
	});
	Sessions->FindSessions();
}

void UEchoGameInstance::EchoJoinGame(int32 Index)
{
	GetSubsystem<UEchoSessionSubsystem>()->JoinSession(Index);
}

void UEchoGameInstance::EchoReady(int32 bReady)
{
	if (AEchoFrontEndPlayerController* PC = Cast<AEchoFrontEndPlayerController>(GetFirstLocalPlayerController()))
	{
		PC->SetReady(bReady != 0);
	}
}

void UEchoGameInstance::EchoStartGame()
{
	if (AEchoFrontEndPlayerController* PC = Cast<AEchoFrontEndPlayerController>(GetFirstLocalPlayerController()))
	{
		PC->RequestStartGame();
	}
}

void UEchoGameInstance::EchoLeave()
{
	ReturnToTitle(FText::GetEmpty());
}

void UEchoGameInstance::EchoJoinCode(const FString& Code)
{
	GetSubsystem<UEchoSessionSubsystem>()->JoinByCode(Code);
}

void UEchoGameInstance::EchoNetStats()
{
	UEchoNetDiagnostics* Diagnostics = GetSubsystem<UEchoNetDiagnostics>();
	Diagnostics->SetOverlayShown(!Diagnostics->IsOverlayShown());
}

void UEchoGameInstance::EchoLogNote(const FString& Note)
{
	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("NOTE: %s"), *Note));
}

#undef LOCTEXT_NAMESPACE
