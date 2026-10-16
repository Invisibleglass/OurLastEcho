// Our Last Echo

#include "EchoSessionSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "EchoGameInstance.h"
#include "EchoNetDiagnostics.h"
#include "EchoOnline.h"
#include "OurLastEcho.h"

#define LOCTEXT_NAMESPACE "EchoSessions"

const TCHAR* UEchoSessionSubsystem::LobbyMap = TEXT("/Game/Echo/Maps/TitleScreen");

namespace
{
	/** Marks our sessions, so a search only finds Our Last Echo games */
	const FName GameKey(TEXT("ECHO_GAME"));
	const FString GameValue(TEXT("OurLastEcho"));

	/** The join code the host's lobby shows */
	const FName CodeKey(TEXT("ECHO_CODE"));

	/** EOS keeps games in "buckets"; ours is fixed, so the editor's and packaged builds find each other
	 *  (OSS EOS's own default is the build's network version) */
	const FName BucketKey(TEXT("OSSEOS_BUCKET_ID_ATTRIBUTE_KEY"));
	const FString BucketValue(TEXT("OurLastEcho"));

	/** OSS EOS searches lobbies only when asked to */
	const FName LobbiesKey(TEXT("LOBBYSEARCH"));
}

void UEchoSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UEchoSessionSubsystem::Tick), 0.5f);
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
	if (IOnlineSessionPtr InviteInterface = InviteSessions.Pin())
	{
		InviteInterface->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
		InviteInterface->ClearOnSessionInviteReceivedDelegate_Handle(InviteReceivedHandle);
	}
	FTSTicker::RemoveTicker(TickHandle);
	Super::Deinitialize();
}

IOnlineSessionPtr UEchoSessionSubsystem::GetSessionInterface() const
{
	const IOnlineSubsystem* Subsystem = EchoOnline::GetSubsystem(GetGameInstance());
	return Subsystem ? Subsystem->GetSessionInterface() : nullptr;
}

FString UEchoSessionSubsystem::GetOnlineServiceName() const
{
	return EchoOnline::GetServiceName(GetGameInstance());
}

bool UEchoSessionSubsystem::IsInternetService() const
{
	return EchoOnline::IsEOS(GetGameInstance());
}

bool UEchoSessionSubsystem::IsInSession() const
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	return Sessions && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

FString UEchoSessionSubsystem::GetJoinCode() const
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	const FNamedOnlineSession* Session = Sessions ? Sessions->GetNamedSession(NAME_GameSession) : nullptr;
	FString Code;
	if (Session)
	{
		Session->SessionSettings.Get(CodeKey, Code);
	}
	return Code;
}

FString UEchoSessionSubsystem::MakeJoinCode() const
{
	const UEchoOnlineSettings* Settings = GetDefault<UEchoOnlineSettings>();
	FString Code;
	for (int32 Index = 0; Index < Settings->JoinCodeLength; ++Index)
	{
		Code.AppendChar(Settings->JoinCodeAlphabet[FMath::RandRange(0, Settings->JoinCodeAlphabet.Len() - 1)]);
	}
	return Code;
}

FString UEchoSessionSubsystem::ListenOptions() const
{
	// LAN games connect by IP; EOS games peer to peer through EOS (the EOS net driver decides by this option)
	return IsInternetService() ? TEXT("listen") : TEXT("listen?bUseIPSockets");
}

// ------------------------------------------------------------------ hosting

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
	if (!Sessions)
	{
		OnHostComplete.Broadcast(false, LOCTEXT("NoOnline2", "Online play isn't available."));
		return;
	}

	const FString Code = MakeJoinCode();
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = MaxPlayers;
	// Advertised so a search for the join code can find it; nothing lists internet games, so in practice only
	// people with the code or an invite get in
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = false;
	Settings.bIsLANMatch = !IsInternetService();
	Settings.bUsesPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bUseLobbiesVoiceChatIfAvailable = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bAllowInvites = true;
	Settings.Set(GameKey, GameValue, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(CodeKey, Code, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(BucketKey, BucketValue, EOnlineDataAdvertisementType::ViaOnlineService);

	Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UEchoSessionSubsystem::HandleCreateComplete));

	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Host: creating a game on %s (join code %s)"), *GetOnlineServiceName(), *Code));
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

	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Host: create %s"), bSuccess ? TEXT("succeeded, opening the lobby") : TEXT("FAILED")));
	OnHostComplete.Broadcast(bSuccess, bSuccess ? FText::GetEmpty() : LOCTEXT("HostFailed", "Couldn't create a game. Check your connection and try again."));

	if (bSuccess)
	{
		// The lobby: the title scene as a listen server
		UGameplayStatics::OpenLevel(GetGameInstance()->GetWorld(), FName(LobbyMap), true, ListenOptions());
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

	UEchoNetDiagnostics::Log(this, TEXT("Session: leaving"));
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

// ------------------------------------------------------------------ finding

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
	Search->bIsLanQuery = !IsInternetService();
	Search->TimeoutInSeconds = 3.0f;
	Search->QuerySettings.Set(GameKey, GameValue, EOnlineComparisonOp::Equals);
	Search->QuerySettings.Set(BucketKey, BucketValue, EOnlineComparisonOp::Equals);
	Search->QuerySettings.Set(LobbiesKey, true, EOnlineComparisonOp::Equals);
	if (!SearchCode.IsEmpty() && IsInternetService())
	{
		// EOS filters by the code on its servers; LAN answers are filtered below
		Search->QuerySettings.Set(CodeKey, SearchCode, EOnlineComparisonOp::Equals);
	}

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
	int32 CodeMatch = INDEX_NONE;
	bool bCodeMatchFull = false;
	if (bSuccess && Search.IsValid())
	{
		for (int32 Index = 0; Index < Search->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& Result = Search->SearchResults[Index];
			FString Value;
			// LAN beacons also answer for other games' sessions: only ours
			if (!Result.Session.SessionSettings.Get(GameKey, Value) || Value != GameValue)
			{
				continue;
			}
			FString Code;
			Result.Session.SessionSettings.Get(CodeKey, Code);
			if (!SearchCode.IsEmpty() && Code.Equals(SearchCode, ESearchCase::IgnoreCase))
			{
				CodeMatch = Index;
				bCodeMatchFull = Result.Session.NumOpenPublicConnections <= 0;
			}
			if (Result.Session.NumOpenPublicConnections <= 0)
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

	if (!SearchCode.IsEmpty())
	{
		// A join by code: join the match, or say why not
		const FString Code = SearchCode;
		SearchCode.Reset();
		UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Join by code %s: %s"), *Code,
			!bSuccess ? TEXT("search failed") : CodeMatch == INDEX_NONE ? TEXT("no game") : bCodeMatchFull ? TEXT("game is full") : TEXT("found, joining")));
		if (!bSuccess)
		{
			FailJoin(LOCTEXT("CodeSearchFailed", "Couldn't look up that code. Check your connection and try again."));
		}
		else if (CodeMatch == INDEX_NONE)
		{
			FailJoin(FText::Format(LOCTEXT("CodeNotFound", "No game with the code {0}. Check the code, and that the host is still in their lobby."), FText::FromString(Code)));
		}
		else if (bCodeMatchFull)
		{
			FailJoin(LOCTEXT("CodeFull", "That game is full."));
		}
		else
		{
			JoinResult(Search->SearchResults[CodeMatch], TEXT("code"));
		}
		return;
	}

	UE_LOG(LogOurLastEcho, Log, TEXT("Sessions: search %s, %d game(s)"), bSuccess ? TEXT("done") : TEXT("failed"), Results.Num());
	OnFindComplete.Broadcast(bSuccess, bSuccess ? FText::GetEmpty() : LOCTEXT("FindFailed", "Couldn't search for games."));
}

// ------------------------------------------------------------------ joining

void UEchoSessionSubsystem::JoinByCode(const FString& InCode)
{
	FString Code = InCode.TrimStartAndEnd().ToUpper();
	Code.ReplaceInline(TEXT(" "), TEXT(""));
	Code.ReplaceInline(TEXT("-"), TEXT(""));
	if (Code.Len() != GetDefault<UEchoOnlineSettings>()->JoinCodeLength)
	{
		OnJoinComplete.Broadcast(false, FText::Format(LOCTEXT("CodeLength", "Join codes are {0} letters and numbers."), FText::AsNumber(GetDefault<UEchoOnlineSettings>()->JoinCodeLength)));
		return;
	}
	if (bSearching || bJoining)
	{
		return;
	}
	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Join by code %s: searching"), *Code));
	bJoining = true;
	JoinSource = TEXT("code");
	SearchCode = Code;
	FindSessions();
}

void UEchoSessionSubsystem::JoinSession(int32 Index)
{
	if (!Search.IsValid() || !Search->SearchResults.IsValidIndex(Index))
	{
		OnJoinComplete.Broadcast(false, LOCTEXT("JoinGone", "That game is no longer available."));
		return;
	}
	JoinResult(Search->SearchResults[Index], TEXT("list"));
}

void UEchoSessionSubsystem::JoinResult(const FOnlineSessionSearchResult& Result, const TCHAR* Source)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions)
	{
		FailJoin(LOCTEXT("NoOnlineJoin", "Online play isn't available."));
		return;
	}

	bJoining = true;
	bTravelling = false;
	JoinSource = Source;
	JoinStartedAt = FPlatformTime::Seconds();

	// Can't join while still in an old session (e.g. our own lobby, when accepting an invite)
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		Sessions->DestroySession(NAME_GameSession);
	}

	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Join (%s): joining %s's game"), Source, *Result.Session.OwningUserName));
	Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UEchoSessionSubsystem::HandleJoinComplete));
	if (!Sessions->JoinSession(0, NAME_GameSession, Result))
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
	if (!bJoining)
	{
		// Cancelled meanwhile
		if (Sessions && Result == EOnJoinSessionCompleteResult::Success)
		{
			Sessions->DestroySession(NAME_GameSession);
		}
		return;
	}

	FString Url;
	const bool bJoined = Result == EOnJoinSessionCompleteResult::Success && Sessions && Sessions->GetResolvedConnectString(SessionName, Url);
	APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
	if (!bJoined || !PC)
	{
		const bool bInvite = JoinSource == TEXT("invite");
		const FText Error =
			Result == EOnJoinSessionCompleteResult::SessionIsFull ? LOCTEXT("JoinFull", "That game is full.") :
			Result == EOnJoinSessionCompleteResult::SessionDoesNotExist ? (bInvite
				? LOCTEXT("InviteExpired", "That invite has expired: the game it was for has ended.")
				: LOCTEXT("JoinMissing", "That game is no longer available.")) :
			LOCTEXT("JoinFailed", "Couldn't join that game.");
		UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Join: FAILED (result %d)"), static_cast<int32>(Result)));
		if (Sessions && Sessions->GetNamedSession(NAME_GameSession))
		{
			Sessions->DestroySession(NAME_GameSession);
		}
		FailJoin(Error);
		return;
	}

	// Success is reported by arriving in the host's lobby (this world and the Join screen get replaced); until then
	// the Join screen keeps showing "Joining...", and the tick reports a host that never answers
	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Join: in the session, connecting to the host (%s)"), Url.StartsWith(TEXT("EOS:")) ? TEXT("EOS peer to peer") : *Url));
	bTravelling = true;
	JoinStartedAt = FPlatformTime::Seconds();
	PC->ClientTravel(Url, TRAVEL_Absolute);
}

void UEchoSessionSubsystem::FailJoin(const FText& Error)
{
	const bool bInvite = JoinSource == TEXT("invite");
	bJoining = false;
	bTravelling = false;
	JoinSource.Reset();
	OnJoinComplete.Broadcast(false, Error);
	if (bInvite)
	{
		// Accepted from the overlay: no Join screen is waiting, so say it on the title screen
		if (UEchoGameInstance* GameInstance = Cast<UEchoGameInstance>(GetGameInstance()))
		{
			GameInstance->ReturnToTitle(Error);
		}
	}
}

void UEchoSessionSubsystem::CancelJoin()
{
	if (!bJoining)
	{
		return;
	}
	UEchoNetDiagnostics::Log(this, TEXT("Join: cancelled by the player"));
	SearchCode.Reset();
	bJoining = false;
	if (bTravelling)
	{
		UGameInstance* GameInstance = GetGameInstance();
		FWorldContext* Context = GameInstance ? GameInstance->GetWorldContext() : nullptr;
		if (Context)
		{
			GEngine->CancelPending(Context->World());
			Context->TravelURL.Empty();
		}
	}
	bTravelling = false;
	JoinSource.Reset();
	LeaveSession();
}

bool UEchoSessionSubsystem::Tick(float DeltaTime)
{
	// Invites: keep our handlers on the current session interface
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions != InviteSessions.Pin())
	{
		if (IOnlineSessionPtr Old = InviteSessions.Pin())
		{
			Old->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
			Old->ClearOnSessionInviteReceivedDelegate_Handle(InviteReceivedHandle);
		}
		InviteSessions = Sessions;
		if (Sessions)
		{
			InviteAcceptedHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UEchoSessionSubsystem::HandleInviteAccepted));
			InviteReceivedHandle = Sessions->AddOnSessionInviteReceivedDelegate_Handle(FOnSessionInviteReceivedDelegate::CreateUObject(this, &UEchoSessionSubsystem::HandleInviteReceived));
		}
	}

	if (bTravelling)
	{
		CheckJoinTimeout();
	}
	return true;
}

bool UEchoSessionSubsystem::CheckJoinTimeout()
{
	UGameInstance* GameInstance = GetGameInstance();
	FWorldContext* Context = GameInstance ? GameInstance->GetWorldContext() : nullptr;
	const bool bConnecting = Context && (Context->PendingNetGame || !Context->TravelURL.IsEmpty());
	if (!bConnecting)
	{
		// Connected (or failed, which the game instance reports): nothing more to do
		bTravelling = false;
		bJoining = false;
		return false;
	}
	if (FPlatformTime::Seconds() - JoinStartedAt < JoinConnectTimeout)
	{
		return true;
	}

	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Join: the host didn't answer in %.0f s, giving up"), JoinConnectTimeout));
	bTravelling = false;
	GEngine->CancelPending(Context->World());
	Context->TravelURL.Empty();
	LeaveSession();
	FailJoin(LOCTEXT("JoinNoAnswer", "Couldn't connect to that game. It may have closed, or the connection between you is blocked."));
	return false;
}

// ------------------------------------------------------------------ invites

bool UEchoSessionSubsystem::OpenInviteOverlay()
{
	const IOnlineSubsystem* Subsystem = EchoOnline::GetSubsystem(GetGameInstance());
	const IOnlineExternalUIPtr ExternalUI = Subsystem ? Subsystem->GetExternalUIInterface() : nullptr;
	// OSS EOS has no invite dialog of its own; the friends overlay invites to the current (presence) lobby
	const bool bShown = ExternalUI && IsInternetService() && ExternalUI->ShowFriendsUI(0);
	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Invite: Epic friends overlay %s"), bShown ? TEXT("opened") : TEXT("not available")));
	return bShown;
}

void UEchoSessionSubsystem::ReadFriends()
{
	const IOnlineSubsystem* Subsystem = EchoOnline::GetSubsystem(GetGameInstance());
	const IOnlineFriendsPtr FriendsInterface = Subsystem ? Subsystem->GetFriendsInterface() : nullptr;
	if (!FriendsInterface || !IsInternetService())
	{
		Friends.Reset();
		FriendIds.Reset();
		OnFriendsRead.Broadcast(false, LOCTEXT("NoFriends", "Friends lists need Epic Online Services."));
		return;
	}

	TWeakObjectPtr<UEchoSessionSubsystem> WeakThis(this);
	FriendsInterface->ReadFriendsList(0, EFriendsLists::ToString(EFriendsLists::Default), FOnReadFriendsListComplete::CreateLambda(
		[WeakThis, FriendsInterface](int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			WeakThis->Friends.Reset();
			WeakThis->FriendIds.Reset();
			TArray<TSharedRef<FOnlineFriend>> List;
			if (bWasSuccessful)
			{
				FriendsInterface->GetFriendsList(0, ListName, List);
			}
			for (const TSharedRef<FOnlineFriend>& Friend : List)
			{
				FEchoFriendInfo& Info = WeakThis->Friends.AddDefaulted_GetRef();
				Info.Name = Friend->GetDisplayName();
				Info.bOnline = Friend->GetPresence().bIsOnline;
				Info.Index = WeakThis->FriendIds.Num();
				WeakThis->FriendIds.Add(Friend->GetUserId());
			}
			UEchoNetDiagnostics::Log(WeakThis.Get(), FString::Printf(TEXT("Invite: friends list read (%s, %d friends)"), bWasSuccessful ? TEXT("ok") : *ErrorStr, WeakThis->Friends.Num()));
			WeakThis->OnFriendsRead.Broadcast(bWasSuccessful, bWasSuccessful ? FText::GetEmpty() : LOCTEXT("FriendsFailed", "Couldn't read your Epic friends list."));
		}));
}

bool UEchoSessionSubsystem::InviteFriend(int32 Index)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions || !FriendIds.IsValidIndex(Index) || !IsInSession())
	{
		return false;
	}
	const bool bSent = Sessions->SendSessionInviteToFriend(0, NAME_GameSession, *FriendIds[Index]);
	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Invite: to %s %s"), *Friends[Index].Name, bSent ? TEXT("sent") : TEXT("FAILED")));
	return bSent;
}

void UEchoSessionSubsystem::HandleInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FromId, const FString& AppId, const FOnlineSessionSearchResult& InviteResult)
{
	if (!InviteResult.IsValid())
	{
		return;
	}
	PendingInvite = MakeShared<FOnlineSessionSearchResult>(InviteResult);
	PendingInviteFrom = InviteResult.Session.OwningUserName;
	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Invite: received from %s"), *PendingInviteFrom));
	OnInviteReceived.Broadcast(PendingInviteFrom);
}

void UEchoSessionSubsystem::AnswerPendingInvite(bool bAccept)
{
	if (!PendingInvite.IsValid())
	{
		return;
	}
	const TSharedPtr<FOnlineSessionSearchResult> Invite = PendingInvite;
	PendingInvite.Reset();
	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Invite: from %s %s in game"), *PendingInviteFrom, bAccept ? TEXT("accepted") : TEXT("declined")));
	if (bAccept)
	{
		JoinResult(*Invite, TEXT("invite"));
	}
}

void UEchoSessionSubsystem::HandleInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Invite: accepted in the Epic overlay (%s)"), bWasSuccessful && InviteResult.IsValid() ? TEXT("joining") : TEXT("the game is gone")));
	PendingInvite.Reset();
	JoinSource = TEXT("invite");
	if (!bWasSuccessful || !InviteResult.IsValid())
	{
		FailJoin(LOCTEXT("InviteGone", "That invite has expired: the game it was for has ended."));
		return;
	}
	JoinResult(InviteResult, TEXT("invite"));
}

#undef LOCTEXT_NAMESPACE
