// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OurLastEchoPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UUserWidget;
class UEchoSettingsMenu;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
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

	/** Pause / settings menu layout (a Widget Blueprint based on UEchoSettingsMenu) */
	UPROPERTY(EditAnywhere, Category="Echo|Settings Menu")
	TSoftClassPtr<UEchoSettingsMenu> SettingsMenuClass;

	/** Opens/closes the menu: Esc or P / gamepad Start (triggers while paused, so either player can open it) */
	UPROPERTY(EditAnywhere, Category="Echo|Settings Menu")
	TSoftObjectPtr<UInputAction> MenuAction;

	UPROPERTY(EditAnywhere, Category="Echo|Settings Menu")
	TSoftObjectPtr<UInputMappingContext> MenuMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UEchoSettingsMenu> SettingsMenu;

public:

	AOurLastEchoPlayerController();

	/** Shows the settings menu and pauses the game for both players (local player controllers only) */
	UFUNCTION(BlueprintCallable, Category="Echo|Settings Menu")
	void OpenSettingsMenu();

	/** Hides the menu; the game resumes once nobody has it open */
	UFUNCTION(BlueprintCallable, Category="Echo|Settings Menu")
	void CloseSettingsMenu();

	UFUNCTION(BlueprintCallable, Category="Echo|Settings Menu")
	void ToggleSettingsMenu();

	UFUNCTION(BlueprintPure, Category="Echo|Settings Menu")
	bool IsSettingsMenuOpen() const;

	/** Console command: toggles the settings menu (handy in PIE, where Esc stops the session) */
	UFUNCTION(Exec)
	void EchoMenu();

protected:

	/** Tells the server this player opened/closed the menu; it pauses while anyone has it open */
	UFUNCTION(Server, Reliable)
	void ServerSetInSettingsMenu(bool bOpen);

	void ReportMenuState(bool bOpen);

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

};
