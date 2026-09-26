// Our Last Echo

#include "EchoFrontEndPlayerController.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "EchoFrontEndGameMode.h"
#include "EchoGameInstance.h"
#include "EchoGameUserSettings.h"
#include "EchoMenuScreens.h"
#include "EchoTitleScene.h"
#include "EchoUIWidgets.h"

#define LOCTEXT_NAMESPACE "EchoFrontEnd"

AEchoFrontEndPlayerController::AEchoFrontEndPlayerController()
{
	// The title camera is the view, not a pawn
	bAutoManageActiveCameraTarget = false;
	bShowMouseCursor = true;
}

void AEchoFrontEndPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	ViewTitleCamera();
	if (UEchoGameUserSettings* Settings = UEchoGameUserSettings::GetEchoSettings())
	{
		Settings->ApplyAudio();
	}

	UIRoot = CreateWidget<UEchoUIRoot>(this, UEchoUIRoot::StaticClass());
	UIRoot->AddToPlayerScreen(10);

	// A plain title screen shows the main menu; a hosted or joined game (listen server or client) is the lobby
	if (GetNetMode() == NM_Standalone)
	{
		UIRoot->Push<UEchoMainMenuScreen>();
	}
	else
	{
		UIRoot->Push<UEchoLobbyScreen>();
	}

	// Whatever sent us back here ("The host left the game.")
	if (UEchoGameInstance* GameInstance = GetGameInstance<UEchoGameInstance>())
	{
		const FText Message = GameInstance->ConsumePendingMessage();
		if (!Message.IsEmpty())
		{
			UIRoot->ShowDialog(LOCTEXT("MessageTitle", "Our Last Echo"), Message, { { LOCTEXT("OK", "OK"), nullptr } });
		}
	}
}

void AEchoFrontEndPlayerController::ViewTitleCamera()
{
	for (TActorIterator<AEchoTitleCamera> It(GetWorld()); It; ++It)
	{
		SetViewTarget(*It);
		return;
	}
}

void AEchoFrontEndPlayerController::SetReady(bool bReady)
{
	ServerSetReady(bReady);
}

void AEchoFrontEndPlayerController::ServerSetReady_Implementation(bool bReady)
{
	if (AEchoLobbyPlayerState* State = GetPlayerState<AEchoLobbyPlayerState>())
	{
		State->SetReady(bReady);
	}
}

void AEchoFrontEndPlayerController::RequestStartGame()
{
	ServerStartGame();
}

void AEchoFrontEndPlayerController::ServerStartGame_Implementation()
{
	if (AEchoFrontEndGameMode* GameMode = GetWorld()->GetAuthGameMode<AEchoFrontEndGameMode>())
	{
		GameMode->StartGame(this);
	}
}

void AEchoFrontEndPlayerController::ClientShowToast_Implementation(const FText& Message)
{
	if (UIRoot)
	{
		UIRoot->ShowToast(Message);
	}
}

#undef LOCTEXT_NAMESPACE
