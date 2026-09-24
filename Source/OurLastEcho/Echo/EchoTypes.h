// Shared types for Our Last Echo gameplay

#pragma once

#include "CoreMinimal.h"
#include "EchoTypes.generated.h"

/** Which side of the veil a character (or object) belongs to */
UENUM(BlueprintType)
enum class EEchoRealm : uint8
{
	/** Present-day canyon (Bat) */
	Living,
	/** The canyon's ancient past / afterlife (Saraa) */
	Spirit
};

/**
 *  Object channel used by Spirit-realm character capsules.
 *  Defined as "SpiritPawn" in Config/DefaultEngine.ini - keep the two in sync.
 *  Spirit platforms block ONLY this channel, so only Spirit characters can stand on them.
 */
#define ECC_SpiritPawn ECC_GameTraceChannel1

/**
 *  Object channel used by spirit bow arrows. Defined as "EchoArrow" in Config/DefaultEngine.ini.
 *  Its default response is Block, so walls, rocks and the landscape stop arrows. Echo platforms have a
 *  hit box that blocks ONLY this channel, so arrows (and the bow's aim trace) can hit a platform that
 *  nobody can stand on yet.
 */
#define ECC_EchoArrow ECC_GameTraceChannel2
