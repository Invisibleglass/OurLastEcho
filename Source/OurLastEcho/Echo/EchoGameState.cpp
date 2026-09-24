// Our Last Echo

#include "EchoGameState.h"
#include "Net/UnrealNetwork.h"
#include "OurLastEcho.h"

void AEchoGameState::SetMilestoneComplete()
{
	if (!HasAuthority() || bMilestoneComplete)
	{
		return;
	}

	UE_LOG(LogOurLastEcho, Log, TEXT("Milestone complete"));
	bMilestoneComplete = true;
}

void AEchoGameState::SetPlayersInSettingsMenu(const TArray<APlayerState*>& Players)
{
	if (HasAuthority())
	{
		PlayersInSettingsMenu = TArray<TObjectPtr<APlayerState>>(Players);
		ForceNetUpdate();
	}
}

void AEchoGameState::SetDebugShowAllPlatforms(bool bShow)
{
	if (HasAuthority() && bDebugShowAllPlatforms != bShow)
	{
		UE_LOG(LogOurLastEcho, Log, TEXT("Debug: show all platforms %s"), bShow ? TEXT("ON") : TEXT("OFF"));
		bDebugShowAllPlatforms = bShow;
		ForceNetUpdate();
	}
}

void AEchoGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEchoGameState, bMilestoneComplete);
	DOREPLIFETIME(AEchoGameState, bDebugShowAllPlatforms);
	DOREPLIFETIME(AEchoGameState, PlayersInSettingsMenu);
}
