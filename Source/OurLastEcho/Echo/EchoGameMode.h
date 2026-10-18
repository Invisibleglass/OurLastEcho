// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "OurLastEchoGameMode.h"
#include "EchoGameMode.generated.h"

class AOurLastEchoCharacter;

/**
 *  Co-op game mode: the first player to join plays Bat, everyone after plays Saraa (swapped with bHostPlaysSaraa).
 *  Pawn and controller classes are assigned in BP_EchoGameMode.
 */
UCLASS()
class AEchoGameMode : public AOurLastEchoGameMode
{
	GENERATED_BODY()

protected:

	/** Pawn for the first player (Bat, Living realm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Echo")
	TSubclassOf<APawn> BatPawnClass;

	/** Pawn for every other player (Saraa, Spirit realm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Echo")
	TSubclassOf<APawn> SaraaPawnClass;

	/** Swap roles: the first player (the host) plays Saraa and the joiner plays Bat. For testing Saraa as the host */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo")
	bool bHostPlaysSaraa = false;

	/** The first player (the host). Cleared automatically if they leave, so the next joiner takes their role */
	TWeakObjectPtr<AController> FirstController;

public:

	AEchoGameMode();

	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	/** In an online game, when the other player leaves, the host returns to the title screen too, with a message */
	virtual void Logout(AController* Exiting) override;

	/**
	 *  A player opened or closed their in-game menu. The game is paused for everyone while anyone has it open
	 *  (the engine's pause, which replicates), and resumes once nobody does.
	 */
	void SetPlayerInPauseMenu(APlayerController* PlayerController, bool bOpen);

	// ---- Combat (Milestone 8)

	/** A player went down. If both are down: the arena restarts (or both go back to their checkpoints) */
	void HandlePlayerDowned(AOurLastEchoCharacter* Character);

	/** A downed player's respawn delay is over: revive them beside the other player */
	void RespawnDownedPlayer(AOurLastEchoCharacter* Character);

	/** Seconds after both players are down before the arena restarts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo|Combat", meta = (ClampMin = 0))
	float AllDownRestartDelay = 2.5f;

	// ---- Travelling between levels together (the combat door)

	/**
	 *  Takes both players to another level. Online (EOS) it's a seamless travel, so the peer-to-peer connection and
	 *  the session (and voice) stay up; on LAN and in Play-In-Editor a normal server travel. Players arrive at the
	 *  PlayerStarts tagged ArrivalTag + "_Bat" / "_Saraa" (the level's own starts if there are none)
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Echo|Travel")
	bool TravelBothTo(const FString& MapPath, FName ArrivalTag);

	UFUNCTION(BlueprintPure, Category="Echo|Travel")
	bool IsTravelling() const { return bTravelling; }

	/** Is this controller the one who plays Bat (the host, unless roles are swapped)? */
	bool IsBatController(const AController* Controller) const;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

protected:

	bool CanUnpauseMenu() const;
	void RefreshMenuPause();
	void RestartAfterAllDown();

	TArray<TWeakObjectPtr<APlayerController>> PlayersInPauseMenu;

	/** Where players arrive (from the travel URL's ?EchoArrive=) */
	FName ArrivalTag;

	bool bTravelling = false;
	FTimerHandle AllDownTimer;
};
