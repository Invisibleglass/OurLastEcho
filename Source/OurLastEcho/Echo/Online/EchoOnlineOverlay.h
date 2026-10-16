// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EchoOnlineOverlay.generated.h"

class UBorder;
class UTextBlock;

/**
 *  Small always-on-top layer (title, lobby and game alike), kept on every map by UEchoNetDiagnostics:
 *  - top right, when switched on: ping, packet loss and bandwidth to the other player
 *  - bottom left: "<name> is speaking" while the other player talks, and "Mic on" while you transmit with
 *    push-to-talk. Never takes input.
 */
UCLASS()
class UEchoOnlineOverlay : public UUserWidget
{
	GENERATED_BODY()

public:

	/** What the speaking indicator shows right now ("" when hidden; for tests) */
	UFUNCTION(BlueprintPure, Category="Echo|Online")
	FString GetSpeakingText() const;

	/** What the network panel shows right now ("" when hidden; for tests) */
	UFUNCTION(BlueprintPure, Category="Echo|Online")
	FString GetStatsText() const;

protected:

	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	FString GetOtherPlayerName() const;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> StatsBack;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatsText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> SpeakingBack;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpeakingText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> MicBack;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MicText;
};
