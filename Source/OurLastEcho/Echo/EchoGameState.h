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

	/** Debug: every spirit and echo platform is shown to both players (EchoShowAllPlatforms console command) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Echo|Debug")
	bool bDebugShowAllPlatforms = false;

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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
