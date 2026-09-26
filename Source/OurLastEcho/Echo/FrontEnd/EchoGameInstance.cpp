// Our Last Echo

#include "EchoGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
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
	UE_LOG(LogOurLastEcho, Log, TEXT("Returning to the title screen%s%s"), Message.IsEmpty() ? TEXT("") : TEXT(": "), *Message.ToString());
	SetPendingMessage(Message);
	GetSubsystem<UEchoSessionSubsystem>()->LeaveSession();

	// Opening a map without ?listen also closes our listen server, which disconnects the other player
	UGameplayStatics::OpenLevel(GetWorld(), FName(TitleMap), true);
}

void UEchoGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// Only our own world's connection (in PIE every player's failures come through the same engine delegate)
	if (World != GetWorld())
	{
		return;
	}

	const bool bWasClient = NetDriver && NetDriver->ServerConnection != nullptr;
	UE_LOG(LogOurLastEcho, Log, TEXT("Network failure (%s): %s"), ENetworkFailure::ToString(FailureType), *ErrorString);
	if (PendingMessage.IsEmpty())
	{
		SetPendingMessage(bWasClient
			? LOCTEXT("HostLeft", "The host left the game.")
			: LOCTEXT("ConnectionLost", "The connection to the other player was lost."));
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

	UE_LOG(LogOurLastEcho, Log, TEXT("Travel failure (%s): %s"), ETravelFailure::ToString(FailureType), *ErrorString);
	SetPendingMessage(LOCTEXT("JoinTravelFailed", "Couldn't connect to that game."));
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

#undef LOCTEXT_NAMESPACE
