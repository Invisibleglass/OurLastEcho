// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OnlineSubsystem.h"
#include "EchoOnline.generated.h"

/**
 *  Online settings that aren't the engine's ([/Script/OurLastEcho.EchoOnlineSettings] in DefaultGame.ini).
 *  The online service itself is the engine's [OnlineSubsystem] DefaultPlatformService (EOS or Null).
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Our Last Echo Online"))
class UEchoOnlineSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/**
	 *  The online service Play-In-Editor uses ("" = the game's DefaultPlatformService). Null by default: PIE
	 *  connects its windows by IP, which EOS's peer-to-peer sessions can't use, and needs no Epic login. Test EOS
	 *  with game processes (Scripts/run_eos_test.ps1) or a packaged build.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Online")
	FString PIEOnlineService = TEXT("Null");

	/** Where the EOS Developer Authentication Tool listens (two Epic accounts on one PC; see NOTES.md) */
	UPROPERTY(Config, EditAnywhere, Category="Online")
	FString DevAuthHost = TEXT("localhost:6300");

	/** Join codes: this many characters from CodeAlphabet */
	UPROPERTY(Config, EditAnywhere, Category="Online")
	int32 JoinCodeLength = 6;

	/** No 0/O, 1/I/L: easy to read out loud and type */
	UPROPERTY(Config, EditAnywhere, Category="Online")
	FString JoinCodeAlphabet = TEXT("ABCDEFGHJKMNPQRSTUVWXYZ23456789");

	/** Seconds between network samples written to the session log */
	UPROPERTY(Config, EditAnywhere, Category="Online")
	float SessionLogSampleSeconds = 10.0f;

	/** The screen holds black at most this long while an era level loads, then fades in anyway */
	UPROPERTY(Config, EditAnywhere, Category="Online")
	float MaxBlackScreenSeconds = 20.0f;
};

/** Which online service this machine (or Play-In-Editor window) uses, and small helpers around it */
namespace EchoOnline
{
	/** EOS or NULL, as the engine names them */
	extern const FName EOSName;
	extern const FName NullName;

	/** The online subsystem for this world: per Play-In-Editor window, the PIE override, else the default */
	IOnlineSubsystem* GetSubsystem(const UObject* WorldContext);

	/** "EOS", "NULL" or "none" */
	FString GetServiceName(const UObject* WorldContext);

	/** True when the world uses Epic Online Services (login, internet sessions, voice) */
	bool IsEOS(const UObject* WorldContext);

	/** Logged-in local player's display name ("" if none) */
	FString GetLocalPlayerName(const UObject* WorldContext);
}
