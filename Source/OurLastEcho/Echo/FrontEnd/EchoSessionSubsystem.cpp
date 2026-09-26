// Our Last Echo

#include "EchoSessionSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OurLastEcho.h"

#define LOCTEXT_NAMESPACE "EchoSessions"

const TCHAR* UEchoSessionSubsystem::LobbyMap = TEXT("/Game/Echo/Maps/TitleScreen");

namespace
{
	/** Marks our sessions, so a search only lists Our Last Echo games */
	const FName GameKey(TEXT("ECHO_GAME"));
	const FString GameValue(TEXT("OurLastEcho"));
}

void UEchoSessionSubsystem::Deinitialize()
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	}
	FTSTicker::RemoveTicker(JoinTimeoutHandle);
	Super::Deinitialize();
}

IOnlineSessionPtr UEchoSessionSubsystem::GetSessionInterface() const
{
	// Per world, so each Play-In-Editor player gets its own online instance
	return Online::GetSessionInterface(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
}

FString UEchoSessionSubsystem::GetOnlineServiceName() const
{
	const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	return Subsystem ? Subsystem->GetSubsystemName().ToString() : TEXT("none");
}

bool UEchoSessionSubsystem::IsLanService() const
{
	// Null: LAN only. Real services (EOS) use internet sessions
	return GetOnlineServiceName() == TEXT("NULL");
}

bool UEchoSessionSubsystem::IsInSession() const
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	return Sessions && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

void UEchoSessionSubsystem::HostSession()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions)
	{
		OnHostComplete.Broadcast(false, LOCTEXT("NoOnline", "Online play isn't available."));
		return;
	}

	// A session left over from an earlier game has to go first
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		bHostAfterDestroy = true;
		LeaveSession();
		return;
	}
	CreateSessionNow();
}

void UEchoSessionSubsystem::CreateSessionNow()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = MaxPlayers;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = false;
	Settings.bIsLANMatch = IsLanService();
	Settings.bUsesPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.Set(GameKey, GameValue, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UEchoSessionSubsystem::HandleCreateComplete));

	UE_LOG(LogOurLastEcho, Log, TEXT("Sessions: hosting on %s"), *GetOnlineServiceName());
	if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
	{
		HandleCreateComplete(NAME_GameSession, false);
	}
}

void UEchoSessionSubsystem::HandleCreateComplete(FName SessionName, bool bSuccess)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	}

	UE_LOG(LogOurLastEcho, Log, TEXT("Sessions: create %s"), bSuccess ? TEXT("succeeded") : TEXT("failed"));
	OnHostComplete.Broadcast(bSuccess, bSuccess ? FText::GetEmpty() : LOCTEXT("HostFailed", "Couldn't create a game. Please try again."));

	if (bSuccess)
	{
		// The lobby: the title scene as a listen server. Null answers searches with the listen port from here on
		UGameplayStatics::OpenLevel(GetGameInstance()->GetWorld(), FName(LobbyMap), true, TEXT("listen"));
	}
}

void UEchoSessionSubsystem::LeaveSession()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions || !Sessions->GetNamedSession(NAME_GameSession))
	{
		if (bHostAfterDestroy)
		{
			bHostAfterDestroy = false;
			CreateSessionNow();
		}
		return;
	}

	Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &UEchoSessionSubsystem::HandleDestroyComplete));
	if (!Sessions->DestroySession(NAME_GameSession))
	{
		HandleDestroyComplete(NAME_GameSession, false);
	}
}

void UEchoSessionSubsystem::HandleDestroyComplete(FName SessionName, bool bSuccess)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}
	UE_LOG(LogOurLastEcho, Log, TEXT("Sessions: left session (%s)"), bSuccess ? TEXT("ok") : TEXT("failed"));

	if (bHostAfterDestroy)
	{
		bHostAfterDestroy = false;
		CreateSessionNow();
	}
}

void UEchoSessionSubsystem::FindSessions()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions || bSearching)
	{
		if (!Sessions)
		{
			OnFindComplete.Broadcast(false, LOCTEXT("NoOnlineFind", "Online play isn't available."));
		}
		return;
	}

	Search = MakeShared<FOnlineSessionSearch>();
	Search->MaxSearchResults = 20;
	Search->bIsLanQuery = IsLanService();
	Search->TimeoutInSeconds = 3.0f;
	Search->QuerySettings.Set(GameKey, GameValue, EOnlineComparisonOp::Equals);

	Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UEchoSessionSubsystem::HandleFindComplete));

	bSearching = true;
	Results.Reset();
	if (!Sessions->FindSessions(0, Search.ToSharedRef()))
	{
		HandleFindComplete(false);
	}
}

void UEchoSessionSubsystem::HandleFindComplete(bool bSuccess)
{
	bSearching = false;
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	}

	Results.Reset();
	if (bSuccess && Search.IsValid())
	{
		for (int32 Index = 0; Index < Search->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& Result = Search->SearchResults[Index];
			FString Value;
			// Null: LAN beacons also answer for other games' sessions; only list ours, and only ones with room
			if (!Result.Session.SessionSettings.Get(GameKey, Value) || Value != GameValue || Result.Session.NumOpenPublicConnections <= 0)
			{
				continue;
			}
			FEchoSessionInfo& Info = Results.AddDefaulted_GetRef();
			Info.HostName = Result.Session.OwningUserName;
			Info.PingMs = Result.PingInMs;
			Info.OpenSlots = Result.Session.NumOpenPublicConnections;
			Info.Index = Index;
		}
	}

	UE_LOG(LogOurLastEcho, Log, TEXT("Sessions: search %s, %d game(s)"), bSuccess ? TEXT("done") : TEXT("failed"), Results.Num());
	OnFindComplete.Broadcast(bSuccess, bSuccess ? FText::GetEmpty() : LOCTEXT("FindFailed", "Couldn't search for games."));
}

void UEchoSessionSubsystem::JoinSession(int32 Index)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions || !Search.IsValid() || !Search->SearchResults.IsValidIndex(Index))
	{
		OnJoinComplete.Broadcast(false, LOCTEXT("JoinGone", "That game is no longer available."));
		return;
	}

	// Can't join while still in an old session
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		Sessions->DestroySession(NAME_GameSession);
	}

	Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UEchoSessionSubsystem::HandleJoinComplete));
	if (!Sessions->JoinSession(0, NAME_GameSession, Search->SearchResults[Index]))
	{
		HandleJoinComplete(NAME_GameSession, EOnJoinSessionCompleteResult::UnknownError);
	}
}

void UEchoSessionSubsystem::HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions)
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	}

	FString Url;
	const bool bJoined = Result == EOnJoinSessionCompleteResult::Success && Sessions && Sessions->GetResolvedConnectString(SessionName, Url);
	APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
	if (!bJoined || !PC)
	{
		const FText Error =
			Result == EOnJoinSessionCompleteResult::SessionIsFull ? LOCTEXT("JoinFull", "That game is full.") :
			Result == EOnJoinSessionCompleteResult::SessionDoesNotExist ? LOCTEXT("JoinMissing", "That game is no longer available.") :
			LOCTEXT("JoinFailed", "Couldn't join that game.");
		UE_LOG(LogOurLastEcho, Warning, TEXT("Sessions: join failed (%d)"), static_cast<int32>(Result));
		if (Sessions && Sessions->GetNamedSession(NAME_GameSession))
		{
			Sessions->DestroySession(NAME_GameSession);
		}
		OnJoinComplete.Broadcast(false, Error);
		return;
	}

	// Success is reported by arriving in the host's lobby (this world, and the Join screen, get replaced); until
	// then the Join screen keeps showing "Joining...", and CheckJoinTimeout reports a host that never answers
	UE_LOG(LogOurLastEcho, Log, TEXT("Sessions: joined, travelling to %s"), *Url);
	PC->ClientTravel(Url, TRAVEL_Absolute);
	JoinStartedAt = FPlatformTime::Seconds();
	FTSTicker::RemoveTicker(JoinTimeoutHandle);
	JoinTimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UEchoSessionSubsystem::CheckJoinTimeout), 0.5f);
}

bool UEchoSessionSubsystem::CheckJoinTimeout(float DeltaTime)
{
	UGameInstance* GameInstance = GetGameInstance();
	FWorldContext* Context = GameInstance ? GameInstance->GetWorldContext() : nullptr;
	const bool bConnecting = Context && (Context->PendingNetGame || !Context->TravelURL.IsEmpty());
	if (!bConnecting)
	{
		// Connected (or failed, which the game instance reports): nothing more to do
		JoinTimeoutHandle.Reset();
		return false;
	}
	if (FPlatformTime::Seconds() - JoinStartedAt < JoinConnectTimeout)
	{
		return true;
	}

	UE_LOG(LogOurLastEcho, Warning, TEXT("Sessions: the host didn't answer in %.0f s, giving up on the join"), JoinConnectTimeout);
	JoinTimeoutHandle.Reset();
	GEngine->CancelPending(Context->World());
	Context->TravelURL.Empty();
	LeaveSession();
	OnJoinComplete.Broadcast(false, LOCTEXT("JoinNoAnswer", "Couldn't connect to that game. It may have closed."));
	return false;
}

#undef LOCTEXT_NAMESPACE
