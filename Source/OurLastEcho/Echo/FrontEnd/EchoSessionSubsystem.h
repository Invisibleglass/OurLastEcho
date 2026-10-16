// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "EchoSessionSubsystem.generated.h"

class FOnlineSessionSearch;

/** One game found by FindSessions, as the menus see it */
USTRUCT(BlueprintType)
struct FEchoSessionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Echo|Sessions")
	FString HostName;

	UPROPERTY(BlueprintReadOnly, Category="Echo|Sessions")
	int32 PingMs = 0;

	UPROPERTY(BlueprintReadOnly, Category="Echo|Sessions")
	int32 OpenSlots = 0;

	/** Pass to JoinSession */
	UPROPERTY(BlueprintReadOnly, Category="Echo|Sessions")
	int32 Index = INDEX_NONE;
};

/** An Epic friend, for the in-game invite list */
USTRUCT(BlueprintType)
struct FEchoFriendInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Echo|Sessions")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category="Echo|Sessions")
	bool bOnline = false;

	/** Pass to InviteFriend */
	UPROPERTY(BlueprintReadOnly, Category="Echo|Sessions")
	int32 Index = INDEX_NONE;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FEchoSessionResult, bool /*bSuccess*/, const FText& /*Error*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FEchoInviteReceived, const FString& /*FromName*/);

/**
 *  Hosting and joining games. The menus only ever talk to this subsystem, never to an online service.
 *
 *  It uses the world's online service (EchoOnline::GetSubsystem): EOS for real online play, Null for LAN.
 *  - Host: creates a 2-player session (an EOS lobby, with a voice room) carrying a short join code, then opens
 *    the title map as a listen server: the lobby. Nobody can find the game in a list over the internet: friends
 *    join with an invite (Epic overlay or the in-game friends list) or by typing the code. LAN games are also
 *    listed on the Join screen.
 *  - Join: by code (a search for that code), from the LAN list, or by accepting an invite. Then travel to the host.
 *  - EOS connections go peer to peer through EOS's relay; LAN ones use plain IP (?bUseIPSockets on the listen URL).
 */
UCLASS()
class UEchoSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** The map the lobby runs on (the title scene, opened with ?listen) */
	static const TCHAR* LobbyMap;

	/** Players per game: Bat and Saraa */
	static constexpr int32 MaxPlayers = 2;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Creates a session and, when it's up, opens the lobby as a listen server */
	UFUNCTION(BlueprintCallable, Category="Echo|Sessions")
	void HostSession();

	/** Searches for games on the network (LAN; results come with OnFindComplete) */
	UFUNCTION(BlueprintCallable, Category="Echo|Sessions")
	void FindSessions();

	/** Joins one of the found games and travels to it */
	UFUNCTION(BlueprintCallable, Category="Echo|Sessions")
	void JoinSession(int32 Index);

	/** Looks up the game with this join code and joins it (result with OnJoinComplete) */
	UFUNCTION(BlueprintCallable, Category="Echo|Sessions")
	void JoinByCode(const FString& Code);

	/** Gives up on a join that's still searching or connecting */
	UFUNCTION(BlueprintCallable, Category="Echo|Sessions")
	void CancelJoin();

	/** Ends or leaves this machine's session (no travel) */
	UFUNCTION(BlueprintCallable, Category="Echo|Sessions")
	void LeaveSession();

	UFUNCTION(BlueprintPure, Category="Echo|Sessions")
	bool IsInSession() const;

	UFUNCTION(BlueprintPure, Category="Echo|Sessions")
	bool IsSearching() const { return bSearching; }

	UFUNCTION(BlueprintPure, Category="Echo|Sessions")
	bool IsJoining() const { return bJoining; }

	UFUNCTION(BlueprintPure, Category="Echo|Sessions")
	const TArray<FEchoSessionInfo>& GetSearchResults() const { return Results; }

	/** Which online service is in use, e.g. "NULL" or "EOS" */
	UFUNCTION(BlueprintPure, Category="Echo|Sessions")
	FString GetOnlineServiceName() const;

	/** Internet play (EOS): invites and codes, no LAN list */
	UFUNCTION(BlueprintPure, Category="Echo|Sessions")
	bool IsInternetService() const;

	/** The current session's join code ("" when not in one) */
	UFUNCTION(BlueprintPure, Category="Echo|Sessions")
	FString GetJoinCode() const;

	/** Opens the Epic friends overlay to invite someone; false if the overlay isn't available */
	UFUNCTION(BlueprintCallable, Category="Echo|Sessions")
	bool OpenInviteOverlay();

	/** Reads the Epic friends list (results with OnFriendsRead) */
	UFUNCTION(BlueprintCallable, Category="Echo|Sessions")
	void ReadFriends();

	UFUNCTION(BlueprintPure, Category="Echo|Sessions")
	const TArray<FEchoFriendInfo>& GetFriends() const { return Friends; }

	/** Sends an invite to friend number Index of the last ReadFriends */
	UFUNCTION(BlueprintCallable, Category="Echo|Sessions")
	bool InviteFriend(int32 Index);

	/** An invite that arrived while the game was running and hasn't been answered */
	UFUNCTION(BlueprintPure, Category="Echo|Sessions")
	bool HasPendingInvite() const { return PendingInvite.IsValid(); }

	UFUNCTION(BlueprintPure, Category="Echo|Sessions")
	FString GetPendingInviteFrom() const { return PendingInviteFrom; }

	/** Accepts (joins) or declines the pending invite */
	UFUNCTION(BlueprintCallable, Category="Echo|Sessions")
	void AnswerPendingInvite(bool bAccept);

	FEchoSessionResult OnHostComplete;
	FEchoSessionResult OnFindComplete;
	FEchoSessionResult OnJoinComplete;
	FEchoSessionResult OnFriendsRead;
	FEchoInviteReceived OnInviteReceived;

	/** Seconds to wait for the host to answer before telling the player the join failed */
	static constexpr double JoinConnectTimeout = 20.0;

private:

	IOnlineSessionPtr GetSessionInterface() const;
	FString MakeJoinCode() const;
	FString ListenOptions() const;

	void CreateSessionNow();
	void HandleCreateComplete(FName SessionName, bool bSuccess);
	void HandleDestroyComplete(FName SessionName, bool bSuccess);
	void HandleFindComplete(bool bSuccess);
	void HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
	void HandleInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FromId, const FString& AppId, const FOnlineSessionSearchResult& InviteResult);

	/** Joins a search result; Source says where it came from for messages ("invite", "code", "list") */
	void JoinResult(const FOnlineSessionSearchResult& Result, const TCHAR* Source);
	void FailJoin(const FText& Error);

	/** Keeps the invite delegates on the current session interface (it can change with the world in PIE) */
	bool Tick(float DeltaTime);
	bool CheckJoinTimeout();

	TSharedPtr<FOnlineSessionSearch> Search;
	TArray<FEchoSessionInfo> Results;
	bool bSearching = false;
	bool bJoining = false;
	FString SearchCode;
	FString JoinSource;

	/** Host again once the old session is gone */
	bool bHostAfterDestroy = false;

	TArray<FEchoFriendInfo> Friends;
	TArray<FUniqueNetIdRef> FriendIds;

	TSharedPtr<FOnlineSessionSearchResult> PendingInvite;
	FString PendingInviteFrom;

	FDelegateHandle CreateHandle;
	FDelegateHandle DestroyHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle InviteAcceptedHandle;
	FDelegateHandle InviteReceivedHandle;
	TWeakPtr<IOnlineSession, ESPMode::ThreadSafe> InviteSessions;

	FTSTicker::FDelegateHandle TickHandle;
	double JoinStartedAt = 0.0;
	bool bTravelling = false;
};
