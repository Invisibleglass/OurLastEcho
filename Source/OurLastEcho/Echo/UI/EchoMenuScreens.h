// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "EchoUIWidgets.h"
#include "EchoMenuScreens.generated.h"

class UEditableTextBox;
class UScrollBox;

/**
 *  First screen of a run (Milestone 7): signs in with the Epic account (UEchoLoginSubsystem), then gives way to
 *  the main menu. On failure: the reason, Try Again and Quit.
 */
UCLASS()
class UEchoLoginScreen : public UEchoScreen
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category="Echo|UI")
	FString GetStatusText() const;

protected:

	virtual void BuildContent(UVerticalBox* Content) override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;
	virtual void OnBack() override;

	void Refresh();
	void HandleLoginChanged();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintText;

	UPROPERTY(Transient)
	TObjectPtr<UEchoButton> RetryButton;

	bool bDone = false;
};

/** Title screen: the game's name and Play, Settings, Credits, Quit */
UCLASS()
class UEchoMainMenuScreen : public UEchoScreen
{
	GENERATED_BODY()

protected:

	virtual void BuildContent(UVerticalBox* Content) override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;

	/** The main menu is the bottom of the stack: Back offers to quit */
	virtual void OnBack() override;

	void AskToQuit();

	/** An invite arrived (or was waiting): offer to join */
	void OfferInvite();

	bool bInviteDialogOpen = false;
};

/** Play: Host Game or Join Game */
UCLASS()
class UEchoPlayMenuScreen : public UEchoScreen
{
	GENERATED_BODY()

protected:

	virtual void BuildContent(UVerticalBox* Content) override;
	virtual void NativeDestruct() override;

	void Host();
};

/**
 *  Join Game: type the host's join code (internet and LAN), or on LAN pick a game found on the network.
 *  Invites need no screen: accepting one in the Epic overlay joins straight away.
 */
UCLASS()
class UEchoJoinMenuScreen : public UEchoScreen
{
	GENERATED_BODY()

public:

	/** Searches again (LAN list) */
	UFUNCTION(BlueprintCallable, Category="Echo|UI")
	void Refresh();

	/** Joins the first game in the list (for tests); false if there's none */
	UFUNCTION(BlueprintCallable, Category="Echo|UI")
	bool JoinFirstGame();

	/** Types a code into the code box and presses Join (for tests) */
	UFUNCTION(BlueprintCallable, Category="Echo|UI")
	void JoinWithCode(const FString& Code);

	UFUNCTION(BlueprintPure, Category="Echo|UI")
	int32 GetNumGamesListed() const { return GameButtons.Num(); }

protected:

	virtual void BuildContent(UVerticalBox* Content) override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;

	void HandleFindComplete(bool bSuccess, const FText& Error);
	void HandleJoinComplete(bool bSuccess, const FText& Error);
	void Join(int32 SessionIndex);
	void JoinCode();
	void ShowJoining();

	UFUNCTION()
	void HandleCodeCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> CodeBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> GameList;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UEchoButton>> GameButtons;

	UPROPERTY(Transient)
	TObjectPtr<UEchoDialog> JoiningDialog;

	TArray<int32> GameIndices;
};

/** The lobby: both players with their roles, Ready, Start (host) and Leave */
UCLASS()
class UEchoLobbyScreen : public UEchoScreen
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category="Echo|UI")
	FString GetPlayersText() const;

	/** "Join code: K7Q2MD" as shown ("" for the guest; for tests) */
	UFUNCTION(BlueprintPure, Category="Echo|UI")
	FString GetCodeText() const;

protected:

	virtual void BuildContent(UVerticalBox* Content) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void OnBack() override;

	void AskToLeave();
	void InviteFriend();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CodeLine;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> VoiceLine;

	UPROPERTY(Transient)
	TObjectPtr<UEchoButton> InviteButton;

	UPROPERTY(Transient)
	TObjectPtr<UEchoButton> FriendsButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BatLine;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SaraaLine;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintText;

	UPROPERTY(Transient)
	TObjectPtr<UEchoButton> ReadyButton;

	UPROPERTY(Transient)
	TObjectPtr<UEchoButton> StartButton;
};

/** Invite from a list of Epic friends (when the Epic overlay isn't available, or the player prefers it) */
UCLASS()
class UEchoInviteScreen : public UEchoScreen
{
	GENERATED_BODY()

protected:

	virtual void BuildContent(UVerticalBox* Content) override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;

	void HandleFriendsRead(bool bSuccess, const FText& Error);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> FriendList;
};

/** Scrolling placeholder credits */
UCLASS()
class UEchoCreditsScreen : public UEchoScreen
{
	GENERATED_BODY()

protected:

	virtual void BuildContent(UVerticalBox* Content) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> Scroll;

	float Offset = 0.0f;
};

/** In-game menu over the running game (it doesn't pause: the game is online): Resume, Settings, Leave Game */
UCLASS()
class UEchoPauseMenuScreen : public UEchoScreen
{
	GENERATED_BODY()

protected:

	virtual void BuildContent(UVerticalBox* Content) override;
	virtual float GetBackdropOpacity() const override { return 0.35f; }
	virtual bool IsCentred() const override { return true; }

	void AskToLeave();
};
