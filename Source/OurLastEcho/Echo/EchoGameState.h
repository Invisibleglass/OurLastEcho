// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "EchoGameState.generated.h"

/**
 *  Replicated match state shared by both players
 */
UCLASS()
class AEchoGameState : public AGameStateBase
{
	GENERATED_BODY()

protected:

	/** True once both players have reached the end zone together */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Echo")
	bool bMilestoneComplete = false;

	/** Players with the in-game menu open; the game is paused while this isn't empty */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Echo")
	TArray<TObjectPtr<APlayerState>> PlayersInPauseMenu;

	/** Debug: every spirit and echo platform is shown to both players (EchoShowAllPlatforms console command) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Echo|Debug")
	bool bDebugShowAllPlatforms = false;

	/** Debug: every enemy is shown to both players, whoever's world it's in (EchoShowAllEnemies) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Echo|Debug")
	bool bDebugShowAllEnemies = false;

public:

	/** Marks the milestone complete for everyone. Server only */
	void SetMilestoneComplete();

	UFUNCTION(BlueprintPure, Category="Echo")
	bool IsMilestoneComplete() const { return bMilestoneComplete; }

	/** Server only. Use AOurLastEchoCharacter::RequestShowAllPlatforms from a client */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Echo|Debug")
	void SetDebugShowAllPlatforms(bool bShow);

	UFUNCTION(BlueprintPure, Category="Echo|Debug")
	bool IsDebugShowAllPlatforms() const { return bDebugShowAllPlatforms; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Echo|Debug")
	void SetDebugShowAllEnemies(bool bShow) { bDebugShowAllEnemies = bShow; ForceNetUpdate(); }

	UFUNCTION(BlueprintPure, Category="Echo|Debug")
	bool IsDebugShowAllEnemies() const { return bDebugShowAllEnemies; }

	/** Server only (AEchoGameMode keeps this up to date) */
	void SetPlayersInPauseMenu(const TArray<APlayerState*>& Players);

	UFUNCTION(BlueprintPure, Category="Echo")
	TArray<APlayerState*> GetPlayersInPauseMenu() const { return TArray<APlayerState*>(PlayersInPauseMenu); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
