// Our Last Echo

#include "EchoOnlineOverlay.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "EchoNetDiagnostics.h"
#include "EchoOnline.h"
#include "EchoUIWidgets.h"
#include "EchoVoiceSubsystem.h"

#define LOCTEXT_NAMESPACE "EchoOnlineOverlay"

namespace
{
	UBorder* MakeChip(UWidgetTree* Tree, UTextBlock* Text, const FLinearColor& Back)
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>();
		Border->SetBrushColor(Back);
		Border->SetPadding(FMargin(12.0f, 6.0f));
		Border->SetContent(Text);
		Border->SetVisibility(ESlateVisibility::Collapsed);
		return Border;
	}
}

void UEchoOnlineOverlay::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();

	StatsText = EchoUI::MakeText(WidgetTree, FText::GetEmpty(), 13);
	StatsBack = MakeChip(WidgetTree, StatsText, FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	UOverlaySlot* StatsSlot = Root->AddChildToOverlay(StatsBack);
	StatsSlot->SetHorizontalAlignment(HAlign_Right);
	StatsSlot->SetVerticalAlignment(VAlign_Top);
	StatsSlot->SetPadding(FMargin(0.0f, 16.0f, 16.0f, 0.0f));

	UVerticalBox* VoiceColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	SpeakingText = EchoUI::MakeText(WidgetTree, FText::GetEmpty(), 18, EchoUI::TextColor, true);
	SpeakingBack = MakeChip(WidgetTree, SpeakingText, FLinearColor(0.05f, 0.18f, 0.32f, 0.8f));
	VoiceColumn->AddChildToVerticalBox(SpeakingBack)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	MicText = EchoUI::MakeText(WidgetTree, LOCTEXT("MicOn", "Mic on"), 15, EchoUI::TextColor);
	MicBack = MakeChip(WidgetTree, MicText, FLinearColor(0.35f, 0.08f, 0.05f, 0.8f));
	VoiceColumn->AddChildToVerticalBox(MicBack);
	UOverlaySlot* VoiceSlot = Root->AddChildToOverlay(VoiceColumn);
	VoiceSlot->SetHorizontalAlignment(HAlign_Left);
	VoiceSlot->SetVerticalAlignment(VAlign_Bottom);
	VoiceSlot->SetPadding(FMargin(24.0f, 0.0f, 0.0f, 28.0f));

	WidgetTree->RootWidget = Root;
}

void UEchoOnlineOverlay::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UGameInstance* GameInstance = GetGameInstance();
	const UEchoNetDiagnostics* Diagnostics = GameInstance ? GameInstance->GetSubsystem<UEchoNetDiagnostics>() : nullptr;
	if (Diagnostics && Diagnostics->IsOverlayShown())
	{
		const FEchoNetStats Stats = Diagnostics->GetStats();
		StatsText->SetText(Stats.bConnected
			? FText::Format(LOCTEXT("Stats", "{0}  |  ping {1} ms\nloss in {2}%  out {3}%\nin {4} KB/s  out {5} KB/s"),
				FText::FromString(Stats.Transport), FText::AsNumber(FMath::RoundToInt(Stats.PingMs)),
				FText::AsNumber(FMath::RoundToInt(Stats.InLossPercent)), FText::AsNumber(FMath::RoundToInt(Stats.OutLossPercent)),
				FText::AsNumber(FMath::RoundToInt(Stats.InKBps)), FText::AsNumber(FMath::RoundToInt(Stats.OutKBps)))
			: FText::Format(LOCTEXT("NotConnected", "{0}  |  not connected"), FText::FromString(EchoOnline::GetServiceName(this))));
		StatsBack->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		StatsBack->SetVisibility(ESlateVisibility::Collapsed);
	}

	const UEchoVoiceSubsystem* Voice = GameInstance ? GameInstance->GetSubsystem<UEchoVoiceSubsystem>() : nullptr;
	const bool bOtherTalking = Voice && Voice->IsOtherPlayerTalking();
	if (bOtherTalking)
	{
		const FString Name = GetOtherPlayerName();
		SpeakingText->SetText(FText::Format(LOCTEXT("Speaking", "((( {0} is speaking"), FText::FromString(Name.IsEmpty() ? TEXT("Your partner") : Name)));
	}
	SpeakingBack->SetVisibility(bOtherTalking ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	MicBack->SetVisibility(Voice && Voice->IsPushToTalkActive() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

FString UEchoOnlineOverlay::GetOtherPlayerName() const
{
	const APlayerController* PC = GetOwningPlayer();
	const AGameStateBase* GameState = PC && PC->GetWorld() ? PC->GetWorld()->GetGameState() : nullptr;
	if (GameState)
	{
		for (const APlayerState* Player : GameState->PlayerArray)
		{
			if (Player && Player != PC->PlayerState)
			{
				return Player->GetPlayerName();
			}
		}
	}
	return FString();
}

FString UEchoOnlineOverlay::GetSpeakingText() const
{
	return SpeakingBack && SpeakingBack->GetVisibility() != ESlateVisibility::Collapsed ? SpeakingText->GetText().ToString() : FString();
}

FString UEchoOnlineOverlay::GetStatsText() const
{
	return StatsBack && StatsBack->GetVisibility() != ESlateVisibility::Collapsed ? StatsText->GetText().ToString() : FString();
}

#undef LOCTEXT_NAMESPACE
