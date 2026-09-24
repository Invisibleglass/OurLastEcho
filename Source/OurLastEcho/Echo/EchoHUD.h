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

	/** Spirit bow reticle, drawn while the local character is aiming */
	UPROPERTY(EditDefaultsOnly, Category="Echo|Reticle")
	FLinearColor ReticleColor = FLinearColor(1.0f, 0.82f, 0.4f, 0.95f);

	/** Length of each reticle arm, in pixels */
	UPROPERTY(EditDefaultsOnly, Category="Echo|Reticle")
	float ReticleSize = 10.0f;

	/** Gap between the centre and each arm, in pixels */
	UPROPERTY(EditDefaultsOnly, Category="Echo|Reticle")
	float ReticleGap = 5.0f;

public:

	virtual void DrawHUD() override;

protected:

	void DrawReticle();

	/** While paused, tells the player who DIDN'T open the menu who did */
	void DrawPausedBanner();
};
