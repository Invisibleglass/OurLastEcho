// Our Last Echo

#include "EchoVoiceSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "IOnlineSubsystemEOS.h"
#include "VoiceChat.h"
#include "Widgets/SWindow.h"
#include "EchoGameUserSettings.h"
#include "EchoNetDiagnostics.h"
#include "EchoOnline.h"

/** Sees every key before the UI and the game do; only notes the push-to-talk keys, never handles them */
class FEchoPushToTalkInput : public IInputProcessor
{
public:

	explicit FEchoPushToTalkInput(UEchoVoiceSubsystem* InOwner) : Owner(InOwner) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		Note(InKeyEvent.GetKey(), true);
		return false;
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		Note(InKeyEvent.GetKey(), false);
		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		Note(MouseEvent.GetEffectingButton(), true);
		return false;
	}

	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		Note(MouseEvent.GetEffectingButton(), false);
		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("EchoPushToTalk"); }

	TWeakObjectPtr<UEchoVoiceSubsystem> Owner;

private:

	void Note(const FKey& Key, bool bDown)
	{
		const UEchoGameUserSettings* Settings = UEchoGameUserSettings::GetEchoSettings();
		if (!Owner.IsValid() || !Settings || (Key != Settings->PushToTalkKey && Key != Settings->PushToTalkGamepadKey))
		{
			return;
		}
		// Releases always count; presses only in this game's own window (Play-In-Editor has one per player)
		if (!bDown || Owner->OwnsFocusedWindow())
		{
			Owner->SetPushToTalkHeld(bDown);
		}
	}
};

void UEchoVoiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UEchoVoiceSubsystem::Tick), 0.05f);
	if (FSlateApplication::IsInitialized())
	{
		InputProcessor = MakeShared<FEchoPushToTalkInput>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
	}
}

void UEchoVoiceSubsystem::Deinitialize()
{
	FTSTicker::RemoveTicker(TickHandle);
	if (InputProcessor && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	InputProcessor.Reset();
	// The voice user belongs to the online subsystem; only touch it if it's still the live one
	if (BoundUser && FindVoiceUser() != BoundUser)
	{
		BoundUser = nullptr;
	}
	Bind(nullptr);
	Super::Deinitialize();
}

bool UEchoVoiceSubsystem::OwnsFocusedWindow() const
{
	const UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	return Window.IsValid() && FSlateApplication::IsInitialized() && FSlateApplication::Get().GetActiveTopLevelWindow() == Window;
}

IVoiceChatUser* UEchoVoiceSubsystem::FindVoiceUser() const
{
	IOnlineSubsystem* Subsystem = EchoOnline::GetSubsystem(GetGameInstance());
	if (!Subsystem || Subsystem->GetSubsystemName() != EchoOnline::EOSName)
	{
		return nullptr;
	}
	const IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
	const FUniqueNetIdPtr UserId = Identity && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn ? Identity->GetUniquePlayerId(0) : nullptr;
	return UserId ? static_cast<IOnlineSubsystemEOS*>(Subsystem)->GetVoiceChatUserInterface(*UserId) : nullptr;
}

void UEchoVoiceSubsystem::Bind(IVoiceChatUser* User)
{
	if (User == BoundUser)
	{
		return;
	}
	if (BoundUser)
	{
		BoundUser->OnVoiceChatPlayerTalkingUpdated().Remove(TalkingHandle);
	}
	BoundUser = User;
	OthersTalking.Reset();
	AppliedMuted.Reset();
	AppliedVolume.Reset();
	if (BoundUser)
	{
		TalkingHandle = BoundUser->OnVoiceChatPlayerTalkingUpdated().AddUObject(this, &UEchoVoiceSubsystem::HandleTalkingUpdated);
	}
}

void UEchoVoiceSubsystem::HandleTalkingUpdated(const FString& ChannelName, const FString& PlayerName, bool bIsTalking)
{
	if (!BoundUser || PlayerName == BoundUser->GetLoggedInPlayerName())
	{
		return;
	}
	if (bIsTalking)
	{
		OthersTalking.Add(PlayerName);
	}
	else
	{
		OthersTalking.Remove(PlayerName);
	}
}

bool UEchoVoiceSubsystem::Tick(float DeltaTime)
{
	Bind(FindVoiceUser());

	const UEchoGameUserSettings* Settings = UEchoGameUserSettings::GetEchoSettings();
	const bool bInRoom = IsInVoiceRoom();
	if (bInRoom != bWasInRoom)
	{
		bWasInRoom = bInRoom;
		UEchoNetDiagnostics::Log(this, bInRoom ? TEXT("Voice: joined the voice room") : TEXT("Voice: left the voice room"));
		if (!bInRoom)
		{
			OthersTalking.Reset();
		}
	}

	bTransmitting = Settings && Settings->bMicrophoneEnabled && (Settings->VoiceMode == EEchoVoiceMode::OpenMic || bPushToTalkHeld);
	bOtherTalking = bInRoom && OthersTalking.Num() > 0;

	if (BoundUser && Settings)
	{
		const bool bMuted = !bTransmitting;
		if (!AppliedMuted.IsSet() || AppliedMuted.GetValue() != bMuted)
		{
			AppliedMuted = bMuted;
			BoundUser->SetAudioInputDeviceMuted(bMuted);
		}
		// EOS voice volume: 1 = normal (up to 2); the settings slider is 0..1, under the master volume
		const float Volume = FMath::Clamp(Settings->VoiceChatVolume * Settings->MasterVolume, 0.0f, 1.0f);
		if (!AppliedVolume.IsSet() || !FMath::IsNearlyEqual(AppliedVolume.GetValue(), Volume))
		{
			AppliedVolume = Volume;
			BoundUser->SetAudioOutputVolume(Volume);
		}
	}
	return true;
}

bool UEchoVoiceSubsystem::IsInVoiceRoom() const
{
	return BoundUser && BoundUser->IsLoggedIn() && BoundUser->GetChannels().Num() > 0;
}

bool UEchoVoiceSubsystem::IsPushToTalkActive() const
{
	const UEchoGameUserSettings* Settings = UEchoGameUserSettings::GetEchoSettings();
	return Settings && Settings->VoiceMode == EEchoVoiceMode::PushToTalk && bTransmitting && IsInVoiceRoom();
}

FString UEchoVoiceSubsystem::GetStatusText() const
{
	const UEchoGameUserSettings* Settings = UEchoGameUserSettings::GetEchoSettings();
	const FString Mode = !Settings || !Settings->bMicrophoneEnabled ? TEXT("mic off")
		: Settings->VoiceMode == EEchoVoiceMode::PushToTalk ? FString::Printf(TEXT("push-to-talk (%s)"), *Settings->PushToTalkKey.GetDisplayName().ToString())
		: TEXT("open mic");
	if (!EchoOnline::IsEOS(GetGameInstance()))
	{
		return TEXT("Voice: needs Epic Online Services (LAN games have no voice)");
	}
	return FString::Printf(TEXT("Voice: %s, %s%s"), IsInVoiceRoom() ? TEXT("connected") : TEXT("not in a game"), *Mode,
		bTransmitting && IsInVoiceRoom() ? TEXT(", transmitting") : TEXT(""));
}
