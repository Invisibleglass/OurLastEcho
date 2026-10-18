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
#include "EngineUtils.h"
#include "EchoArenaManager.h"
#include "EchoEnemy.h"
#include "EchoHealthComponent.h"
#include "EchoPlayerCombatComponent.h"

void AEchoHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	DrawHurtFlash();
	DrawReticle();
	DrawWhipTarget();
	DrawEnemyBars();
	DrawPlayerHealth();
	DrawCharge();
	DrawArena();

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
	DrawPausedBanner();

	if (GameState->IsDebugShowAllPlatforms())
	{
		DrawText(TEXT("DEBUG: showing all platforms (EchoShowAllPlatforms to turn off)"), FLinearColor(1.0f, 0.8f, 0.3f), 20.0f, 20.0f, GEngine->GetSmallFont(), 1.2f);
	}
	if (GameState->IsDebugShowAllEnemies())
	{
		DrawText(TEXT("DEBUG: showing all enemies to both players (EchoShowAllEnemies to turn off)"), FLinearColor(1.0f, 0.5f, 0.4f), 20.0f, 60.0f, GEngine->GetSmallFont(), 1.2f);
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

void AEchoHUD::DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& Fill, const FLinearColor& Back)
{
	DrawRect(Back, X - 2.0f, Y - 2.0f, Width + 4.0f, Height + 4.0f);
	DrawRect(Fill, X, Y, Width * FMath::Clamp(Fraction, 0.0f, 1.0f), Height);
}

void AEchoHUD::DrawHurtFlash()
{
	const AOurLastEchoCharacter* Character = Cast<AOurLastEchoCharacter>(GetOwningPawn());
	const UEchoPlayerCombatComponent* Combat = Character ? Character->GetPlayerCombat() : nullptr;
	if (!Combat)
	{
		return;
	}
	const float Since = GetWorld()->GetTimeSeconds() - Combat->GetLastHurtTime();
	if (Since >= 0.0f && Since < 0.45f)
	{
		// Red edges, fading
		const float Alpha = 0.45f * (1.0f - Since / 0.45f);
		const FLinearColor Red(0.8f, 0.0f, 0.0f, Alpha);
		const float Edge = Canvas->ClipY * 0.08f;
		DrawRect(Red, 0.0f, 0.0f, Canvas->ClipX, Edge);
		DrawRect(Red, 0.0f, Canvas->ClipY - Edge, Canvas->ClipX, Edge);
		DrawRect(Red, 0.0f, Edge, Edge, Canvas->ClipY - 2.0f * Edge);
		DrawRect(Red, Canvas->ClipX - Edge, Edge, Edge, Canvas->ClipY - 2.0f * Edge);
	}
}

void AEchoHUD::DrawPlayerHealth()
{
	const AOurLastEchoCharacter* Character = Cast<AOurLastEchoCharacter>(GetOwningPawn());
	const UEchoHealthComponent* Health = Character ? Character->GetHealth() : nullptr;
	if (!Health)
	{
		return;
	}
	UFont* Font = GEngine->GetSmallFont();
	const bool bBat = Character->GetRealm() == EEchoRealm::Living;
	const FLinearColor Own = bBat ? FLinearColor(1.0f, 0.7f, 0.25f) : FLinearColor(0.4f, 0.75f, 1.0f);

	// Yours: bottom-left
	const float X = 30.0f;
	const float Y = Canvas->ClipY - 70.0f;
	DrawText(FString::Printf(TEXT("%s  %d / %d%s"), bBat ? TEXT("Bat") : TEXT("Saraa"), FMath::CeilToInt(Health->GetHealth()), FMath::RoundToInt(Health->MaxHealth),
		Health->IsGodMode() ? TEXT("   GOD MODE") : TEXT("")), FLinearColor::White, X, Y - 22.0f, Font, 1.2f);
	DrawBar(X, Y, 300.0f, 16.0f, Health->GetHealthFraction(), Health->GetHealthFraction() > 0.3f ? Own : FLinearColor(0.9f, 0.15f, 0.1f));

	// Your partner's, smaller, under it
	for (TActorIterator<AOurLastEchoCharacter> It(GetWorld()); It; ++It)
	{
		const AOurLastEchoCharacter* Partner = *It;
		if (Partner == Character || !Partner->GetHealth())
		{
			continue;
		}
		const bool bPartnerBat = Partner->GetRealm() == EEchoRealm::Living;
		DrawText(FString::Printf(TEXT("%s%s"), bPartnerBat ? TEXT("Bat") : TEXT("Saraa"), Partner->IsDowned() ? TEXT("  (down)") : TEXT("")),
			FLinearColor(0.85f, 0.85f, 0.85f), X, Y + 26.0f, Font, 1.0f);
		DrawBar(X + 120.0f, Y + 30.0f, 150.0f, 8.0f, Partner->GetHealth()->GetHealthFraction(), bPartnerBat ? FLinearColor(1.0f, 0.7f, 0.25f) : FLinearColor(0.4f, 0.75f, 1.0f));
	}

	// Down: a countdown in the middle of the screen
	if (Character->IsDowned())
	{
		const float Remaining = Character->GetPlayerCombat()->GetRespawnRemaining();
		const FString Message = Remaining > 0.05f ? FString::Printf(TEXT("You're down - back beside your partner in %.0f"), FMath::CeilToFloat(Remaining)) : FString(TEXT("You're down"));
		UFont* Large = GEngine->GetLargeFont();
		float W = 0.0f;
		float H = 0.0f;
		GetTextSize(Message, W, H, Large, 1.6f);
		DrawRect(FLinearColor(0.15f, 0.0f, 0.0f, 0.45f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
		DrawText(Message, FLinearColor::White, (Canvas->ClipX - W) * 0.5f, Canvas->ClipY * 0.4f, Large, 1.6f);
	}
}

void AEchoHUD::DrawEnemyBars()
{
	LastBarEnemies.Reset();
	for (TActorIterator<AEchoEnemy> It(GetWorld()); It; ++It)
	{
		const AEchoEnemy* Enemy = *It;
		// Only enemies this player can see (in their world, or the show-all debug view)
		if (!Enemy->IsShownLocally() || Enemy->IsDefeated() || !Enemy->GetHealth())
		{
			continue;
		}
		const FVector Screen = Project(Enemy->GetHealthBarLocation(), true);
		if (Screen.Z <= 0.0f || Screen.X < 0.0f || Screen.X > Canvas->ClipX || Screen.Y < 0.0f || Screen.Y > Canvas->ClipY)
		{
			continue;
		}
		LastBarEnemies.Add(const_cast<AEchoEnemy*>(Enemy));
		const float Width = 90.0f;
		const float X = Screen.X - Width * 0.5f;
		DrawBar(X, Screen.Y, Width, 7.0f, Enemy->GetHealth()->GetHealthFraction(), FLinearColor(0.85f, 0.15f, 0.1f));
		DrawText(Enemy->DisplayName.ToString(), FLinearColor(0.9f, 0.9f, 0.9f, 0.9f), X, Screen.Y - 16.0f, GEngine->GetSmallFont(), 0.9f);

		// Hit-first enemies: "OPEN" and a timer that runs down while it's open for the other player
		if (Enemy->IsOpen())
		{
			DrawBar(X, Screen.Y + 11.0f, Width, 5.0f, Enemy->GetOpenFraction(), FLinearColor(1.0f, 0.85f, 0.2f));
			DrawText(TEXT("OPEN"), FLinearColor(1.0f, 0.85f, 0.2f), X + Width + 6.0f, Screen.Y + 2.0f, GEngine->GetSmallFont(), 1.0f);
		}
	}
}

bool AEchoHUD::IsEnemyBarShown(AActor* Enemy) const
{
	return LastBarEnemies.Contains(Enemy);
}

void AEchoHUD::DrawArena()
{
	const AEchoArenaManager* Arena = AEchoArenaManager::Get(GetWorld());
	if (!Arena)
	{
		return;
	}
	UFont* Font = GEngine->GetLargeFont();
	const FString Status = Arena->GetStatusText().ToString();
	const FString Hint = Arena->GetHintText().ToString();
	float W = 0.0f;
	float H = 0.0f;
	if (!Status.IsEmpty())
	{
		GetTextSize(Status, W, H, Font, 1.1f);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f), (Canvas->ClipX - W) * 0.5f - 14.0f, 14.0f, W + 28.0f, H + 10.0f);
		DrawText(Status, FLinearColor::White, (Canvas->ClipX - W) * 0.5f, 19.0f, Font, 1.1f);
	}
	if (!Hint.IsEmpty())
	{
		UFont* Small = GEngine->GetSmallFont();
		GetTextSize(Hint, W, H, Small, 1.2f);
		DrawText(Hint, FLinearColor(1.0f, 0.9f, 0.6f), (Canvas->ClipX - W) * 0.5f, 60.0f, Small, 1.2f);
	}
}

void AEchoHUD::DrawCharge()
{
	const AOurLastEchoCharacter* Character = Cast<AOurLastEchoCharacter>(GetOwningPawn());
	if (!Character)
	{
		return;
	}
	float Charge = 0.0f;
	if (const UEchoSpiritBowComponent* Bow = Character->GetSpiritBow(); Bow && Bow->IsCharging())
	{
		Charge = Bow->GetChargeFraction();
	}
	else if (const UEchoSwordWhipComponent* Whip = Character->GetSwordWhip(); Whip && Whip->IsChargingLash())
	{
		Charge = Whip->GetLashChargeFraction();
	}
	if (Charge <= 0.0f)
	{
		// Bat: a small dot where quick shots go
		if (Character->GetRealm() == EEchoRealm::Living && !Character->GetSpiritBow()->IsAiming())
		{
			DrawRect(FLinearColor(1.0f, 0.85f, 0.5f, 0.6f), Canvas->ClipX * 0.5f - 2.0f, Canvas->ClipY * 0.5f - 2.0f, 4.0f, 4.0f);
		}
		return;
	}
	// Under the crosshair; full = white-hot
	const float Width = 120.0f;
	const FLinearColor Fill = Charge >= 1.0f ? FLinearColor(1.0f, 1.0f, 0.9f) : FLinearColor(1.0f, 0.65f, 0.2f);
	DrawBar(Canvas->ClipX * 0.5f - Width * 0.5f, Canvas->ClipY * 0.5f + 40.0f, Width, 6.0f, Charge, Fill);
}

void AEchoHUD::DrawPausedBanner()
{
	const AEchoGameState* GameState = GetWorld()->GetGameState<AEchoGameState>();
	const AOurLastEchoPlayerController* PC = Cast<AOurLastEchoPlayerController>(PlayerOwner);
	// Only for a player who isn't in the menu themselves (their own menu says it's paused)
	if (!GameState || !GetWorld()->IsPaused() || (PC && PC->IsPauseMenuOpen()))
	{
		return;
	}

	// "Bat" / "Saraa" from the character of each player with the menu open
	TArray<FString> Names;
	for (const APlayerState* Player : GameState->GetPlayersInPauseMenu())
	{
		const AOurLastEchoCharacter* Character = Player ? Cast<AOurLastEchoCharacter>(Player->GetPawn()) : nullptr;
		Names.Add(Character ? (Character->GetRealm() == EEchoRealm::Living ? TEXT("Bat") : TEXT("Saraa")) : TEXT("The other player"));
	}
	const FString Who = Names.Num() > 0 ? FString::Join(Names, TEXT(" and ")) : TEXT("The other player");
	const FString Message = FString::Printf(TEXT("Paused - %s paused the game"), *Who);

	UFont* Font = GEngine->GetLargeFont();
	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	GetTextSize(Message, TextWidth, TextHeight, Font, 1.5f);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	DrawText(Message, FLinearColor::White, (Canvas->ClipX - TextWidth) * 0.5f, (Canvas->ClipY - TextHeight) * 0.5f, Font, 1.5f);
}
