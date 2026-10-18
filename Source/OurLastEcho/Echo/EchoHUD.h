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

	// ---- Combat (Milestone 8)

	/** Your health bottom-left (and your partner's, smaller), god mode, the downed countdown */
	void DrawPlayerHealth();

	/** A red flash at the screen edges when you're hurt */
	void DrawHurtFlash();

	/** Health bars over the enemies YOU can see (never ones outside your world), with the open-state timer */
	void DrawEnemyBars();

	/** The arena's wave line and hint at the top */
	void DrawArena();

	/** Charge meters for the bow's charged shot and the whip's charged lash */
	void DrawCharge();

	void DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& Fill, const FLinearColor& Back = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));

	/** Enemies with a bar drawn last frame (for tests) */
	TArray<TWeakObjectPtr<AActor>> LastBarEnemies;

	FText SubtitleText;
	float SubtitleUntil = 0.0f;

public:

	/** Shows a subtitle for Seconds (nothing shows if subtitles are off) */
	UFUNCTION(BlueprintCallable, Category="Echo|Subtitles")
	void ShowSubtitle(const FText& Text, float Seconds = 4.0f);

	/** The subtitle on screen right now, and its text scale (for tests); empty if none is showing */
	UFUNCTION(BlueprintPure, Category="Echo|Subtitles")
	FString GetVisibleSubtitle(float& OutScale) const;

	/** Did the last frame draw a health bar over this enemy (it's in this player's world)? For tests */
	UFUNCTION(BlueprintPure, Category="Echo|Combat")
	bool IsEnemyBarShown(AActor* Enemy) const;

	/** How many enemy health bars the last frame drew (for tests) */
	UFUNCTION(BlueprintPure, Category="Echo|Combat")
	int32 GetEnemyBarCount() const { return LastBarEnemies.Num(); }
};
