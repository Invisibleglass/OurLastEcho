// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EchoUITestLibrary.generated.h"

class UWidget;
class APlayerController;
class UEchoSessionSubsystem;
class UEchoLoginSubsystem;
class UEchoVoiceSubsystem;
class UEchoNetDiagnostics;

/**
 *  Sends real input to the UI through Slate, for the front-end tests: key presses (keyboard or gamepad buttons,
 *  e.g. Gamepad_DPad_Down, Gamepad_FaceButton_Bottom) and mouse clicks on a widget. They take exactly the path a
 *  player's input takes (navigation, focus, CommonUI's action router, button clicks).
 */
UCLASS()
class UEchoUITestLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Presses and releases a key or gamepad button for Slate user 0 */
	UFUNCTION(BlueprintCallable, Category="Echo|Test")
	static bool SendKey(FKey Key);

	/** Moves the mouse onto the widget's centre and clicks the left button there */
	UFUNCTION(BlueprintCallable, Category="Echo|Test")
	static bool ClickWidget(UWidget* Widget);

	/** Moves the mouse onto the widget's centre (hover) */
	UFUNCTION(BlueprintCallable, Category="Echo|Test")
	static bool HoverWidget(UWidget* Widget);

	/**
	 *  Same as ClickWidget / SendKey, but on the next engine tick instead of right now. Use these from editor Python
	 *  when the click leads to a client-to-server RPC (e.g. the lobby's Ready): while Python runs, the editor
	 *  runs such RPCs locally instead of sending them, so the click has to happen outside the Python call.
	 */
	UFUNCTION(BlueprintCallable, Category="Echo|Test")
	static void QueueClickWidget(UWidget* Widget);

	UFUNCTION(BlueprintCallable, Category="Echo|Test")
	static void QueueKey(FKey Key);

	/** Runs a console command on this player controller on the next engine tick (outside Python, for the same reason) */
	UFUNCTION(BlueprintCallable, Category="Echo|Test")
	static void QueueConsoleCommand(APlayerController* PlayerController, const FString& Command);

	/**
	 *  Moves the window the widget is in (e.g. the second PIE player's window) to a screen position. That window
	 *  floats above the editor, so where it overlaps the first player's viewport it catches that player's clicks.
	 */
	UFUNCTION(BlueprintCallable, Category="Echo|Test")
	static bool MoveWidgetWindow(UWidget* Widget, FVector2D ScreenPosition);

	/** Does the widget or one of its children have keyboard/gamepad focus? */
	UFUNCTION(BlueprintPure, Category="Echo|Test")
	static bool HasFocus(UWidget* Widget);

	/** Name of the UMG widget class that has focus (e.g. EchoButton), for logs */
	UFUNCTION(BlueprintPure, Category="Echo|Test")
	static FString GetFocusedWidgetDescription();

	// ---- Online subsystems of a world's game instance (Python in -game can't reach game instance subsystems
	// through SubsystemBlueprintLibrary)

	UFUNCTION(BlueprintPure, Category="Echo|Test", meta=(WorldContext="WorldContextObject"))
	static UEchoSessionSubsystem* GetSessions(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category="Echo|Test", meta=(WorldContext="WorldContextObject"))
	static UEchoLoginSubsystem* GetLogin(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category="Echo|Test", meta=(WorldContext="WorldContextObject"))
	static UEchoVoiceSubsystem* GetVoice(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category="Echo|Test", meta=(WorldContext="WorldContextObject"))
	static UEchoNetDiagnostics* GetNetDiagnostics(const UObject* WorldContextObject);

	/** The online service this world uses ("EOS", "NULL") */
	UFUNCTION(BlueprintPure, Category="Echo|Test", meta=(WorldContext="WorldContextObject"))
	static FString GetOnlineService(const UObject* WorldContextObject);

	/** The game instance's pending title-screen message (e.g. "The host left the game.") */
	UFUNCTION(BlueprintPure, Category="Echo|Test", meta=(WorldContext="WorldContextObject"))
	static FString GetPendingMessage(const UObject* WorldContextObject);
};
