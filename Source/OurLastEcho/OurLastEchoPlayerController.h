// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OurLastEchoPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UUserWidget;
class UEchoUIRoot;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings, and the in-game menu
 */
UCLASS(abstract)
class AOurLastEchoPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Opens the in-game menu: Esc or P / gamepad Start */
	UPROPERTY(EditAnywhere, Category="Echo|Menu")
	TSoftObjectPtr<UInputAction> MenuAction;

	UPROPERTY(EditAnywhere, Category="Echo|Menu")
	TSoftObjectPtr<UInputMappingContext> MenuMappingContext;

	/** The menu layer (CommonUI screen stack) */
	UPROPERTY(Transient)
	TObjectPtr<UEchoUIRoot> UIRoot;

public:

	AOurLastEchoPlayerController();

	/** Shows the in-game menu over the game, and pauses the game for both players until everyone closes theirs */
	UFUNCTION(BlueprintCallable, Category="Echo|Menu")
	void OpenPauseMenu();

	/** Closes every menu screen and returns to the game */
	UFUNCTION(BlueprintCallable, Category="Echo|Menu")
	void ClosePauseMenu();

	UFUNCTION(BlueprintCallable, Category="Echo|Menu")
	void TogglePauseMenu();

	UFUNCTION(BlueprintPure, Category="Echo|Menu")
	bool IsPauseMenuOpen() const;

	UFUNCTION(BlueprintPure, Category="Echo|UI")
	UEchoUIRoot* GetUIRoot() const { return UIRoot; }

	/** Console command: toggles the in-game menu (handy in PIE, where Esc stops the session) */
	UFUNCTION(Exec)
	void EchoMenu();

	/** Console command: shows a test subtitle, e.g. EchoSubtitle Hello there */
	UFUNCTION(Exec)
	void EchoSubtitle(const FString& Text);

	/** Debug console command: preview the other era on this machine only (again = back) */
	UFUNCTION(Exec)
	void EchoSwapEra();

	/** Debug console command: EchoEra present | past | auto (auto = this player's own era) */
	UFUNCTION(Exec)
	void EchoEra(const FString& Era);

	/** Debug console command: load both eras at once, to check that they line up (again = off) */
	UFUNCTION(Exec)
	void EchoShowBothEras();

	// ---- Combat debug commands (Milestone 8); the server carries them out

	/** God mode for your own character (again = off) */
	UFUNCTION(Exec)
	void EchoGod();

	/** EchoSpawnEnemy wraith | shade | guardian | talon: spawns one a few metres in front of you */
	UFUNCTION(Exec)
	void EchoSpawnEnemy(const FString& Type);

	/** Shows every enemy to both players, whoever's world it's in (again = off) */
	UFUNCTION(Exec)
	void EchoShowAllEnemies();

	/** EchoSkipToWave 3: jumps the arena to that wave */
	UFUNCTION(Exec)
	void EchoSkipToWave(int32 Wave);

	/** Restarts the arena (like the restart pad) */
	UFUNCTION(Exec)
	void EchoRestartArena();

	/** EchoHurt 30: damages your own character (testing health, hurt reactions, going down) */
	UFUNCTION(Exec)
	void EchoHurt(float Amount);

	/** EchoGoTo combat | back: takes both players through the combat door (or back) without walking there */
	UFUNCTION(Exec)
	void EchoGoTo(const FString& Where);

	/** Tests and debugging: EchoShoot quick | aimed | charged, through the bow's own input path (charged waits out the draw) */
	UFUNCTION(Exec)
	void EchoShoot(const FString& Kind);

	/** Tests and debugging: EchoLash combo | charged, through the whip's own input path (sends the server RPC) */
	UFUNCTION(Exec)
	void EchoLash(const FString& Kind);

	/** Tests and debugging: a dodge roll, as the dodge key does */
	UFUNCTION(Exec)
	void EchoDodge();

	/** Runs a combat debug command on the server (what the Echo... commands above call) */
	UFUNCTION(Server, Reliable)
	void ServerCombatDebug(const FString& Command, const FString& Argument);

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	/** Back to game input once the last menu screen closes */
	void HandleMenusClosed();

	/** Tells the server this player's menu is open or closed (it pauses the game while any player's is) */
	void SetInPauseMenu(bool bOpen);

	UFUNCTION(Server, Reliable)
	void ServerSetInPauseMenu(bool bOpen);

	/** What the server was last told, so it's only told about changes */
	bool bToldServerMenuOpen = false;
};
