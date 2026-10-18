// Our Last Echo

#include "EchoGameMode.h"
#include "GameFramework/Controller.h"
#include "EchoGameState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "EchoGameInstance.h"
#include "OurLastEchoCharacter.h"
#include "EchoGameSession.h"
#include "EchoArenaManager.h"
#include "EchoCharacterMovementComponent.h"
#include "EchoNetDiagnostics.h"
#include "EchoOnline.h"
#include "EchoPlayerCombatComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "EchoHUD.h"
#include "OurLastEcho.h"

namespace
{
	/** Testing: the host plays Saraa (same as BP_EchoGameMode.bHostPlaysSaraa, but settable from the console before starting PIE) */
	TAutoConsoleVariable<bool> CVarEchoHostPlaysSaraa(TEXT("Echo.HostPlaysSaraa"), false, TEXT("If true, the first player (the listen-server host) plays Saraa and the joiner plays Bat."));
}

AEchoGameMode::AEchoGameMode()
{
	GameStateClass = AEchoGameState::StaticClass();
	HUDClass = AEchoHUD::StaticClass();
	GameSessionClass = AEchoGameSession::StaticClass();
}

bool AEchoGameMode::IsBatController(const AController* Controller) const
{
	// The host is the listen server's own (local) player. Arrival order isn't enough: after seamless travel
	// (online games, Milestone 7) players can arrive in either order. Without a listen server, the first player.
	const APlayerController* PC = Cast<APlayerController>(Controller);
	const bool bIsHost = GetNetMode() == NM_ListenServer ? (PC && PC->IsLocalController()) : FirstController.Get() == Controller;
	const bool bSwapRoles = bHostPlaysSaraa || CVarEchoHostPlaysSaraa.GetValueOnGameThread();
	return bIsHost != bSwapRoles;
}

void AEchoGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	const FString Arrive = UGameplayStatics::ParseOption(Options, TEXT("EchoArrive"));
	ArrivalTag = Arrive.IsEmpty() ? NAME_None : FName(*Arrive);
}

AActor* AEchoGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// Arriving through a door: the starts tagged for it, one per role
	if (!ArrivalTag.IsNone())
	{
		if (!FirstController.IsValid())
		{
			FirstController = Player;
		}
		const FName Wanted(*FString::Printf(TEXT("%s_%s"), *ArrivalTag.ToString(), IsBatController(Player) ? TEXT("Bat") : TEXT("Saraa")));
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			if (It->PlayerStartTag == Wanted)
			{
				return *It;
			}
		}
		UE_LOG(LogOurLastEcho, Warning, TEXT("EchoGameMode: no PlayerStart tagged %s, using the level's own"), *Wanted.ToString());
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

bool AEchoGameMode::TravelBothTo(const FString& MapPath, FName InArrivalTag)
{
	if (bTravelling || MapPath.IsEmpty())
	{
		return false;
	}
	bTravelling = true;

	// Online (EOS): seamless, so the peer-to-peer connection, session and voice stay up (a hard travel closes the
	// connection and EOS refuses the reconnect). LAN and Play-In-Editor: the normal hard travel
	const bool bEOS = EchoOnline::IsEOS(this);
	bUseSeamlessTravel = bEOS && !GetWorld()->IsPlayInEditor();
	FString Url = MapPath + TEXT("?listen");
	if (!bEOS)
	{
		Url += TEXT("?bUseIPSockets");
	}
	if (!InArrivalTag.IsNone())
	{
		Url += FString::Printf(TEXT("?EchoArrive=%s"), *InArrivalTag.ToString());
	}
	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Travel: both players to %s (%s)"), *Url, bUseSeamlessTravel ? TEXT("seamless") : TEXT("hard")));
	// Relative: a hard travel listens again on the same host port the guest reconnects to (absolute would reset it to
	// the default 7777, but Play-In-Editor listens on 17777); the old URL options are replaced by the new ones
	GetWorld()->ServerTravel(Url, false);
	return true;
}

void AEchoGameMode::HandlePlayerDowned(AOurLastEchoCharacter* Character)
{
	// Both down: start over after a moment (the arena restarts from its checkpoint)
	bool bAllDown = true;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AOurLastEchoCharacter* Player = It->Get() ? Cast<AOurLastEchoCharacter>(It->Get()->GetPawn()) : nullptr;
		if (Player && !Player->IsDowned())
		{
			bAllDown = false;
		}
	}
	if (bAllDown && !GetWorldTimerManager().IsTimerActive(AllDownTimer))
	{
		UE_LOG(LogOurLastEcho, Log, TEXT("EchoGameMode: both players are down, restarting in %.1f s"), AllDownRestartDelay);
		GetWorldTimerManager().SetTimer(AllDownTimer, this, &AEchoGameMode::RestartAfterAllDown, FMath::Max(0.1f, AllDownRestartDelay), false);
	}
}

void AEchoGameMode::RestartAfterAllDown()
{
	if (AEchoArenaManager* Arena = AEchoArenaManager::Get(GetWorld()))
	{
		Arena->RestartArena();
		return;
	}
	// No arena: everyone back to their own checkpoint
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AOurLastEchoCharacter* Player = It->Get() ? Cast<AOurLastEchoCharacter>(It->Get()->GetPawn()) : nullptr)
		{
			Player->RespawnAtStart();
			Player->GetPlayerCombat()->ServerRevive();
		}
	}
}

void AEchoGameMode::RespawnDownedPlayer(AOurLastEchoCharacter* Character)
{
	if (!Character || !Character->IsDowned())
	{
		return;
	}

	// Beside the other player, if they're still standing
	const AOurLastEchoCharacter* Partner = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AOurLastEchoCharacter* Player = It->Get() ? Cast<AOurLastEchoCharacter>(It->Get()->GetPawn()) : nullptr;
		if (Player && Player != Character)
		{
			Partner = Player;
		}
	}
	if (!Partner || Partner->IsDowned())
	{
		// Everyone's down: the all-down restart takes care of it
		HandlePlayerDowned(Character);
		return;
	}

	// Try a few spots around the partner; take the first with floor under it and room to stand
	const float Offset = Character->GetPlayerCombat()->PartnerRespawnOffset;
	const FVector Center = Partner->GetActorLocation();
	const FVector Right = Partner->GetActorRightVector();
	const FVector Forward = Partner->GetActorForwardVector();
	const FVector Candidates[] = { Right, -Right, -Forward, Forward, (Right - Forward).GetSafeNormal(), (-Right - Forward).GetSafeNormal() };
	FVector Spot = Center + FVector(0.0f, 0.0f, 20.0f);
	for (const FVector& Direction : Candidates)
	{
		const FVector Try = Center + Direction * Offset;
		FHitResult Floor;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(EchoRespawnSpot), false, Partner);
		if (GetWorld()->LineTraceSingleByChannel(Floor, Try + FVector(0.0f, 0.0f, 100.0f), Try - FVector(0.0f, 0.0f, 400.0f), ECC_Visibility, Params)
			&& !GetWorld()->OverlapAnyTestByChannel(Floor.ImpactPoint + FVector(0.0f, 0.0f, 100.0f), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(40.0f, 90.0f), Params))
		{
			Spot = Floor.ImpactPoint + FVector(0.0f, 0.0f, 100.0f);
			break;
		}
	}

	Character->GetEchoMovement()->CancelSwing();
	Character->GetCharacterMovement()->StopMovementImmediately();
	Character->TeleportTo(Spot, Partner->GetActorRotation(), false, true);
	Character->GetPlayerCombat()->ServerRevive();
	UE_LOG(LogOurLastEcho, Log, TEXT("EchoGameMode: %s revived beside %s"), *Character->GetName(), *Partner->GetName());
}

UClass* AEchoGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// Called more than once per player (start-spot selection and spawning), so this must be idempotent
	if (!FirstController.IsValid())
	{
		FirstController = InController;
	}

	const bool bIsBat = IsBatController(InController);
	const TSubclassOf<APawn> PawnClass = bIsBat ? BatPawnClass : SaraaPawnClass;

	if (!PawnClass)
	{
		UE_LOG(LogOurLastEcho, Warning, TEXT("EchoGameMode: %s pawn class not set, using default pawn"), bIsBat ? TEXT("Bat") : TEXT("Saraa"));
		return Super::GetDefaultPawnClassForController_Implementation(InController);
	}

	UE_LOG(LogOurLastEcho, Log, TEXT("EchoGameMode: %s plays %s (%s)"), *GetNameSafe(InController), bIsBat ? TEXT("Bat") : TEXT("Saraa"), *PawnClass->GetName());
	return PawnClass;
}


void AEchoGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	// A leaver's open menu no longer holds the pause (this also has to run before the timer below: a paused world's
	// timers don't tick)
	PlayersInPauseMenu.Remove(Cast<APlayerController>(Exiting));
	RefreshMenuPause();

	// The other player left an online game: the host goes back to their lobby (the session and its join code stay,
	// so the friend can come back or someone else can join), and says why.
	// (Not while the world is being torn down, e.g. the host leaving or Play-In-Editor stopping; and not during a
	// travel to another map: in a hard travel the guest disconnects on purpose, to reconnect to the new map.)
	UWorld* World = GetWorld();
	const APlayerController* Leaver = Cast<APlayerController>(Exiting);
	if (!World || World->bIsTearingDown || bTravelling || !World->NextURL.IsEmpty() || !HasActorBegunPlay() || GetNetMode() != NM_ListenServer
		|| !Leaver || Leaver->IsLocalController())
	{
		return;
	}

	// The leaver's pawn is already gone; they played whichever role the host doesn't
	const APlayerController* Host = World->GetFirstPlayerController();
	const AOurLastEchoCharacter* HostCharacter = Host ? Cast<AOurLastEchoCharacter>(Host->GetPawn()) : nullptr;
	const FText Message = HostCharacter && HostCharacter->GetRealm() == EEchoRealm::Spirit
		? NSLOCTEXT("EchoGame", "BatLeft", "Bat left the game.")
		: NSLOCTEXT("EchoGame", "SaraaLeft", "Saraa left the game.");
	UE_LOG(LogOurLastEcho, Log, TEXT("EchoGameMode: %s left, returning the host to the lobby"), *GetNameSafe(Exiting));

	// Next tick: loading a map from inside Logout isn't safe
	TWeakObjectPtr<UWorld> WeakWorld(World);
	World->GetTimerManager().SetTimerForNextTick([WeakWorld, Message]()
	{
		if (UEchoGameInstance* GameInstance = WeakWorld.IsValid() ? WeakWorld->GetGameInstance<UEchoGameInstance>() : nullptr)
		{
			GameInstance->ReturnToLobby(Message);
		}
	});
}

void AEchoGameMode::SetPlayerInPauseMenu(APlayerController* PlayerController, bool bOpen)
{
	if (!PlayerController)
	{
		return;
	}
	if (bOpen)
	{
		PlayersInPauseMenu.AddUnique(PlayerController);
	}
	else
	{
		PlayersInPauseMenu.Remove(PlayerController);
	}
	RefreshMenuPause();
}

bool AEchoGameMode::CanUnpauseMenu() const
{
	for (const TWeakObjectPtr<APlayerController>& Player : PlayersInPauseMenu)
	{
		if (Player.IsValid())
		{
			return false;
		}
	}
	return true;
}

void AEchoGameMode::RefreshMenuPause()
{
	PlayersInPauseMenu.RemoveAll([](const TWeakObjectPtr<APlayerController>& Player) { return !Player.IsValid(); });

	TArray<APlayerState*> InMenu;
	for (const TWeakObjectPtr<APlayerController>& Player : PlayersInPauseMenu)
	{
		InMenu.Add(Player->PlayerState);
	}
	if (AEchoGameState* EchoGameState = GetGameState<AEchoGameState>())
	{
		EchoGameState->SetPlayersInPauseMenu(InMenu);
	}

	// The pause itself is the engine's: the pauser replicates through AWorldSettings, so every machine freezes
	if (PlayersInPauseMenu.Num() > 0 && !IsPaused())
	{
		UE_LOG(LogOurLastEcho, Log, TEXT("Paused: %s opened the menu"), *GetNameSafe(PlayersInPauseMenu[0].Get()));
		SetPause(PlayersInPauseMenu[0].Get(), FCanUnpause::CreateUObject(this, &AEchoGameMode::CanUnpauseMenu));
	}
	else if (PlayersInPauseMenu.Num() == 0 && IsPaused())
	{
		UE_LOG(LogOurLastEcho, Log, TEXT("Unpaused: nobody has the menu open"));
		ClearPause();
	}

	// Paused, the server's clock stops, so actors whose next network update isn't due never replicate: push the
	// world settings (which carry the pause) out now so the other player's world pauses and resumes too
	if (AWorldSettings* WorldSettings = GetWorldSettings())
	{
		WorldSettings->ForceNetUpdate();
	}
}
