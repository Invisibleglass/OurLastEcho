// Our Last Echo

#include "EchoHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EchoGameState.h"
#include "EchoAnchorPoint.h"
#include "EchoGameUserSettings.h"
#include "EchoSpiritBowComponent.h"
#include "EchoSwordWhipComponent.h"
#include "OurLastEchoCharacter.h"
#include "OurLastEchoPlayerController.h"
#include "GameFramework/PlayerState.h"

void AEchoHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	DrawReticle();
	DrawWhipTarget();

	if (UEchoSwordWhipComponent::IsDebugDrawOn())
	{
		DrawText(TEXT("DEBUG: whip range, anchors and swing arcs (EchoWhipDebug to turn off)"), FLinearColor(0.5f, 0.8f, 1.0f), 20.0f, 40.0f, GEngine->GetSmallFont(), 1.2f);
	}

	const AEchoGameState* GameState = GetWorld()->GetGameState<AEchoGameState>();
	if (!GameState)
	{
		return;
	}

	DrawSubtitle();

	if (GameState->IsDebugShowAllPlatforms())
	{
		DrawText(TEXT("DEBUG: showing all platforms (EchoShowAllPlatforms to turn off)"), FLinearColor(1.0f, 0.8f, 0.3f), 20.0f, 20.0f, GEngine->GetSmallFont(), 1.2f);
	}

	if (!GameState->IsMilestoneComplete())
	{
		return;
	}

	UFont* Font = GEngine->GetLargeFont();
	const FString Message = CompleteMessage.ToString();

	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	GetTextSize(Message, TextWidth, TextHeight, Font, TextScale);

	const float X = (Canvas->ClipX - TextWidth) * 0.5f;
	const float Y = Canvas->ClipY * 0.3f - TextHeight * 0.5f;

	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), X - 40.0f, Y - 20.0f, TextWidth + 80.0f, TextHeight + 40.0f);
	DrawText(Message, FLinearColor::White, X, Y, Font, TextScale);
}

void AEchoHUD::DrawWhipTarget()
{
	const AOurLastEchoCharacter* Character = Cast<AOurLastEchoCharacter>(GetOwningPawn());
	const UEchoSwordWhipComponent* Whip = Character ? Character->GetSwordWhip() : nullptr;
	const AEchoAnchorPoint* Target = Whip ? Whip->GetHighlightedAnchor() : nullptr;
	if (!Target || Whip->IsSwinging())
	{
		return;
	}

	// Four blue corner brackets around the anchor the whip would latch onto
	const FVector Screen = Project(Target->GetSwingPoint(), true);
	if (Screen.Z <= 0.0f)
	{
		return;
	}
	const float Half = WhipMarkerSize;
	const float Arm = WhipMarkerSize * 0.45f;
	for (const FVector2D Corner : { FVector2D(-1, -1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(1, 1) })
	{
		const float X = Screen.X + Corner.X * Half;
		const float Y = Screen.Y + Corner.Y * Half;
		DrawLine(X, Y, X - Corner.X * Arm, Y, WhipMarkerColor, 2.5f);
		DrawLine(X, Y, X, Y - Corner.Y * Arm, WhipMarkerColor, 2.5f);
	}
}

void AEchoHUD::DrawReticle()
{
	const AOurLastEchoCharacter* Character = Cast<AOurLastEchoCharacter>(GetOwningPawn());
	const UEchoSpiritBowComponent* Bow = Character ? Character->GetSpiritBow() : nullptr;
	if (!Bow || !Bow->IsAiming())
	{
		return;
	}

	// Gold cross with a gap; it dims and opens up while the bow is cooling down
	const float Ready = Bow->GetCooldownReadiness();
	const FLinearColor Color = FLinearColor::LerpUsingHSV(FLinearColor(0.5f, 0.4f, 0.3f, 0.5f), ReticleColor, Ready);
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const float Gap = ReticleGap + (1.0f - Ready) * 8.0f;
	const float Len = ReticleSize;

	DrawLine(CX - Gap - Len, CY, CX - Gap, CY, Color, 2.0f);
	DrawLine(CX + Gap, CY, CX + Gap + Len, CY, Color, 2.0f);
	DrawLine(CX, CY - Gap - Len, CX, CY - Gap, Color, 2.0f);
	DrawLine(CX, CY + Gap, CX, CY + Gap + Len, Color, 2.0f);
	DrawRect(Color, CX - 1.5f, CY - 1.5f, 3.0f, 3.0f);
}

void AEchoHUD::ShowSubtitle(const FText& Text, float Seconds)
{
	SubtitleText = Text;
	SubtitleUntil = GetWorld()->GetRealTimeSeconds() + Seconds;
}

FString AEchoHUD::GetVisibleSubtitle(float& OutScale) const
{
	const UEchoGameUserSettings* Settings = UEchoGameUserSettings::GetEchoSettings();
	OutScale = Settings ? Settings->SubtitleScale : 1.0f;
	const bool bShowing = Settings && Settings->bSubtitlesEnabled && GetWorld()->GetRealTimeSeconds() < SubtitleUntil;
	return bShowing ? SubtitleText.ToString() : FString();
}

void AEchoHUD::DrawSubtitle()
{
	float Scale = 1.0f;
	const FString Line = GetVisibleSubtitle(Scale);
	if (Line.IsEmpty())
	{
		return;
	}

	// Centred near the bottom, on a dark band, sized by the Subtitle size setting
	UFont* Font = GEngine->GetLargeFont();
	const float SubtitleTextScale = 1.1f * Scale;
	float Width = 0.0f;
	float Height = 0.0f;
	GetTextSize(Line, Width, Height, Font, SubtitleTextScale);
	const float X = (Canvas->ClipX - Width) * 0.5f;
	const float Y = Canvas->ClipY * 0.86f - Height;
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), X - 16.0f, Y - 8.0f, Width + 32.0f, Height + 16.0f);
	DrawText(Line, FLinearColor::White, X, Y, Font, SubtitleTextScale);
}
