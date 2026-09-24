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
#include "EchoAudioSettings.h"
#include "EchoGameMode.h"
#include "EchoSettingsMenu.h"
#include "EchoSpiritBowComponent.h"
#include "OurLastEchoCharacter.h"

AOurLastEchoPlayerController::AOurLastEchoPlayerController()
{
	SettingsMenuClass = TSoftClassPtr<UEchoSettingsMenu>(FSoftObjectPath(TEXT("/Game/Echo/UI/WBP_SettingsMenu.WBP_SettingsMenu_C")));
	MenuAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_Menu.IA_Menu")));
	MenuMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Input/IMC_Menu.IMC_Menu")));
}

void AOurLastEchoPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// This machine's saved sound volume
	if (IsLocalPlayerController())
	{
		UEchoAudioSettings::ApplySavedVolume(this);
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
			EnhancedInput->BindAction(Menu, ETriggerEvent::Started, this, &AOurLastEchoPlayerController::ToggleSettingsMenu);
		}
	}
}

bool AOurLastEchoPlayerController::IsSettingsMenuOpen() const
{
	return SettingsMenu && SettingsMenu->IsInViewport();
}

void AOurLastEchoPlayerController::ToggleSettingsMenu()
{
	if (IsSettingsMenuOpen())
	{
		CloseSettingsMenu();
	}
	else
	{
		OpenSettingsMenu();
	}
}

void AOurLastEchoPlayerController::EchoMenu()
{
	ToggleSettingsMenu();
}

void AOurLastEchoPlayerController::OpenSettingsMenu()
{
	if (!IsLocalPlayerController() || IsSettingsMenuOpen())
	{
		return;
	}

	if (!SettingsMenu)
	{
		UClass* MenuClass = SettingsMenuClass.LoadSynchronous();
		if (!MenuClass)
		{
			UE_LOG(LogOurLastEcho, Error, TEXT("Settings menu widget %s not found"), *SettingsMenuClass.ToString());
			return;
		}
		SettingsMenu = CreateWidget<UEchoSettingsMenu>(this, MenuClass);
	}

	// Don't leave Bat stuck aiming if the aim button is let go while the menu is up
	if (const AOurLastEchoCharacter* EchoCharacter = Cast<AOurLastEchoCharacter>(GetPawn()))
	{
		EchoCharacter->GetSpiritBow()->SetAiming(false);
	}

	SettingsMenu->AddToViewport(100);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(SettingsMenu->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
	SettingsMenu->FocusFirstControl();

	ReportMenuState(true);
}

void AOurLastEchoPlayerController::CloseSettingsMenu()
{
	if (!IsSettingsMenuOpen())
	{
		return;
	}

	SettingsMenu->RemoveFromParent();
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);

	ReportMenuState(false);
}

void AOurLastEchoPlayerController::ReportMenuState(bool bOpen)
{
	if (HasAuthority())
	{
		ServerSetInSettingsMenu_Implementation(bOpen);
	}
	else
	{
		ServerSetInSettingsMenu(bOpen);
	}
}

void AOurLastEchoPlayerController::ServerSetInSettingsMenu_Implementation(bool bOpen)
{
	if (AEchoGameMode* GameMode = GetWorld()->GetAuthGameMode<AEchoGameMode>())
	{
		GameMode->SetPlayerInSettingsMenu(this, bOpen);
	}
}

bool AOurLastEchoPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
