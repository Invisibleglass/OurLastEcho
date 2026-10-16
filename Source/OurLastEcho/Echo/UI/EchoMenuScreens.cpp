// Our Last Echo

#include "EchoMenuScreens.h"
#include "Blueprint/WidgetTree.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Containers/Ticker.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EchoFrontEndGameMode.h"
#include "EchoFrontEndPlayerController.h"
#include "EchoGameInstance.h"
#include "EchoLoginSubsystem.h"
#include "EchoOnline.h"
#include "EchoSessionSubsystem.h"
#include "EchoSettingsScreen.h"
#include "EchoVoiceSubsystem.h"

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

	UEchoLoginSubsystem* LoginFor(const UUserWidget* Widget)
	{
		const UGameInstance* GameInstance = Widget ? Widget->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UEchoLoginSubsystem>() : nullptr;
	}

	void AskToQuitFrom(UEchoScreen* Screen)
	{
		TWeakObjectPtr<UEchoScreen> WeakScreen(Screen);
		Screen->GetRoot()->ShowDialog(LOCTEXT("QuitTitle", "Quit"), LOCTEXT("QuitMessage", "Quit Our Last Echo?"),
		{
			{ LOCTEXT("QuitYes", "Quit"), [WeakScreen]()
				{
					if (WeakScreen.IsValid())
					{
						UKismetSystemLibrary::QuitGame(WeakScreen.Get(), WeakScreen->GetOwningPlayer(), EQuitPreference::Quit, false);
					}
				} },
			{ LOCTEXT("QuitNo", "Cancel"), nullptr },
		});
	}
}

// ------------------------------------------------------------------ login

void UEchoLoginScreen::BuildContent(UVerticalBox* Content)
{
	AddTitle(Content, LOCTEXT("LoginGameTitle", "Our Last Echo"), 56);
	AddText(Content, LOCTEXT("LoginTagline", "Two eras. One canyon."), 18, EchoUI::DimTextColor);
	AddSpacer(Content, 28.0f);
	StatusText = AddText(Content, FText::GetEmpty(), 20);
	HintText = AddText(Content, FText::GetEmpty(), 16, EchoUI::DimTextColor);
	AddSpacer(Content, 16.0f);
	RetryButton = AddButton(Content, LOCTEXT("Retry", "Try Again"), [this]()
	{
		if (UEchoLoginSubsystem* Login = LoginFor(this))
		{
			Login->Login();
		}
	});
	AddButton(Content, LOCTEXT("LoginQuit", "Quit"), [this]() { AskToQuitFrom(this); });
}

void UEchoLoginScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	UEchoLoginSubsystem* Login = LoginFor(this);
	if (!Login)
	{
		return;
	}
	Login->OnLoginChanged.RemoveAll(this);
	Login->OnLoginChanged.AddUObject(this, &UEchoLoginScreen::HandleLoginChanged);
	if (Login->GetState() == EEchoLoginState::NotStarted)
	{
		Login->Login();
	}
	HandleLoginChanged();
}

void UEchoLoginScreen::NativeDestruct()
{
	if (UEchoLoginSubsystem* Login = LoginFor(this))
	{
		Login->OnLoginChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UEchoLoginScreen::OnBack()
{
	AskToQuitFrom(this);
}

void UEchoLoginScreen::HandleLoginChanged()
{
	Refresh();
	const UEchoLoginSubsystem* Login = LoginFor(this);
	if (!Login || !Login->IsLoggedIn() || bDone)
	{
		return;
	}
	// Signed in: the main menu takes over, on the next tick (this can run inside our own activation)
	bDone = true;
	TWeakObjectPtr<UEchoLoginScreen> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
	{
		if (WeakThis.IsValid() && WeakThis->GetRoot())
		{
			UEchoUIRoot* UIRoot = WeakThis->GetRoot();
			WeakThis->DeactivateWidget();
			UIRoot->Push<UEchoMainMenuScreen>();
		}
		return false;
	}));
}

void UEchoLoginScreen::Refresh()
{
	const UEchoLoginSubsystem* Login = LoginFor(this);
	const EEchoLoginState State = Login ? Login->GetState() : EEchoLoginState::Failed;
	switch (State)
	{
	case EEchoLoginState::LoggedIn:
		StatusText->SetText(FText::Format(LOCTEXT("SignedIn", "Signed in as {0}"), FText::FromString(Login->GetDisplayName())));
		HintText->SetText(FText::GetEmpty());
		break;
	case EEchoLoginState::Failed:
		StatusText->SetText(Login ? Login->GetErrorText() : LOCTEXT("NoLogin", "Couldn't sign in."));
		HintText->SetText(LOCTEXT("FailedHint", "Check your internet connection, then try again."));
		break;
	default:
		StatusText->SetText(LOCTEXT("SigningIn", "Signing in with your Epic account..."));
		HintText->SetText(LOCTEXT("SigningInHint", "The first time, an Epic sign-in window opens (in the game or your web browser)."));
		break;
	}
	RetryButton->SetVisibility(State == EEchoLoginState::Failed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (State == EEchoLoginState::Failed && IsActivated())
	{
		RetryButton->SetFocus();
	}
}

FString UEchoLoginScreen::GetStatusText() const
{
	return StatusText ? StatusText->GetText().ToString() : FString();
}

// ------------------------------------------------------------------ main menu

void UEchoMainMenuScreen::BuildContent(UVerticalBox* Content)
{
	AddTitle(Content, LOCTEXT("GameTitle", "Our Last Echo"), 56);
	AddText(Content, LOCTEXT("Tagline", "Two eras. One canyon."), 18, EchoUI::DimTextColor);
	if (const UEchoLoginSubsystem* Login = LoginFor(this); Login && Login->IsLoggedIn())
	{
		AddText(Content, Login->IsUsingEpicAccount()
			? FText::Format(LOCTEXT("SignedInEpic", "Signed in as {0}  (Epic account)"), FText::FromString(Login->GetDisplayName()))
			: FText::Format(LOCTEXT("SignedInLan", "{0}  (LAN only)"), FText::FromString(Login->GetDisplayName())), 14, EchoUI::DimTextColor);
	}
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
	AskToQuitFrom(this);
}

void UEchoMainMenuScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (UEchoSessionSubsystem* Sessions = SessionsFor(this))
	{
		Sessions->OnInviteReceived.RemoveAll(this);
		Sessions->OnInviteReceived.AddWeakLambda(this, [this](const FString&) { OfferInvite(); });
		if (Sessions->HasPendingInvite())
		{
			OfferInvite();
		}
	}
}

void UEchoMainMenuScreen::NativeDestruct()
{
	if (UEchoSessionSubsystem* Sessions = SessionsFor(this))
	{
		Sessions->OnInviteReceived.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UEchoMainMenuScreen::OfferInvite()
{
	UEchoSessionSubsystem* Sessions = SessionsFor(this);
	if (!Sessions || !Sessions->HasPendingInvite() || bInviteDialogOpen || !GetRoot())
	{
		return;
	}
	bInviteDialogOpen = true;
	TWeakObjectPtr<UEchoMainMenuScreen> WeakThis(this);
	auto Answer = [WeakThis](bool bAccept)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		WeakThis->bInviteDialogOpen = false;
		if (UEchoSessionSubsystem* Sessions = SessionsFor(WeakThis.Get()))
		{
			if (bAccept)
			{
				WeakThis->GetRoot()->ShowDialog(LOCTEXT("InviteJoinTitle", "Join Game"), LOCTEXT("InviteJoining", "Joining..."), {});
			}
			Sessions->AnswerPendingInvite(bAccept);
		}
	};
	GetRoot()->ShowDialog(LOCTEXT("InviteTitle", "Invite"),
		FText::Format(LOCTEXT("InviteMessage", "{0} invited you to play. Join their game as Saraa?"), FText::FromString(Sessions->GetPendingInviteFrom())),
		{
			{ LOCTEXT("InviteYes", "Join"), [Answer]() { Answer(true); } },
			{ LOCTEXT("InviteNo", "Not now"), [Answer]() { Answer(false); } },
		});
}

// ------------------------------------------------------------------ play

void UEchoPlayMenuScreen::BuildContent(UVerticalBox* Content)
{
	AddTitle(Content, LOCTEXT("PlayTitle", "Play"));
	const UEchoSessionSubsystem* Sessions = SessionsFor(this);
	AddText(Content, Sessions && Sessions->IsInternetService()
		? LOCTEXT("PlayHintOnline", "Host a game as Bat and invite a friend, or join a friend's game as Saraa.")
		: LOCTEXT("PlayHint", "Host a game as Bat, or join a friend's game as Saraa (LAN)."), 18, EchoUI::DimTextColor);
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
	const UEchoSessionSubsystem* Sessions = SessionsFor(this);
	const bool bInternet = Sessions && Sessions->IsInternetService();

	AddTitle(Content, LOCTEXT("JoinTitle", "Join Game"));
	AddText(Content, LOCTEXT("CodePrompt", "Type the join code shown in your friend's lobby:"), 18);
	AddSpacer(Content, 6.0f);

	USizeBox* CodeSize = WidgetTree->ConstructWidget<USizeBox>();
	CodeSize->SetWidthOverride(320.0f);
	CodeBox = WidgetTree->ConstructWidget<UEditableTextBox>();
	CodeBox->SetHintText(LOCTEXT("CodeHint", "e.g. K7Q2MD"));
	FEditableTextBoxStyle Style = CodeBox->WidgetStyle;
	Style.TextStyle.Font = EchoUI::Font(24, true);
	CodeBox->WidgetStyle = Style;
	CodeBox->OnTextCommitted.AddDynamic(this, &UEchoJoinMenuScreen::HandleCodeCommitted);
	CodeSize->SetContent(CodeBox);
	Content->AddChildToVerticalBox(CodeSize)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	AddButton(Content, LOCTEXT("JoinCode", "Join with Code"), [this]() { JoinCode(); });
	FirstFocus = CodeBox;

	AddSpacer(Content, 12.0f);
	if (bInternet)
	{
		AddText(Content, LOCTEXT("InviteHint", "Or ask your friend for an invite: accept it in the Epic overlay (Shift+F3)\nand you join straight away."), 15, EchoUI::DimTextColor);
	}
	StatusText = AddText(Content, FText::GetEmpty(), 18, EchoUI::DimTextColor);
	AddSpacer(Content, 8.0f);

	GameList = WidgetTree->ConstructWidget<UVerticalBox>();
	Content->AddChildToVerticalBox(GameList);
	AddSpacer(Content, 12.0f);

	if (!bInternet)
	{
		AddButton(Content, LOCTEXT("Refresh", "Refresh"), [this]() { Refresh(); });
	}
	AddButton(Content, LOCTEXT("JoinBack", "Back"), [this]() { DeactivateWidget(); });
}

void UEchoJoinMenuScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	const UEchoSessionSubsystem* Sessions = SessionsFor(this);
	if (Sessions && !Sessions->IsInternetService())
	{
		Refresh();
	}
}

void UEchoJoinMenuScreen::HandleCodeCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		JoinCode();
	}
}

void UEchoJoinMenuScreen::JoinWithCode(const FString& Code)
{
	CodeBox->SetText(FText::FromString(Code));
	JoinCode();
}

void UEchoJoinMenuScreen::JoinCode()
{
	UEchoSessionSubsystem* Sessions = SessionsFor(this);
	if (!Sessions || Sessions->IsJoining())
	{
		return;
	}
	FString Code = CodeBox->GetText().ToString().TrimStartAndEnd();
	Code.ReplaceInline(TEXT(" "), TEXT(""));
	Code.ReplaceInline(TEXT("-"), TEXT(""));
	if (Code.Len() != GetDefault<UEchoOnlineSettings>()->JoinCodeLength)
	{
		GetRoot()->ShowDialog(LOCTEXT("CodeTitle", "Join Game"),
			FText::Format(LOCTEXT("CodeWrong", "Join codes are {0} letters and numbers, like K7Q2MD."), FText::AsNumber(GetDefault<UEchoOnlineSettings>()->JoinCodeLength)),
			{ { LOCTEXT("CodeOK", "OK"), nullptr } });
		return;
	}
	ShowJoining();
	Sessions->OnJoinComplete.RemoveAll(this);
	Sessions->OnJoinComplete.AddUObject(this, &UEchoJoinMenuScreen::HandleJoinComplete);
	Sessions->JoinByCode(Code);
}

void UEchoJoinMenuScreen::ShowJoining()
{
	TWeakObjectPtr<UEchoJoinMenuScreen> WeakThis(this);
	JoiningDialog = GetRoot()->ShowDialog(LOCTEXT("JoiningTitle", "Join Game"), LOCTEXT("Joining", "Joining..."),
	{
		{ LOCTEXT("JoinCancel", "Cancel"), [WeakThis]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->JoiningDialog = nullptr;
					if (UEchoSessionSubsystem* Sessions = SessionsFor(WeakThis.Get()))
					{
						Sessions->OnJoinComplete.RemoveAll(WeakThis.Get());
						Sessions->CancelJoin();
					}
				}
			} },
	});
}

void UEchoJoinMenuScreen::Refresh()
{
	UEchoSessionSubsystem* Sessions = SessionsFor(this);
	if (!Sessions || Sessions->IsInternetService())
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
		? LOCTEXT("NoGames", "No games found on your network. Ask the other player to host, then Refresh.")
		: LOCTEXT("PickGame", "Or choose a game on your network to join as Saraa:"));

	for (const FEchoSessionInfo& Info : Results)
	{
		const int32 SessionIndex = Info.Index;
		UEchoButton* Button = MakeButton(FText::Format(LOCTEXT("GameEntry", "{0}'s game  ({1} ms)"), FText::FromString(Info.HostName), FText::AsNumber(Info.PingMs)),
			[this, SessionIndex]() { Join(SessionIndex); });
		GameList->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 4.0f));
		GameButtons.Add(Button);
		GameIndices.Add(SessionIndex);
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

	ShowJoining();
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
		if (JoiningDialog->IsActivated())
		{
			JoiningDialog->DeactivateWidget();
		}
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
	CodeLine = AddText(Content, FText::GetEmpty(), 26, EchoUI::AccentColor);
	AddSpacer(Content, 4.0f);
	BatLine = AddText(Content, FText::GetEmpty(), 22);
	SaraaLine = AddText(Content, FText::GetEmpty(), 22);
	AddSpacer(Content, 8.0f);
	HintText = AddText(Content, FText::GetEmpty(), 16, EchoUI::DimTextColor);
	VoiceLine = AddText(Content, FText::GetEmpty(), 14, EchoUI::DimTextColor);
	AddSpacer(Content, 20.0f);

	InviteButton = AddButton(Content, LOCTEXT("Invite", "Invite Friend"), [this]() { InviteFriend(); });
	FriendsButton = AddButton(Content, LOCTEXT("FriendsList", "Invite from Friends List"), [this]()
	{
		GetRoot()->Push<UEchoInviteScreen>();
	});
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

	const UEchoSessionSubsystem* Sessions = SessionsFor(this);
	const bool bInternet = Sessions && Sessions->IsInternetService();
	const FString Code = Sessions ? Sessions->GetJoinCode() : FString();
	const bool bShowCode = Me->IsHostPlayer() && !Code.IsEmpty();
	CodeLine->SetText(bShowCode ? FText::Format(LOCTEXT("CodeLine", "Join code:  {0}"), FText::FromString(Code)) : FText::GetEmpty());
	CodeLine->SetVisibility(bShowCode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	// Inviting: host only, internet only, until Saraa is here
	const ESlateVisibility InviteVisibility = Me->IsHostPlayer() && bInternet && !Guest ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	InviteButton->SetVisibility(InviteVisibility);
	FriendsButton->SetVisibility(InviteVisibility);

	HintText->SetText(!Guest
		? (bInternet
			? LOCTEXT("HintWaitOnline", "Waiting for Saraa: invite a friend, or give them the join code\n(they choose Play > Join Game and type it in).")
			: LOCTEXT("HintWait", "Waiting for Saraa: on another machine on this network, choose Play > Join Game\n(pick this game, or type the join code)."))
		: bBothReady
			? (Me->IsHostPlayer() ? LOCTEXT("HintStart", "Both ready. Start when you like.") : LOCTEXT("HintHostStarts", "Both ready. Waiting for Bat to start."))
			: LOCTEXT("HintReady", "Both players must be ready to start."));

	const UGameInstance* GameInstance = GetGameInstance();
	const UEchoVoiceSubsystem* Voice = GameInstance ? GameInstance->GetSubsystem<UEchoVoiceSubsystem>() : nullptr;
	VoiceLine->SetText(Voice ? FText::FromString(Voice->GetStatusText()) : FText::GetEmpty());
}

void UEchoLobbyScreen::InviteFriend()
{
	UEchoSessionSubsystem* Sessions = SessionsFor(this);
	if (Sessions && !Sessions->OpenInviteOverlay())
	{
		// No overlay: our own list of Epic friends
		GetRoot()->Push<UEchoInviteScreen>();
	}
}

FString UEchoLobbyScreen::GetPlayersText() const
{
	return BatLine->GetText().ToString() + TEXT(" | ") + SaraaLine->GetText().ToString();
}

FString UEchoLobbyScreen::GetCodeText() const
{
	return CodeLine && CodeLine->GetVisibility() != ESlateVisibility::Collapsed ? CodeLine->GetText().ToString() : FString();
}

// ------------------------------------------------------------------ invite from the friends list

void UEchoInviteScreen::BuildContent(UVerticalBox* Content)
{
	AddTitle(Content, LOCTEXT("InviteListTitle", "Invite a Friend"));
	StatusText = AddText(Content, LOCTEXT("ReadingFriends", "Reading your Epic friends list..."), 18, EchoUI::DimTextColor);
	AddSpacer(Content, 12.0f);

	USizeBox* ListSize = WidgetTree->ConstructWidget<USizeBox>();
	ListSize->SetMaxDesiredHeight(380.0f);
	UScrollBox* ListScroll = WidgetTree->ConstructWidget<UScrollBox>();
	FriendList = WidgetTree->ConstructWidget<UVerticalBox>();
	ListScroll->AddChild(FriendList);
	ListSize->SetContent(ListScroll);
	Content->AddChildToVerticalBox(ListSize);
	AddSpacer(Content, 12.0f);
	AddButton(Content, LOCTEXT("InviteBack", "Back"), [this]() { DeactivateWidget(); });
}

void UEchoInviteScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (UEchoSessionSubsystem* Sessions = SessionsFor(this))
	{
		Sessions->OnFriendsRead.RemoveAll(this);
		Sessions->OnFriendsRead.AddUObject(this, &UEchoInviteScreen::HandleFriendsRead);
		Sessions->ReadFriends();
	}
}

void UEchoInviteScreen::NativeDestruct()
{
	if (UEchoSessionSubsystem* Sessions = SessionsFor(this))
	{
		Sessions->OnFriendsRead.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UEchoInviteScreen::HandleFriendsRead(bool bSuccess, const FText& Error)
{
	UEchoSessionSubsystem* Sessions = SessionsFor(this);
	if (!Sessions)
	{
		return;
	}
	FriendList->ClearChildren();
	TArray<FEchoFriendInfo> Sorted = Sessions->GetFriends();
	Sorted.StableSort([](const FEchoFriendInfo& A, const FEchoFriendInfo& B) { return A.bOnline && !B.bOnline; });
	StatusText->SetText(!bSuccess ? Error
		: Sorted.Num() == 0 ? LOCTEXT("NoFriendsYet", "No Epic friends yet. Add your friend in the Epic Games Launcher, or give them the join code.")
		: LOCTEXT("PickFriend", "Choose who to invite (online friends first):"));
	for (const FEchoFriendInfo& Friend : Sorted)
	{
		const int32 Index = Friend.Index;
		const FString Name = Friend.Name;
		UEchoButton* Button = MakeButton(FText::Format(LOCTEXT("FriendEntry", "{0}{1}"), FText::FromString(Name),
			Friend.bOnline ? LOCTEXT("FriendOnline", "  (online)") : FText::GetEmpty()), [this, Index, Name]()
			{
				UEchoSessionSubsystem* Sessions = SessionsFor(this);
				const bool bSent = Sessions && Sessions->InviteFriend(Index);
				GetRoot()->ShowToast(bSent
					? FText::Format(LOCTEXT("InviteSent", "Invite sent to {0}."), FText::FromString(Name))
					: FText::Format(LOCTEXT("InviteNotSent", "Couldn't invite {0}."), FText::FromString(Name)));
			});
		FriendList->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 4.0f));
	}
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
