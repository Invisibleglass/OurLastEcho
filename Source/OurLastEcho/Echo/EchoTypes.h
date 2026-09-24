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
