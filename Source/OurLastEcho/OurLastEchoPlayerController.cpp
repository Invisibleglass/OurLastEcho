// Copyright Epic Games, Inc. All Rights Reserved.


#include "OurLastEchoPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "OurLastEcho.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "EchoArenaManager.h"
#include "EchoEraDirector.h"
#include "EchoGameState.h"
#include "EchoHealthComponent.h"
#include "EchoPlayerCombatComponent.h"
#include "EchoSwordWhipComponent.h"
#include "TimerManager.h"
#include "EchoGameMode.h"
#include "EchoGameUserSettings.h"
#include "EchoHUD.h"
#include "EchoMenuScreens.h"
#include "EchoSettingsScreen.h"
#include "EchoSpiritBowComponent.h"
#include "EchoUIWidgets.h"
#include "OurLastEchoCharacter.h"

AOurLastEchoPlayerController::AOurLastEchoPlayerController()
{
	MenuAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_Menu.IA_Menu")));
	MenuMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Input/IMC_Menu.IMC_Menu")));
}

void AOurLastEchoPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		// This machine's saved settings (volumes into this world's sound mix)
		if (UEchoGameUserSettings* Settings = UEchoGameUserSettings::GetEchoSettings())
		{
			Settings->ApplyAudio();
		}

		// Coming from the title screen's menus, the cursor and UI input mode may still be on
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogOurLastEcho, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AOurLastEchoPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			// Key bindings the player changed in Settings apply to these (Enhanced Input user settings)
			if (UEnhancedInputUserSettings* UserSettings = Subsystem->GetUserSettings())
			{
				for (UInputMappingContext* Context : UEchoSettingsScreen::LoadMappableContexts())
				{
					UserSettings->RegisterInputMappingContext(Context);
				}
			}

			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}

			if (UInputMappingContext* MenuContext = MenuMappingContext.LoadSynchronous())
			{
				Subsystem->AddMappingContext(MenuContext, 10);
			}
		}

		UInputAction* Menu = MenuAction.LoadSynchronous();
		if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent); EnhancedInput && Menu)
		{
			EnhancedInput->BindAction(Menu, ETriggerEvent::Started, this, &AOurLastEchoPlayerController::TogglePauseMenu);
		}
	}
}

bool AOurLastEchoPlayerController::IsPauseMenuOpen() const
{
	return UIRoot && UIRoot->HasScreens();
}

void AOurLastEchoPlayerController::TogglePauseMenu()
{
	if (IsPauseMenuOpen())
	{
		ClosePauseMenu();
	}
	else
	{
		OpenPauseMenu();
	}
}

void AOurLastEchoPlayerController::EchoMenu()
{
	TogglePauseMenu();
}

void AOurLastEchoPlayerController::OpenPauseMenu()
{
	if (!IsLocalPlayerController() || IsPauseMenuOpen())
	{
		return;
	}

	if (!UIRoot)
	{
		UIRoot = CreateWidget<UEchoUIRoot>(this, UEchoUIRoot::StaticClass());
		UIRoot->AddToPlayerScreen(50);
		UIRoot->OnEmptied.AddUObject(this, &AOurLastEchoPlayerController::HandleMenusClosed);
	}

	// Don't leave Bat stuck aiming if the aim button is let go while the menu is up
	if (const AOurLastEchoCharacter* EchoCharacter = Cast<AOurLastEchoCharacter>(GetPawn()))
	{
		EchoCharacter->GetSpiritBow()->SetAiming(false);
	}

	// The menu takes input (CommonUI switches to menu input and shows the cursor), and the game pauses for both
	UIRoot->Push<UEchoPauseMenuScreen>();
	SetInPauseMenu(true);
}

void AOurLastEchoPlayerController::ClosePauseMenu()
{
	if (UIRoot)
	{
		UIRoot->CloseAll();
	}
	HandleMenusClosed();
}

void AOurLastEchoPlayerController::HandleMenusClosed()
{
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
	SetInPauseMenu(false);
}

void AOurLastEchoPlayerController::SetInPauseMenu(bool bOpen)
{
	if (bOpen == bToldServerMenuOpen)
	{
		return;
	}
	bToldServerMenuOpen = bOpen;
	if (HasAuthority())
	{
		ServerSetInPauseMenu_Implementation(bOpen);
	}
	else
	{
		ServerSetInPauseMenu(bOpen);
	}
}

void AOurLastEchoPlayerController::ServerSetInPauseMenu_Implementation(bool bOpen)
{
	if (AEchoGameMode* GameMode = GetWorld()->GetAuthGameMode<AEchoGameMode>())
	{
		GameMode->SetPlayerInPauseMenu(this, bOpen);
	}
}

void AOurLastEchoPlayerController::EchoSubtitle(const FString& Text)
{
	if (AEchoHUD* EchoHUD = GetHUD<AEchoHUD>())
	{
		EchoHUD->ShowSubtitle(FText::FromString(Text), 5.0f);
	}
}

void AOurLastEchoPlayerController::EchoSwapEra()
{
	if (AEchoEraDirector* Director = AEchoEraDirector::Get(this))
	{
		Director->SwapEra();
	}
}

void AOurLastEchoPlayerController::EchoEra(const FString& Era)
{
	AEchoEraDirector* Director = AEchoEraDirector::Get(this);
	if (!Director)
	{
		return;
	}
	if (Era.Equals(TEXT("present"), ESearchCase::IgnoreCase))
	{
		Director->SetDebugEra(true, EEchoEra::Present);
	}
	else if (Era.Equals(TEXT("past"), ESearchCase::IgnoreCase))
	{
		Director->SetDebugEra(true, EEchoEra::Past);
	}
	else
	{
		Director->SetDebugEra(false, EEchoEra::Present);
	}
}

void AOurLastEchoPlayerController::EchoShowBothEras()
{
	if (AEchoEraDirector* Director = AEchoEraDirector::Get(this))
	{
		Director->SetShowBothEras(!Director->IsShowingBothEras());
	}
}

void AOurLastEchoPlayerController::EchoGod()
{
	ServerCombatDebug(TEXT("God"), FString());
}

void AOurLastEchoPlayerController::EchoSpawnEnemy(const FString& Type)
{
	ServerCombatDebug(TEXT("Spawn"), Type);
}

void AOurLastEchoPlayerController::EchoShowAllEnemies()
{
	ServerCombatDebug(TEXT("ShowAll"), FString());
}

void AOurLastEchoPlayerController::EchoSkipToWave(int32 Wave)
{
	ServerCombatDebug(TEXT("Wave"), FString::FromInt(Wave));
}

void AOurLastEchoPlayerController::EchoRestartArena()
{
	ServerCombatDebug(TEXT("Restart"), FString());
}

void AOurLastEchoPlayerController::EchoHurt(float Amount)
{
	ServerCombatDebug(TEXT("Hurt"), FString::SanitizeFloat(Amount));
}

void AOurLastEchoPlayerController::EchoGoTo(const FString& Where)
{
	ServerCombatDebug(TEXT("GoTo"), Where);
}

void AOurLastEchoPlayerController::EchoShoot(const FString& Kind)
{
	AOurLastEchoCharacter* PlayerCharacter = Cast<AOurLastEchoCharacter>(GetPawn());
	UEchoSpiritBowComponent* Bow = PlayerCharacter ? PlayerCharacter->GetSpiritBow() : nullptr;
	if (!Bow)
	{
		return;
	}
	if (Kind.Equals(TEXT("quick"), ESearchCase::IgnoreCase))
	{
		Bow->SetAiming(false);
		Bow->QuickShot();
		return;
	}
	Bow->SetAiming(true);
	Bow->BeginCharge();
	if (Kind.Equals(TEXT("charged"), ESearchCase::IgnoreCase))
	{
		// Let go once fully drawn
		TWeakObjectPtr<UEchoSpiritBowComponent> WeakBow(Bow);
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakBow]()
		{
			if (WeakBow.IsValid())
			{
				WeakBow->ReleaseCharge();
			}
		}), Bow->ChargeTime + 0.1f, false);
	}
	else
	{
		Bow->ReleaseCharge();
	}
}

void AOurLastEchoPlayerController::EchoLash(const FString& Kind)
{
	AOurLastEchoCharacter* PlayerCharacter = Cast<AOurLastEchoCharacter>(GetPawn());
	UEchoSwordWhipComponent* Whip = PlayerCharacter ? PlayerCharacter->GetSwordWhip() : nullptr;
	if (!Whip)
	{
		return;
	}
	if (Kind.Equals(TEXT("charged"), ESearchCase::IgnoreCase))
	{
		FVector Location;
		FRotator Rotation;
		GetPlayerViewPoint(Location, Rotation);
		Whip->LashOfKind(Rotation.Vector(), EEchoAttackKind::WhipCharged, 1.0f);
	}
	else
	{
		Whip->ComboLash();
	}
}

void AOurLastEchoPlayerController::EchoDodge()
{
	if (AOurLastEchoCharacter* PlayerCharacter = Cast<AOurLastEchoCharacter>(GetPawn()))
	{
		PlayerCharacter->GetPlayerCombat()->RequestDodge();
	}
}

void AOurLastEchoPlayerController::ServerCombatDebug_Implementation(const FString& Command, const FString& Argument)
{
	AOurLastEchoCharacter* PlayerCharacter = Cast<AOurLastEchoCharacter>(GetPawn());
	AEchoGameState* GameState = GetWorld()->GetGameState<AEchoGameState>();
	AEchoArenaManager* Arena = AEchoArenaManager::Get(GetWorld());
	UE_LOG(LogOurLastEcho, Log, TEXT("Combat debug from %s: %s %s"), *GetName(), *Command, *Argument);

	if (Command == TEXT("God") && PlayerCharacter)
	{
		UEchoHealthComponent* Health = PlayerCharacter->GetHealth();
		Health->SetGodMode(!Health->IsGodMode());
		ClientMessage(Health->IsGodMode() ? TEXT("God mode on") : TEXT("God mode off"));
	}
	else if (Command == TEXT("Spawn") && PlayerCharacter)
	{
		const FString Name = Argument.ToLower();
		const EEchoEnemyType Type = Name.StartsWith(TEXT("sh")) ? EEchoEnemyType::Shade : Name.StartsWith(TEXT("g")) || Name.StartsWith(TEXT("st")) ? EEchoEnemyType::StoneGuardian
			: Name.StartsWith(TEXT("t")) || Name.StartsWith(TEXT("sk")) ? EEchoEnemyType::SkyTalon : EEchoEnemyType::DustWraith;
		const FVector Ahead = PlayerCharacter->GetActorLocation() + PlayerCharacter->GetActorForwardVector() * 500.0f
			+ FVector(0.0f, 0.0f, Type == EEchoEnemyType::SkyTalon ? 500.0f : 50.0f);
		AEchoArenaManager::SpawnEnemyAnywhere(GetWorld(), Type, Ahead);
	}
	else if (Command == TEXT("ShowAll") && GameState)
	{
		GameState->SetDebugShowAllEnemies(!GameState->IsDebugShowAllEnemies());
	}
	else if (Command == TEXT("Wave") && Arena)
	{
		Arena->SkipToWave(FCString::Atoi(*Argument));
	}
	else if (Command == TEXT("Restart") && Arena)
	{
		Arena->RestartArena();
	}
	else if (Command == TEXT("Hurt") && PlayerCharacter)
	{
		FEchoDamageInfo Info;
		Info.Amount = FCString::Atof(*Argument);
		Info.Kind = EEchoAttackKind::Debug;
		Info.HitLocation = PlayerCharacter->GetActorLocation();
		PlayerCharacter->GetHealth()->ApplyDamage(Info);
	}
	else if (Command == TEXT("GoTo"))
	{
		if (AEchoGameMode* GameMode = GetWorld()->GetAuthGameMode<AEchoGameMode>())
		{
			const bool bBack = Argument.Equals(TEXT("back"), ESearchCase::IgnoreCase);
			GameMode->TravelBothTo(bBack ? TEXT("/Game/Echo/Maps/Lvl_SpiritPath") : TEXT("/Game/Echo/Maps/CombatTest"), bBack ? FName(TEXT("CombatDoor")) : FName(TEXT("ArenaEntrance")));
		}
	}
}

bool AOurLastEchoPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
