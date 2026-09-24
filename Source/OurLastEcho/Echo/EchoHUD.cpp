// Our Last Echo

#include "EchoHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EchoGameState.h"
#include "EchoSpiritBowComponent.h"
#include "OurLastEchoCharacter.h"

void AEchoHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	DrawReticle();

	const AEchoGameState* GameState = GetWorld()->GetGameState<AEchoGameState>();
	if (!GameState)
	{
		return;
	}

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
