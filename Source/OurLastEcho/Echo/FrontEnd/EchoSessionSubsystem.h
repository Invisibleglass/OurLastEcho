// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
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

DECLARE_MULTICAST_DELEGATE_TwoParams(FEchoSessionResult, bool /*bSuccess*/, const FText& /*Error*/);

/**
 *  Hosting and joining games. The menus only ever talk to this subsystem, never to an online service.
 *
 *  It uses whichever Online Subsystem is configured as the default platform service ([OnlineSubsystem]
 *  DefaultPlatformService in DefaultEngine.ini). Today that's Null: LAN sessions, found by broadcast, fine for
 *  testing on one network. Switching to Epic Online Services later is a config change (DefaultPlatformService=EOS
 *  plus the EOS settings) and a login step before hosting; the calls below are the standard session interface,
 *  so the menus don't change. Anything backend-specific is marked "Null:" below.
 *
 *  Flow: HostSession creates the session, then opens the title map as a listen server, which is the lobby.
 *  JoinSession joins the chosen session and travels to the host. LeaveSession ends it (the caller travels).
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

	virtual void Deinitialize() override;

	/** Creates a session and, when it's up, opens the lobby as a listen server */
	UFUNCTION(BlueprintCallable, Category="Echo|Sessions")
	void HostSession();

	/** Searches for games (results come with OnFindComplete) */
	UFUNCTION(BlueprintCallable, Category="Echo|Sessions")
	void FindSessions();

	/** Joins one of the found games and travels to it */
	UFUNCTION(BlueprintCallable, Category="Echo|Sessions")
	void JoinSession(int32 Index);

	/** Ends or leaves this machine's session (no travel) */
	UFUNCTION(BlueprintCallable, Category="Echo|Sessions")
	void LeaveSession();

	UFUNCTION(BlueprintPure, Category="Echo|Sessions")
	bool IsInSession() const;

	UFUNCTION(BlueprintPure, Category="Echo|Sessions")
	bool IsSearching() const { return bSearching; }

	UFUNCTION(BlueprintPure, Category="Echo|Sessions")
	const TArray<FEchoSessionInfo>& GetSearchResults() const { return Results; }

	/** Which online service is in use, e.g. "NULL" or "EOS" */
	UFUNCTION(BlueprintPure, Category="Echo|Sessions")
	FString GetOnlineServiceName() const;

	FEchoSessionResult OnHostComplete;
	FEchoSessionResult OnFindComplete;
	FEchoSessionResult OnJoinComplete;

private:

	IOnlineSessionPtr GetSessionInterface() const;
	bool IsLanService() const;

	void CreateSessionNow();
	void HandleCreateComplete(FName SessionName, bool bSuccess);
	void HandleDestroyComplete(FName SessionName, bool bSuccess);
	void HandleFindComplete(bool bSuccess);
	void HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	TSharedPtr<FOnlineSessionSearch> Search;
	TArray<FEchoSessionInfo> Results;
	bool bSearching = false;

	/** Host again once the old session is gone */
	bool bHostAfterDestroy = false;

	FDelegateHandle CreateHandle;
	FDelegateHandle DestroyHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
};
