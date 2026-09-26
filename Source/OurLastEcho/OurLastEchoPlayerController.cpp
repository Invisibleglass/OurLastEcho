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

	// The menu takes input (CommonUI switches to menu input and shows the cursor); the world keeps running
	UIRoot->Push<UEchoPauseMenuScreen>();
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
}

void AOurLastEchoPlayerController::EchoSubtitle(const FString& Text)
{
	if (AEchoHUD* EchoHUD = GetHUD<AEchoHUD>())
	{
		EchoHUD->ShowSubtitle(FText::FromString(Text), 5.0f);
	}
}

bool AOurLastEchoPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
