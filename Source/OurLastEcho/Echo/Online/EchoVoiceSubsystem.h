// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EchoVoiceSubsystem.generated.h"

class IVoiceChatUser;
class FEchoPushToTalkInput;

/**
 *  Voice chat between the two players (Milestone 7), through EOS voice: the host's EOS lobby has a voice room
 *  that both players join automatically with the session, so voice works in the lobby and in the game, and ends
 *  when the session does. Null (LAN) has no voice.
 *
 *  Settings > Audio: Microphone on/off, Voice mode (open mic / push-to-talk), the push-to-talk key, and the voice
 *  chat volume. Push-to-talk listens to raw key events (a Slate input pre-processor), so it works in menus and
 *  in game alike, and never eats the key.
 */
UCLASS()
class UEchoVoiceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** In a voice room with EOS (voice can work) */
	UFUNCTION(BlueprintPure, Category="Echo|Voice")
	bool IsInVoiceRoom() const;

	/** The other player is talking */
	UFUNCTION(BlueprintPure, Category="Echo|Voice")
	bool IsOtherPlayerTalking() const { return bOtherTalking; }

	/** The microphone is live (mic on, and open mic or push-to-talk held) */
	UFUNCTION(BlueprintPure, Category="Echo|Voice")
	bool IsTransmitting() const { return bTransmitting; }

	/** Push-to-talk mode with the key held (the overlay shows "Mic on") */
	UFUNCTION(BlueprintPure, Category="Echo|Voice")
	bool IsPushToTalkActive() const;

	/** Holds or releases push-to-talk (the key does this; also for tests) */
	UFUNCTION(BlueprintCallable, Category="Echo|Voice")
	void SetPushToTalkHeld(bool bHeld) { bPushToTalkHeld = bHeld; }

	/** One line for menus and tests, e.g. "Voice: connected, open mic" */
	UFUNCTION(BlueprintPure, Category="Echo|Voice")
	FString GetStatusText() const;

	/** The game window this game instance draws into has keyboard focus (push-to-talk keys count only then) */
	bool OwnsFocusedWindow() const;

private:

	bool Tick(float DeltaTime);
	IVoiceChatUser* FindVoiceUser() const;
	void Bind(IVoiceChatUser* User);
	void HandleTalkingUpdated(const FString& ChannelName, const FString& PlayerName, bool bIsTalking);

	FTSTicker::FDelegateHandle TickHandle;
	TSharedPtr<FEchoPushToTalkInput> InputProcessor;

	IVoiceChatUser* BoundUser = nullptr;
	FDelegateHandle TalkingHandle;
	TSet<FString> OthersTalking;

	bool bPushToTalkHeld = false;
	bool bOtherTalking = false;
	bool bTransmitting = false;
	TOptional<bool> AppliedMuted;
	TOptional<float> AppliedVolume;
	bool bWasInRoom = false;
};
