// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "EchoUIWidgets.h"
#include "EchoMenuScreens.generated.h"

class UScrollBox;

/** Title screen: the game's name and Play, Settings, Credits, Quit */
UCLASS()
class UEchoMainMenuScreen : public UEchoScreen
{
	GENERATED_BODY()

protected:

	virtual void BuildContent(UVerticalBox* Content) override;

	/** The main menu is the bottom of the stack: Back offers to quit */
	virtual void OnBack() override;

	void AskToQuit();
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

/** Join Game: games found on the network, Refresh and Back */
UCLASS()
class UEchoJoinMenuScreen : public UEchoScreen
{
	GENERATED_BODY()

public:

	/** Searches again */
	UFUNCTION(BlueprintCallable, Category="Echo|UI")
	void Refresh();

	/** Joins the first game in the list (for tests); false if there's none */
	UFUNCTION(BlueprintCallable, Category="Echo|UI")
	bool JoinFirstGame();

	UFUNCTION(BlueprintPure, Category="Echo|UI")
	int32 GetNumGamesListed() const { return GameButtons.Num(); }

protected:

	virtual void BuildContent(UVerticalBox* Content) override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;

	void HandleFindComplete(bool bSuccess, const FText& Error);
	void HandleJoinComplete(bool bSuccess, const FText& Error);
	void Join(int32 SessionIndex);

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

protected:

	virtual void BuildContent(UVerticalBox* Content) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void OnBack() override;

	void AskToLeave();

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
