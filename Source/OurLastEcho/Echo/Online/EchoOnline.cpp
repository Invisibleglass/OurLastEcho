// Our Last Echo

#include "EchoOnline.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystemUtils.h"

const FName EchoOnline::EOSName(TEXT("EOS"));
const FName EchoOnline::NullName(TEXT("NULL"));

IOnlineSubsystem* EchoOnline::GetSubsystem(const UObject* WorldContext)
{
	const UWorld* World = WorldContext && GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	FName Service = NAME_None;
	if (World && World->WorldType == EWorldType::PIE)
	{
		const FString& PIEService = GetDefault<UEchoOnlineSettings>()->PIEOnlineService;
		if (!PIEService.IsEmpty())
		{
			Service = FName(*PIEService);
		}
	}
	// Per world, so each Play-In-Editor window gets its own instance
	return Online::GetSubsystem(World, Service);
}

FString EchoOnline::GetServiceName(const UObject* WorldContext)
{
	const IOnlineSubsystem* Subsystem = GetSubsystem(WorldContext);
	return Subsystem ? Subsystem->GetSubsystemName().ToString() : TEXT("none");
}

bool EchoOnline::IsEOS(const UObject* WorldContext)
{
	const IOnlineSubsystem* Subsystem = GetSubsystem(WorldContext);
	return Subsystem && Subsystem->GetSubsystemName() == EOSName;
}

FString EchoOnline::GetLocalPlayerName(const UObject* WorldContext)
{
	const IOnlineSubsystem* Subsystem = GetSubsystem(WorldContext);
	const IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	return Identity && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn ? Identity->GetPlayerNickname(0) : FString();
}
