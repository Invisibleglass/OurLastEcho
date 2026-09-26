// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerState.h"
#include "EchoFrontEndGameMode.generated.h"

/**
 *  A player in the lobby: their role (the host plays Bat, the second player Saraa) and whether they're ready.
 *  Replicated, so both lobby screens show both players.
 */
UCLASS()
class AEchoLobbyPlayerState : public APlayerState
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category="Echo|Lobby")
	bool IsReady() const { return bReady; }

	UFUNCTION(BlueprintPure, Category="Echo|Lobby")
	bool IsHostPlayer() const { return bHostPlayer; }

	/** "Bat" for the host, "Saraa" for the other player */
	UFUNCTION(BlueprintPure, Category="Echo|Lobby")
	FText GetRoleName() const;

	void SetReady(bool bNewReady);
	void SetHostPlayer(bool bNewHost) { bHostPlayer = bNewHost; ForceNetUpdate(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	UPROPERTY(Replicated)
	bool bReady = false;

	UPROPERTY(Replicated)
	bool bHostPlayer = false;
};

/**
 *  Game mode for the title screen map. Standalone, it's just the title scene and the main menu. Opened as a
 *  listen server (Host Game), the same map is the lobby: up to two players, ready flags, and the host starts
 *  the game once both are ready (StartGame travels everyone to the gameplay level).
 *  No pawns: players look through the title scene's camera (AEchoTitleCamera).
 */
UCLASS()
class AEchoFrontEndGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	AEchoFrontEndGameMode();

	/** The level Start travels to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo|Lobby")
	FString GameplayMap = TEXT("/Game/Echo/Maps/Lvl_SpiritPath");

	/** Is this the lobby (a hosted game) rather than the plain title screen? */
	UFUNCTION(BlueprintPure, Category="Echo|Lobby")
	bool IsLobby() const;

	/** Both players present and ready */
	UFUNCTION(BlueprintPure, Category="Echo|Lobby")
	bool CanStartGame() const;

	/** Host only: everyone travels to the gameplay level. False if not everyone is ready */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Echo|Lobby")
	bool StartGame(APlayerController* RequestedBy);

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

protected:

	bool bStarting = false;
};
