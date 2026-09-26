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

	/** Brackets around the anchor Saraa's whip is targeting */
	UPROPERTY(EditDefaultsOnly, Category="Echo|Whip")
	FLinearColor WhipMarkerColor = FLinearColor(0.35f, 0.65f, 1.0f, 0.95f);

	/** Half-size of the whip target brackets, in pixels */
	UPROPERTY(EditDefaultsOnly, Category="Echo|Whip")
	float WhipMarkerSize = 22.0f;

public:

	virtual void DrawHUD() override;

protected:

	void DrawReticle();

	void DrawWhipTarget();

	/** Subtitle line at the bottom of the screen (respects the Subtitles and Subtitle size settings) */
	void DrawSubtitle();

	/** While the other player's menu has the game paused: dims the screen and says who paused it */
	void DrawPausedBanner();

	FText SubtitleText;
	float SubtitleUntil = 0.0f;

public:

	/** Shows a subtitle for Seconds (nothing shows if subtitles are off) */
	UFUNCTION(BlueprintCallable, Category="Echo|Subtitles")
	void ShowSubtitle(const FText& Text, float Seconds = 4.0f);

	/** The subtitle on screen right now, and its text scale (for tests); empty if none is showing */
	UFUNCTION(BlueprintPure, Category="Echo|Subtitles")
	FString GetVisibleSubtitle(float& OutScale) const;
};
