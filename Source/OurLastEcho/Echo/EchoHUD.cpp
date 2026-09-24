// Our Last Echo

#include "EchoHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EchoGameState.h"

void AEchoHUD::DrawHUD()
{
	Super::DrawHUD();

	const AEchoGameState* GameState = GetWorld()->GetGameState<AEchoGameState>();
	if (!Canvas || !GameState || !GameState->IsMilestoneComplete())
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
