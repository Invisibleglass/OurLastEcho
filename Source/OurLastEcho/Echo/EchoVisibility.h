// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "EchoTypes.h"

class UWorld;

/**
 *  Helpers for realm-only visibility, shared by AEchoSpiritPlatform and AEchoPlatform.
 *  Visibility is always decided on each machine from ITS local player, never replicated.
 */
namespace EchoVisibility
{
	/** The realm of the character the local player controls. False if there's no local character (e.g. dedicated server) */
	bool GetLocalViewerRealm(const UWorld* World, EEchoRealm& OutRealm);

	/**
	 *  The era this machine shows: the era director's (which follows the local player's role, or a debug override),
	 *  else the local character's own. False if neither is known yet
	 */
	bool GetLocalViewerEra(const UWorld* World, EEchoEra& OutEra);

	/** True while the EchoShowAllPlatforms debug command is on (replicated through AEchoGameState) */
	bool IsDebugShowAll(const UWorld* World);
}
