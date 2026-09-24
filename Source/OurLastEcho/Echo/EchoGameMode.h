// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "OurLastEchoGameMode.h"
#include "EchoGameMode.generated.h"

/**
 *  Co-op game mode: the first player to join plays Bat, everyone after plays Saraa.
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

	/** Whoever is currently Bat. Cleared automatically if they leave, so the next joiner becomes Bat */
	TWeakObjectPtr<AController> BatController;

public:

	AEchoGameMode();

	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	/** A player opened/closed the settings menu. The game is paused for everyone while anyone has it open */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Echo|Settings Menu")
	void SetPlayerInSettingsMenu(APlayerController* PlayerController, bool bOpen);

	virtual void Logout(AController* Exiting) override;

protected:

	/** Players with the settings menu open (server only) */
	TArray<TWeakObjectPtr<APlayerController>> PlayersInSettingsMenu;

	/** Pauses or unpauses to match PlayersInSettingsMenu, and tells the game state who's in the menu */
	void RefreshSettingsMenuPause();

	/** FCanUnpause for our pause: only once nobody has the menu open */
	bool CanUnpauseSettingsMenu() const;
};
