// Our Last Echo

#include "EchoLoginSubsystem.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "EchoNetDiagnostics.h"
#include "EchoOnline.h"
#include "OurLastEcho.h"

#define LOCTEXT_NAMESPACE "EchoLogin"

void UEchoLoginSubsystem::Deinitialize()
{
	if (IOnlineSubsystem* Subsystem = EchoOnline::GetSubsystem(GetGameInstance()))
	{
		if (IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface())
		{
			Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);
		}
	}
	FTSTicker::RemoveTicker(TimeoutHandle);
	Super::Deinitialize();
}

void UEchoLoginSubsystem::Login()
{
	if (State == EEchoLoginState::LoggingIn || State == EEchoLoginState::LoggedIn)
	{
		return;
	}

	IOnlineSubsystem* Subsystem = EchoOnline::GetSubsystem(GetGameInstance());
	const IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	if (!Identity)
	{
		Finish(false, LOCTEXT("NoService", "Online services aren't available. Check the game's online setup (NOTES.md)."));
		return;
	}

	State = EEchoLoginState::LoggingIn;
	ErrorText = FText::GetEmpty();
	bEpic = Subsystem->GetSubsystemName() == EchoOnline::EOSName;
	OnLoginChanged.Broadcast();

	if (!bEpic)
	{
		// Null (LAN): no account; the computer's name is the player's name
		UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Online service %s: no login needed"), *Subsystem->GetSubsystemName().ToString()));
		DisplayName = FPlatformProcess::ComputerName();
		Finish(true, FText::GetEmpty());
		return;
	}

	if (Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
	{
		DisplayName = Identity->GetPlayerNickname(0);
		Finish(true, FText::GetEmpty());
		return;
	}

	Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);
	LoginHandle = Identity->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &UEchoLoginSubsystem::HandleLoginComplete));
	StartedAt = FPlatformTime::Seconds();
	FTSTicker::RemoveTicker(TimeoutHandle);
	TimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UEchoLoginSubsystem::CheckTimeout), 1.0f);

	// Two accounts on one PC: -EchoDevAuth=<credential name in the Developer Authentication Tool>
	FString DevAuthCredential;
	bDevAuth = FParse::Value(FCommandLine::Get(), TEXT("EchoDevAuth="), DevAuthCredential) && !DevAuthCredential.IsEmpty();
	if (bDevAuth)
	{
		UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Login: Epic account through the Developer Authentication Tool (credential %s)"), *DevAuthCredential));
		Identity->Login(0, FOnlineAccountCredentials(TEXT("developer"), GetDefault<UEchoOnlineSettings>()->DevAuthHost, DevAuthCredential));
		return;
	}

	// The remembered login, else the Epic account portal (OSS EOS falls back to it by itself)
	UEchoNetDiagnostics::Log(this, TEXT("Login: Epic account (remembered login, else the account portal)"));
	Identity->AutoLogin(0);
}

void UEchoLoginSubsystem::HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	if (State != EEchoLoginState::LoggingIn)
	{
		return;
	}
	if (IOnlineSubsystem* Subsystem = EchoOnline::GetSubsystem(GetGameInstance()))
	{
		if (IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface())
		{
			Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);
			DisplayName = bWasSuccessful ? Identity->GetPlayerNickname(0) : FString();
		}
	}
	if (!bWasSuccessful)
	{
		UE_LOG(LogOurLastEcho, Warning, TEXT("Login: failed (%s)"), *Error);
	}
	// Through the Developer Authentication Tool, "no connection" means the tool isn't running, not the internet
	const FText Reason = bDevAuth && Error.Contains(TEXT("NoConnection"))
		? LOCTEXT("DevAuthDown", "Couldn't reach the Developer Authentication Tool. Is it running, started on its port, and logged in?")
		: DescribeError(Error);
	Finish(bWasSuccessful, bWasSuccessful ? FText::GetEmpty() : Reason);
}

void UEchoLoginSubsystem::Finish(bool bSuccess, const FText& Error)
{
	FTSTicker::RemoveTicker(TimeoutHandle);
	TimeoutHandle.Reset();
	State = bSuccess ? EEchoLoginState::LoggedIn : EEchoLoginState::Failed;
	ErrorText = Error;
	UEchoNetDiagnostics::Log(this, bSuccess
		? FString::Printf(TEXT("Login: signed in as %s"), *DisplayName)
		: FString::Printf(TEXT("Login: failed: %s"), *Error.ToString()));
	OnLoginChanged.Broadcast();
}

bool UEchoLoginSubsystem::CheckTimeout(float DeltaTime)
{
	if (State != EEchoLoginState::LoggingIn)
	{
		TimeoutHandle.Reset();
		return false;
	}
	if (FPlatformTime::Seconds() - StartedAt < LoginTimeout)
	{
		return true;
	}
	if (IOnlineSubsystem* Subsystem = EchoOnline::GetSubsystem(GetGameInstance()))
	{
		if (IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface())
		{
			Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);
		}
	}
	TimeoutHandle.Reset();
	Finish(false, LOCTEXT("TimedOut", "Signing in took too long. Please try again."));
	return false;
}

FText UEchoLoginSubsystem::DescribeError(const FString& Error)
{
	if (Error.Contains(TEXT("Canceled")) || Error.Contains(TEXT("Cancelled")))
	{
		return LOCTEXT("Cancelled", "Sign-in was cancelled.");
	}
	if (Error.Contains(TEXT("NoConnection")) || Error.Contains(TEXT("TimedOut")) || Error.Contains(TEXT("ServiceFailure")))
	{
		return LOCTEXT("NoConnection", "Couldn't reach Epic's servers. Check your internet connection and try again.");
	}
	if (Error.Contains(TEXT("Developer")) || Error.Contains(TEXT("6300")) || Error.Contains(TEXT("InvalidUser")))
	{
		return LOCTEXT("DevAuth", "Couldn't sign in through the Developer Authentication Tool. Is it running and logged in?");
	}
	if (Error.Contains(TEXT("InvalidCredentials")) || Error.Contains(TEXT("ClientPolicy")) || Error.Contains(TEXT("InvalidParameters")) || Error.Contains(TEXT("Not configured")))
	{
		return LOCTEXT("Setup", "The game's Epic Online Services setup is incomplete (see NOTES.md).");
	}
	if (Error.Contains(TEXT("AccessDenied")) || Error.Contains(TEXT("InvalidAuth")))
	{
		return LOCTEXT("Denied", "Epic didn't accept this sign-in. If the game is still in testing, your Epic account may need to be added to its developer organization.");
	}
	return FText::Format(LOCTEXT("Generic", "Couldn't sign in to your Epic account ({0})."), FText::FromString(Error));
}

#undef LOCTEXT_NAMESPACE
