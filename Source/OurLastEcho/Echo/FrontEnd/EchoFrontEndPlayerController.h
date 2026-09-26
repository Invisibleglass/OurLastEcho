// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EchoFrontEndPlayerController.generated.h"

class UEchoUIRoot;

/**
 *  Player controller on the title screen and in the lobby. Looks through the title scene's drifting camera,
 *  shows the menus (the main menu, or the lobby when this is a hosted/joined game), and carries the lobby's
 *  Ready and Start requests to the server.
 */
UCLASS()
class AEchoFrontEndPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	AEchoFrontEndPlayerController();

	/** Lobby: marks this player ready or not (asks the server) */
	UFUNCTION(BlueprintCallable, Category="Echo|Lobby")
	void SetReady(bool bReady);

	/** Lobby, host: starts the game if both players are ready */
	UFUNCTION(BlueprintCallable, Category="Echo|Lobby")
	void RequestStartGame();

	/** A short message on this player's screen */
	UFUNCTION(Client, Reliable)
	void ClientShowToast(const FText& Message);

	UFUNCTION(BlueprintPure, Category="Echo|UI")
	UEchoUIRoot* GetUIRoot() const { return UIRoot; }

protected:

	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bReady);

	UFUNCTION(Server, Reliable)
	void ServerStartGame();

	/** Looks through the first AEchoTitleCamera in the level */
	void ViewTitleCamera();

	UPROPERTY(Transient)
	TObjectPtr<UEchoUIRoot> UIRoot;
};
