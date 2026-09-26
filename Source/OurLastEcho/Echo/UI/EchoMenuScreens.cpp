// Our Last Echo

#include "EchoMenuScreens.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EchoFrontEndGameMode.h"
#include "EchoFrontEndPlayerController.h"
#include "EchoGameInstance.h"
#include "EchoSessionSubsystem.h"
#include "EchoSettingsScreen.h"

#define LOCTEXT_NAMESPACE "EchoMenus"

namespace
{
	UEchoSessionSubsystem* SessionsFor(const UUserWidget* Widget)
	{
		const UGameInstance* GameInstance = Widget ? Widget->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UEchoSessionSubsystem>() : nullptr;
	}

	UEchoGameInstance* EchoGameInstanceFor(const UUserWidget* Widget)
	{
		return Widget ? Cast<UEchoGameInstance>(Widget->GetGameInstance()) : nullptr;
	}
}

// ------------------------------------------------------------------ main menu

void UEchoMainMenuScreen::BuildContent(UVerticalBox* Content)
{
	AddTitle(Content, LOCTEXT("GameTitle", "Our Last Echo"), 56);
	AddText(Content, LOCTEXT("Tagline", "Two eras. One canyon."), 18, EchoUI::DimTextColor);
	AddSpacer(Content, 28.0f);

	AddButton(Content, LOCTEXT("Play", "Play"), [this]()
	{
		GetRoot()->Push<UEchoPlayMenuScreen>();
	});
	AddButton(Content, LOCTEXT("Settings", "Settings"), [this]()
	{
		GetRoot()->Push<UEchoSettingsScreen>();
	});
	AddButton(Content, LOCTEXT("Credits", "Credits"), [this]()
	{
		GetRoot()->Push<UEchoCreditsScreen>();
	});
	AddButton(Content, LOCTEXT("Quit", "Quit"), [this]()
	{
		AskToQuit();
	});
}

void UEchoMainMenuScreen::OnBack()
{
	AskToQuit();
}

void UEchoMainMenuScreen::AskToQuit()
{
	TWeakObjectPtr<UEchoMainMenuScreen> WeakThis(this);
	GetRoot()->ShowDialog(LOCTEXT("QuitTitle", "Quit"), LOCTEXT("QuitMessage", "Quit Our Last Echo?"),
	{
		{ LOCTEXT("QuitYes", "Quit"), [WeakThis]()
			{
				if (WeakThis.IsValid())
				{
					UKismetSystemLibrary::QuitGame(WeakThis.Get(), WeakThis->GetOwningPlayer(), EQuitPreference::Quit, false);
				}
			} },
		{ LOCTEXT("QuitNo", "Cancel"), nullptr },
	});
}

// ------------------------------------------------------------------ play

void UEchoPlayMenuScreen::BuildContent(UVerticalBox* Content)
{
	AddTitle(Content, LOCTEXT("PlayTitle", "Play"));
	AddText(Content, LOCTEXT("PlayHint", "Host a game as Bat, or join a friend's game as Saraa."), 18, EchoUI::DimTextColor);
	AddSpacer(Content, 20.0f);

	AddButton(Content, LOCTEXT("Host", "Host Game"), [this]() { Host(); });
	AddButton(Content, LOCTEXT("Join", "Join Game"), [this]()
	{
		GetRoot()->Push<UEchoJoinMenuScreen>();
	});
	AddSpacer(Content, 12.0f);
	AddButton(Content, LOCTEXT("Back", "Back"), [this]() { DeactivateWidget(); });
}

void UEchoPlayMenuScreen::Host()
{
	UEchoSessionSubsystem* Sessions = SessionsFor(this);
	if (!Sessions)
	{
		return;
	}

	UEchoUIRoot* UIRoot = GetRoot();
	TWeakObjectPtr<UEchoDialog> Busy = UIRoot->ShowDialog(LOCTEXT("HostingTitle", "Host Game"), LOCTEXT("Hosting", "Creating a game..."), {});
	Sessions->OnHostComplete.RemoveAll(this);
	Sessions->OnHostComplete.AddWeakLambda(this, [this, Busy](bool bSuccess, const FText& Error)
	{
		if (!bSuccess)
		{
			if (Busy.IsValid())
			{
				Busy->DeactivateWidget();
			}
			GetRoot()->ShowDialog(LOCTEXT("HostFailedTitle", "Couldn't host"), Error, { { LOCTEXT("OK", "OK"), nullptr } });
		}
		// On success the lobby map opens and replaces this menu
	});
	Sessions->HostSession();
}

void UEchoPlayMenuScreen::NativeDestruct()
{
	if (UEchoSessionSubsystem* Sessions = SessionsFor(this))
	{
		Sessions->OnHostComplete.RemoveAll(this);
	}
	Super::NativeDestruct();
}

// ------------------------------------------------------------------ join

void UEchoJoinMenuScreen::BuildContent(UVerticalBox* Content)
{
	AddTitle(Content, LOCTEXT("JoinTitle", "Join Game"));
	StatusText = AddText(Content, FText::GetEmpty(), 18, EchoUI::DimTextColor);
	AddSpacer(Content, 12.0f);

	GameList = WidgetTree->ConstructWidget<UVerticalBox>();
	Content->AddChildToVerticalBox(GameList);
	AddSpacer(Content, 12.0f);

	AddButton(Content, LOCTEXT("Refresh", "Refresh"), [this]() { Refresh(); });
	AddButton(Content, LOCTEXT("JoinBack", "Back"), [this]() { DeactivateWidget(); });
}

void UEchoJoinMenuScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	Refresh();
}

void UEchoJoinMenuScreen::Refresh()
{
	UEchoSessionSubsystem* Sessions = SessionsFor(this);
	if (!Sessions)
	{
		return;
	}

	for (UEchoButton* Button : GameButtons)
	{
		Buttons.Remove(Button);
	}
	GameButtons.Reset();
	GameIndices.Reset();
	GameList->ClearChildren();
	StatusText->SetText(LOCTEXT("Searching", "Looking for games on your network..."));

	Sessions->OnFindComplete.RemoveAll(this);
	Sessions->OnFindComplete.AddUObject(this, &UEchoJoinMenuScreen::HandleFindComplete);
	Sessions->FindSessions();
}

void UEchoJoinMenuScreen::HandleFindComplete(bool bSuccess, const FText& Error)
{
	UEchoSessionSubsystem* Sessions = SessionsFor(this);
	if (!Sessions)
	{
		return;
	}
	Sessions->OnFindComplete.RemoveAll(this);

	if (!bSuccess)
	{
		StatusText->SetText(Error);
		return;
	}

	const TArray<FEchoSessionInfo>& Results = Sessions->GetSearchResults();
	StatusText->SetText(Results.Num() == 0
		? LOCTEXT("NoGames", "No games found. Ask the other player to host, then Refresh.")
		: LOCTEXT("PickGame", "Choose a game to join as Saraa:"));

	for (const FEchoSessionInfo& Info : Results)
	{
		const int32 SessionIndex = Info.Index;
		UEchoButton* Button = MakeButton(FText::Format(LOCTEXT("GameEntry", "{0}'s game  ({1} ms)"), FText::FromString(Info.HostName), FText::AsNumber(Info.PingMs)),
			[this, SessionIndex]() { Join(SessionIndex); });
		GameList->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 4.0f));
		GameButtons.Add(Button);
		GameIndices.Add(SessionIndex);
	}

	// Put focus on the first game, so gamepad players can join straight away
	if (GameButtons.Num() > 0)
	{
		FirstFocus = GameButtons[0];
		GameButtons[0]->SetFocus();
	}
}

bool UEchoJoinMenuScreen::JoinFirstGame()
{
	if (GameIndices.Num() == 0)
	{
		return false;
	}
	GameButtons[0]->SimulateClick();
	return true;
}

void UEchoJoinMenuScreen::Join(int32 SessionIndex)
{
	UEchoSessionSubsystem* Sessions = SessionsFor(this);
	if (!Sessions)
	{
		return;
	}

	JoiningDialog = GetRoot()->ShowDialog(LOCTEXT("JoiningTitle", "Join Game"), LOCTEXT("Joining", "Joining..."), {});
	Sessions->OnJoinComplete.RemoveAll(this);
	Sessions->OnJoinComplete.AddUObject(this, &UEchoJoinMenuScreen::HandleJoinComplete);
	Sessions->JoinSession(SessionIndex);
}

void UEchoJoinMenuScreen::HandleJoinComplete(bool bSuccess, const FText& Error)
{
	if (UEchoSessionSubsystem* Sessions = SessionsFor(this))
	{
		Sessions->OnJoinComplete.RemoveAll(this);
	}
	if (bSuccess)
	{
		// Travelling to the host's lobby
		return;
	}

	if (JoiningDialog)
	{
		JoiningDialog->DeactivateWidget();
		JoiningDialog = nullptr;
	}
	GetRoot()->ShowDialog(LOCTEXT("JoinFailedTitle", "Couldn't join"), Error, { { LOCTEXT("JoinOK", "OK"), nullptr } });
	Refresh();
}

void UEchoJoinMenuScreen::NativeDestruct()
{
	if (UEchoSessionSubsystem* Sessions = SessionsFor(this))
	{
		Sessions->OnFindComplete.RemoveAll(this);
		Sessions->OnJoinComplete.RemoveAll(this);
	}
	Super::NativeDestruct();
}

// ------------------------------------------------------------------ lobby

void UEchoLobbyScreen::BuildContent(UVerticalBox* Content)
{
	AddTitle(Content, LOCTEXT("LobbyTitle", "Lobby"));
	BatLine = AddText(Content, FText::GetEmpty(), 22);
	SaraaLine = AddText(Content, FText::GetEmpty(), 22);
	AddSpacer(Content, 8.0f);
	HintText = AddText(Content, FText::GetEmpty(), 16, EchoUI::DimTextColor);
	AddSpacer(Content, 20.0f);

	ReadyButton = AddButton(Content, LOCTEXT("Ready", "Ready"), [this]()
	{
		AEchoFrontEndPlayerController* PC = GetOwningPlayer<AEchoFrontEndPlayerController>();
		const AEchoLobbyPlayerState* Me = PC ? PC->GetPlayerState<AEchoLobbyPlayerState>() : nullptr;
		if (PC && Me)
		{
			PC->SetReady(!Me->IsReady());
		}
	});
	StartButton = AddButton(Content, LOCTEXT("Start", "Start Game"), [this]()
	{
		if (AEchoFrontEndPlayerController* PC = GetOwningPlayer<AEchoFrontEndPlayerController>())
		{
			PC->RequestStartGame();
		}
	});
	AddButton(Content, LOCTEXT("Leave", "Leave Lobby"), [this]() { AskToLeave(); });
}

void UEchoLobbyScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const APlayerController* PC = GetOwningPlayer();
	const AGameStateBase* GameState = PC && PC->GetWorld() ? PC->GetWorld()->GetGameState() : nullptr;
	const AEchoLobbyPlayerState* Me = PC ? PC->GetPlayerState<AEchoLobbyPlayerState>() : nullptr;
	if (!GameState || !Me)
	{
		return;
	}

	const AEchoLobbyPlayerState* Host = nullptr;
	const AEchoLobbyPlayerState* Guest = nullptr;
	for (const APlayerState* Player : GameState->PlayerArray)
	{
		if (const AEchoLobbyPlayerState* LobbyPlayer = Cast<AEchoLobbyPlayerState>(Player))
		{
			(LobbyPlayer->IsHostPlayer() ? Host : Guest) = LobbyPlayer;
		}
	}

	auto Line = [](const FText& Role, const AEchoLobbyPlayerState* Player, const AEchoLobbyPlayerState* Me, const FText& Waiting)
	{
		if (!Player)
		{
			return FText::Format(LOCTEXT("SlotEmpty", "{0}:  {1}"), Role, Waiting);
		}
		return FText::Format(LOCTEXT("SlotLine", "{0}:  {1}{2}  -  {3}"), Role, FText::FromString(Player->GetPlayerName()),
			Player == Me ? LOCTEXT("You", " (you)") : FText::GetEmpty(),
			Player->IsReady() ? LOCTEXT("IsReady", "Ready") : LOCTEXT("NotReady", "Not ready"));
	};
	BatLine->SetText(Line(LOCTEXT("BatRole", "Bat (host)"), Host, Me, LOCTEXT("NoHost", "...")));
	SaraaLine->SetText(Line(LOCTEXT("SaraaRole", "Saraa"), Guest, Me, LOCTEXT("WaitingGuest", "waiting for the second player...")));

	const bool bBothReady = Host && Guest && Host->IsReady() && Guest->IsReady();
	ReadyButton->SetLabel(Me->IsReady() ? LOCTEXT("Unready", "Not Ready") : LOCTEXT("ReadyLabel", "Ready"));
	StartButton->SetVisibility(Me->IsHostPlayer() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	StartButton->SetIsEnabled(Me->IsHostPlayer() && bBothReady);

	HintText->SetText(!Guest
		? LOCTEXT("HintWait", "Waiting for Saraa: on another machine on this network, choose Play > Join Game.")
		: bBothReady
			? (Me->IsHostPlayer() ? LOCTEXT("HintStart", "Both ready. Start when you like.") : LOCTEXT("HintHostStarts", "Both ready. Waiting for Bat to start."))
			: LOCTEXT("HintReady", "Both players must be ready to start."));
}

FString UEchoLobbyScreen::GetPlayersText() const
{
	return BatLine->GetText().ToString() + TEXT(" | ") + SaraaLine->GetText().ToString();
}

void UEchoLobbyScreen::OnBack()
{
	AskToLeave();
}

void UEchoLobbyScreen::AskToLeave()
{
	TWeakObjectPtr<UEchoLobbyScreen> WeakThis(this);
	GetRoot()->ShowDialog(LOCTEXT("LeaveTitle", "Leave Lobby"), LOCTEXT("LeaveMessage", "Leave the lobby and go back to the title screen?"),
	{
		{ LOCTEXT("LeaveYes", "Leave"), [WeakThis]()
			{
				if (UEchoGameInstance* GameInstance = WeakThis.IsValid() ? EchoGameInstanceFor(WeakThis.Get()) : nullptr)
				{
					GameInstance->ReturnToTitle(FText::GetEmpty());
				}
			} },
		{ LOCTEXT("LeaveNo", "Stay"), nullptr },
	});
}

// ------------------------------------------------------------------ credits

void UEchoCreditsScreen::BuildContent(UVerticalBox* Content)
{
	AddTitle(Content, LOCTEXT("CreditsTitle", "Credits"));

	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
	Box->SetHeightOverride(420.0f);
	Box->SetWidthOverride(560.0f);
	Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	Box->SetContent(Scroll);
	Content->AddChildToVerticalBox(Box);

	const TCHAR* Lines[] =
	{
		TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT(""),
		TEXT("OUR LAST ECHO"), TEXT(""),
		TEXT("Game design"), TEXT("[Your name here]"), TEXT(""),
		TEXT("Programming"), TEXT("[Your name here]"), TEXT("with Claude"), TEXT(""),
		TEXT("Level design"), TEXT("[Your name here]"), TEXT(""),
		TEXT("Art"), TEXT("Greybox placeholders"), TEXT(""),
		TEXT("Music and sound"), TEXT("Coming soon"), TEXT(""),
		TEXT("Made with Unreal Engine"), TEXT(""),
		TEXT("Thank you for playing."),
		TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT(""),
	};
	for (const TCHAR* Line : Lines)
	{
		UTextBlock* Text = EchoUI::MakeText(WidgetTree, FText::FromString(Line), 20);
		Text->SetJustification(ETextJustify::Center);
		Scroll->AddChild(Text);
	}

	AddSpacer(Content, 16.0f);
	AddButton(Content, LOCTEXT("CreditsBack", "Back"), [this]() { DeactivateWidget(); });
}

void UEchoCreditsScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// A slow roll that starts again from the top
	Offset += InDeltaTime * 40.0f;
	const float End = Scroll->GetScrollOffsetOfEnd();
	if (End > 0.0f && Offset > End)
	{
		Offset = 0.0f;
	}
	Scroll->SetScrollOffset(Offset);
}

// ------------------------------------------------------------------ pause

void UEchoPauseMenuScreen::BuildContent(UVerticalBox* Content)
{
	AddTitle(Content, LOCTEXT("PauseTitle", "Menu"), 34);
	AddText(Content, LOCTEXT("PauseHint", "The game is paused for both of you."), 16, EchoUI::DimTextColor);
	AddSpacer(Content, 16.0f);

	AddButton(Content, LOCTEXT("Resume", "Resume"), [this]() { DeactivateWidget(); });
	AddButton(Content, LOCTEXT("PauseSettings", "Settings"), [this]()
	{
		GetRoot()->Push<UEchoSettingsScreen>();
	});
	AddButton(Content, LOCTEXT("LeaveGame", "Leave Game"), [this]() { AskToLeave(); });
}

void UEchoPauseMenuScreen::AskToLeave()
{
	TWeakObjectPtr<UEchoPauseMenuScreen> WeakThis(this);
	GetRoot()->ShowDialog(LOCTEXT("LeaveGameTitle", "Leave Game"), LOCTEXT("LeaveGameMessage", "Leave the game? Both players will go back to the title screen."),
	{
		{ LOCTEXT("LeaveGameYes", "Leave"), [WeakThis]()
			{
				if (UEchoGameInstance* GameInstance = WeakThis.IsValid() ? EchoGameInstanceFor(WeakThis.Get()) : nullptr)
				{
					GameInstance->ReturnToTitle(FText::GetEmpty());
				}
			} },
		{ LOCTEXT("LeaveGameNo", "Cancel"), nullptr },
	});
}

#undef LOCTEXT_NAMESPACE
