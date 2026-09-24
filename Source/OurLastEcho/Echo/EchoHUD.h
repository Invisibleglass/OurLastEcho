// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "EchoHUD.generated.h"

/**
 *  Greybox HUD: draws the "Milestone complete" banner once the game state says so.
 *  Canvas-drawn so it needs no widget assets.
 */
UCLASS()
class AEchoHUD : public AHUD
{
	GENERATED_BODY()

protected:

	UPROPERTY(EditDefaultsOnly, Category="Echo")
	FText CompleteMessage = NSLOCTEXT("Echo", "MilestoneComplete", "Milestone complete");

	UPROPERTY(EditDefaultsOnly, Category="Echo")
	float TextScale = 3.0f;

public:

	virtual void DrawHUD() override;
};
