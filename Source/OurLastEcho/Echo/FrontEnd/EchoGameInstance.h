// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "EchoGameInstance.generated.h"

class UNetDriver;

/**
 *  The game instance (GameInstanceClass in DefaultEngine.ini). It lives for the whole run, across map changes, so it:
 *  - carries a message to show on the title screen after returning there ("The host left the game.")
 *  - turns network and travel failures into such messages (the engine then returns to the default map, the title)
 *  - applies the saved settings at start-up
 *  - has console commands to drive hosting and joining without the menus (used by tests on two game processes)
 */
UCLASS()
class UEchoGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:

	/** The title screen map (also the game's default map) */
	static const TCHAR* TitleMap;

	virtual void Init() override;
	virtual void Shutdown() override;

	/** Leaves any online session and loads the title screen, which will show Message (if any). A host first tells
	 *  the other player, who goes to their title screen with "The host left the game." */
	UFUNCTION(BlueprintCallable, Category="Echo|Front End")
	void ReturnToTitle(const FText& Message);

	/** Host only: back to this host's lobby (the session and join code stay), showing Message */
	UFUNCTION(BlueprintCallable, Category="Echo|Front End")
	void ReturnToLobby(const FText& Message);

	/** The host sent us back (ClientReturnToMainMenuWithTextReason): the title screen, with why */
	virtual void ReturnToMainMenu() override;

	/** A message the title screen shows once it opens */
	UFUNCTION(BlueprintCallable, Category="Echo|Front End")
	void SetPendingMessage(const FText& Message) { PendingMessage = Message; }

	/** Returns and clears the pending message */
	UFUNCTION(BlueprintCallable, Category="Echo|Front End")
	FText ConsumePendingMessage();

	UFUNCTION(BlueprintPure, Category="Echo|Front End")
	FText PeekPendingMessage() const { return PendingMessage; }

	// ---- Console commands (tests and debugging)

	/** Hosts a game, like Play > Host Game */
	UFUNCTION(Exec)
	void EchoHost();

	/** Searches for games; results go to the log */
	UFUNCTION(Exec)
	void EchoFindGames();

	/** Joins game number Index from the last search */
	UFUNCTION(Exec)
	void EchoJoinGame(int32 Index);

	/** In the lobby: ready (1) or not (0) */
	UFUNCTION(Exec)
	void EchoReady(int32 bReady);

	/** In the lobby, host only: starts the game once both players are ready */
	UFUNCTION(Exec)
	void EchoStartGame();

	/** Leaves the lobby or the game for the title screen */
	UFUNCTION(Exec)
	void EchoLeave();

	/** Joins the game with this join code */
	UFUNCTION(Exec)
	void EchoJoinCode(const FString& Code);

	/** Shows or hides the network overlay (ping, packet loss, bandwidth); saved like the setting */
	UFUNCTION(Exec)
	void EchoNetStats();

	/** Writes a note into the online session log (testers can mark moments: "EchoLogNote saw a pop") */
	UFUNCTION(Exec)
	void EchoLogNote(const FString& Note);

private:

	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	FText PendingMessage;

	/** Inside ReturnToTitle (it must never run inside itself) */
	bool bReturningToTitle = false;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};
