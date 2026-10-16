// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EchoNetDiagnostics.generated.h"

class UEchoOnlineOverlay;

/** The connection to the other player, as the network overlay shows it */
USTRUCT(BlueprintType)
struct FEchoNetStats
{
	GENERATED_BODY()

	/** Connected to the other player (host: they're connected to us; guest: we're connected to the host) */
	UPROPERTY(BlueprintReadOnly, Category="Echo|Online")
	bool bConnected = false;

	/** Round trip in milliseconds */
	UPROPERTY(BlueprintReadOnly, Category="Echo|Online")
	float PingMs = 0.0f;

	/** Packets lost, percent (rolling average) */
	UPROPERTY(BlueprintReadOnly, Category="Echo|Online")
	float InLossPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Echo|Online")
	float OutLossPercent = 0.0f;

	/** Kilobytes per second */
	UPROPERTY(BlueprintReadOnly, Category="Echo|Online")
	float InKBps = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Echo|Online")
	float OutKBps = 0.0f;

	/** "EOS P2P" or "IP" */
	UPROPERTY(BlueprintReadOnly, Category="Echo|Online")
	FString Transport;
};

/**
 *  Network diagnostics (Milestone 7):
 *  - a session log per run, Saved/Logs/OnlineSessions/Online-<date-time>.log: login, hosting and joining, map
 *    changes, connections and disconnects, errors, and a network sample every SessionLogSampleSeconds
 *  - the on-screen overlay (ping, packet loss, bandwidth; Settings > Graphics or the EchoNetStats command) and the
 *    voice "speaking" indicator, both drawn by UEchoOnlineOverlay, which this keeps on every map
 */
UCLASS()
class UEchoNetDiagnostics : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Writes one line to the session log of Context's game instance (and the normal log) */
	static void Log(const UObject* Context, const FString& Line);

	void WriteLine(const FString& Line);

	UFUNCTION(BlueprintPure, Category="Echo|Online")
	FEchoNetStats GetStats() const { return Stats; }

	UFUNCTION(BlueprintPure, Category="Echo|Online")
	FString GetLogFilePath() const { return LogPath; }

	/** The network overlay (saved in the settings) */
	UFUNCTION(BlueprintPure, Category="Echo|Online")
	bool IsOverlayShown() const;

	UFUNCTION(BlueprintCallable, Category="Echo|Online")
	void SetOverlayShown(bool bShown);

private:

	bool Tick(float DeltaTime);
	void UpdateStats();
	void EnsureOverlay();

	FTSTicker::FDelegateHandle TickHandle;
	FEchoNetStats Stats;
	FString LogPath;
	FString LastMap;
	FString LastNetMode;
	double LastSampleAt = 0.0;
	bool bWasConnected = false;

	UPROPERTY(Transient)
	TObjectPtr<UEchoOnlineOverlay> Overlay;
};
