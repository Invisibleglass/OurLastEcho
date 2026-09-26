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

	/** Shows the in-game menu over the game. The game keeps running (it's online) */
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

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	/** Back to game input once the last menu screen closes */
	void HandleMenusClosed();
};
