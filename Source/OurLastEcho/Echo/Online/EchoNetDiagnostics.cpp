// Our Last Echo

#include "EchoNetDiagnostics.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "EchoGameUserSettings.h"
#include "EchoOnline.h"
#include "EchoOnlineOverlay.h"
#include "OurLastEcho.h"

void UEchoNetDiagnostics::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// One file per run (and per Play-In-Editor window)
	FString Suffix;
	const FWorldContext* Context = GetGameInstance()->GetWorldContext();
	if (Context && Context->WorldType == EWorldType::PIE)
	{
		Suffix = FString::Printf(TEXT("-PIE%d"), Context->PIEInstance);
	}
	LogPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectLogDir() / TEXT("OnlineSessions") /
		FString::Printf(TEXT("Online-%s%s.log"), *FDateTime::Now().ToString(TEXT("%Y.%m.%d-%H.%M.%S")), *Suffix));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(LogPath), true);
	WriteLine(FString::Printf(TEXT("Our Last Echo online session log (%s, build %s)"), FPlatformProcess::ComputerName(), FApp::GetBuildVersion()));

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UEchoNetDiagnostics::Tick), 0.5f);
}

void UEchoNetDiagnostics::Deinitialize()
{
	FTSTicker::RemoveTicker(TickHandle);
	WriteLine(TEXT("Game closed"));
	Super::Deinitialize();
}

void UEchoNetDiagnostics::Log(const UObject* Context, const FString& Line)
{
	const UWorld* World = Context && GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = Cast<UGameInstance>(Context);
	if (!GameInstance)
	{
		if (const UGameInstanceSubsystem* Subsystem = Cast<UGameInstanceSubsystem>(Context))
		{
			GameInstance = Subsystem->GetGameInstance();
		}
		else if (World)
		{
			GameInstance = World->GetGameInstance();
		}
	}
	if (UEchoNetDiagnostics* Diagnostics = GameInstance ? GameInstance->GetSubsystem<UEchoNetDiagnostics>() : nullptr)
	{
		Diagnostics->WriteLine(Line);
	}
	else
	{
		UE_LOG(LogOurLastEcho, Log, TEXT("Online: %s"), *Line);
	}
}

void UEchoNetDiagnostics::WriteLine(const FString& Line)
{
	UE_LOG(LogOurLastEcho, Log, TEXT("Online: %s"), *Line);
	if (!LogPath.IsEmpty())
	{
		const FString Stamped = FString::Printf(TEXT("[%s] %s\n"), *FDateTime::Now().ToString(TEXT("%H:%M:%S")), *Line);
		FFileHelper::SaveStringToFile(Stamped, *LogPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
}

bool UEchoNetDiagnostics::IsOverlayShown() const
{
	const UEchoGameUserSettings* Settings = UEchoGameUserSettings::GetEchoSettings();
	return Settings && Settings->bShowNetworkStats;
}

void UEchoNetDiagnostics::SetOverlayShown(bool bShown)
{
	if (UEchoGameUserSettings* Settings = UEchoGameUserSettings::GetEchoSettings())
	{
		Settings->bShowNetworkStats = bShown;
		Settings->SaveSettings();
	}
}

bool UEchoNetDiagnostics::Tick(float DeltaTime)
{
	UpdateStats();
	EnsureOverlay();

	const UWorld* World = GetGameInstance()->GetWorld();
	if (World)
	{
		const FString Map = World->GetMapName();
		const TCHAR* NetMode = World->GetNetMode() == NM_Client ? TEXT("guest") : World->GetNetMode() == NM_ListenServer ? TEXT("host") : TEXT("offline");
		if (Map != LastMap || NetMode != LastNetMode)
		{
			LastMap = Map;
			LastNetMode = NetMode;
			WriteLine(FString::Printf(TEXT("Map %s (%s, online service %s)"), *Map, NetMode, *EchoOnline::GetServiceName(World)));
		}
	}

	if (Stats.bConnected != bWasConnected)
	{
		bWasConnected = Stats.bConnected;
		WriteLine(Stats.bConnected ? FString::Printf(TEXT("Connected to the other player (%s)"), *Stats.Transport) : FString(TEXT("No longer connected to the other player")));
		LastSampleAt = FPlatformTime::Seconds();
	}
	if (Stats.bConnected && FPlatformTime::Seconds() - LastSampleAt >= GetDefault<UEchoOnlineSettings>()->SessionLogSampleSeconds)
	{
		LastSampleAt = FPlatformTime::Seconds();
		WriteLine(FString::Printf(TEXT("Net: ping %.0f ms, loss in %.1f%% out %.1f%%, in %.1f KB/s out %.1f KB/s, memory %.0f MB"),
			Stats.PingMs, Stats.InLossPercent, Stats.OutLossPercent, Stats.InKBps, Stats.OutKBps,
			FPlatformMemory::GetStats().UsedPhysical / (1024.0 * 1024.0)));
	}
	return true;
}

void UEchoNetDiagnostics::UpdateStats()
{
	Stats = FEchoNetStats();
	const UWorld* World = GetGameInstance()->GetWorld();
	const UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
	if (!Driver)
	{
		return;
	}
	const UNetConnection* Connection = Driver->ServerConnection ? Driver->ServerConnection.Get()
		: Driver->ClientConnections.Num() > 0 ? Driver->ClientConnections[0].Get() : nullptr;
	if (!Connection || Connection->GetConnectionState() != USOCK_Open)
	{
		return;
	}
	Stats.bConnected = true;
	Stats.PingMs = Connection->AvgLag * 1000.0f;
	Stats.InLossPercent = Connection->GetInLossPercentage().GetAvgLossPercentage() * 100.0f;
	Stats.OutLossPercent = Connection->GetOutLossPercentage().GetAvgLossPercentage() * 100.0f;
	Stats.InKBps = Connection->InBytesPerSecond / 1024.0f;
	Stats.OutKBps = Connection->OutBytesPerSecond / 1024.0f;
	Stats.Transport = Connection->GetClass()->GetName().Contains(TEXT("EOS")) ? TEXT("EOS P2P") : TEXT("IP");
}

void UEchoNetDiagnostics::EnsureOverlay()
{
	// One overlay per map: widgets on the viewport go away with the world
	APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (Overlay && Overlay->GetWorld() == PC->GetWorld() && Overlay->IsInViewport())
	{
		return;
	}
	Overlay = CreateWidget<UEchoOnlineOverlay>(PC, UEchoOnlineOverlay::StaticClass());
	if (Overlay)
	{
		// Above the menus (UIRoot is at 10)
		Overlay->AddToPlayerScreen(50);
	}
}
