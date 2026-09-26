// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "OurLastEchoGameMode.h"
#include "EchoGameMode.generated.h"

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

protected:

	bool CanUnpauseMenu() const;
	void RefreshMenuPause();

	TArray<TWeakObjectPtr<APlayerController>> PlayersInPauseMenu;
};
