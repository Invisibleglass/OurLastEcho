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

	/** Leaves any online session and loads the title screen, which will show Message (if any) */
	UFUNCTION(BlueprintCallable, Category="Echo|Front End")
	void ReturnToTitle(const FText& Message);

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

private:

	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	FText PendingMessage;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};
