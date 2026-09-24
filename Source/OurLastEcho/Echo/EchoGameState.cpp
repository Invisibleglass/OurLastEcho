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

void AEchoGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEchoGameState, bMilestoneComplete);
}
