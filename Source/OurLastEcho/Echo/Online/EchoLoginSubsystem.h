// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EchoLoginSubsystem.generated.h"

class FUniqueNetId;

UENUM(BlueprintType)
enum class EEchoLoginState : uint8
{
	NotStarted,
	LoggingIn,
	LoggedIn,
	Failed
};

/**
 *  Signs the player in before the main menu (Milestone 7). Once per run: returning to the title keeps the login.
 *
 *  EOS: the Epic account. The first time, the Epic account portal opens (overlay or browser); after that the login
 *  is remembered (bPreferPersistentAuth). For two accounts on one PC, start the game with -EchoDevAuth=<credential
 *  name> and the EOS Developer Authentication Tool running (NOTES.md). Null (LAN): nothing to sign in to.
 */
UCLASS()
class UEchoLoginSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Deinitialize() override;

	/** Starts signing in (or tries again after a failure). Does nothing while signing in or signed in */
	UFUNCTION(BlueprintCallable, Category="Echo|Online")
	void Login();

	UFUNCTION(BlueprintPure, Category="Echo|Online")
	EEchoLoginState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category="Echo|Online")
	bool IsLoggedIn() const { return State == EEchoLoginState::LoggedIn; }

	/** Why the last attempt failed, for the player */
	UFUNCTION(BlueprintPure, Category="Echo|Online")
	FText GetErrorText() const { return ErrorText; }

	/** The signed-in player's name (Epic display name, or this computer's name on LAN) */
	UFUNCTION(BlueprintPure, Category="Echo|Online")
	FString GetDisplayName() const { return DisplayName; }

	/** "Epic account" or "LAN": what the player signed in with */
	UFUNCTION(BlueprintPure, Category="Echo|Online")
	bool IsUsingEpicAccount() const { return bEpic; }

	/** Login state changed */
	FSimpleMulticastDelegate OnLoginChanged;

	/** Seconds to wait for the Epic account portal before giving up (the player may be typing a password) */
	static constexpr double LoginTimeout = 300.0;

private:

	void HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
	void Finish(bool bSuccess, const FText& Error);
	bool CheckTimeout(float DeltaTime);
	static FText DescribeError(const FString& Error);

	EEchoLoginState State = EEchoLoginState::NotStarted;
	FText ErrorText;
	FString DisplayName;
	bool bEpic = false;
	double StartedAt = 0.0;

	FDelegateHandle LoginHandle;
	FTSTicker::FDelegateHandle TimeoutHandle;
};
