// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EchoTypes.h"
#include "EchoCombatTypes.generated.h"

class AActor;

/** Who's on which side: players never hurt each other, enemies never hurt each other */
UENUM(BlueprintType)
enum class EEchoTeam : uint8
{
	Players,
	Enemies
};

/** Which attack a hit came from (damage, feedback and the hit-first rules look at this) */
UENUM(BlueprintType)
enum class EEchoAttackKind : uint8
{
	/** Bat: a fast shot without aiming */
	QuickShot,
	/** Bat: a shot while aiming */
	AimedShot,
	/** Bat: a shot held and charged while aiming */
	ChargedShot,
	/** Saraa: lashes 1 and 2 of the light combo */
	WhipLight,
	/** Saraa: the light combo's third, wider lash */
	WhipFinisher,
	/** Saraa: the charged lash */
	WhipCharged,
	/** An enemy's attack */
	Enemy,
	/** Debug and tests */
	Debug
};

/** What a hit did. Every hit gets one, and both players see/hear the matching cue */
UENUM(BlueprintType)
enum class EEchoDamageResult : uint8
{
	/** Took damage */
	Applied,
	/** Took the last of its health */
	Defeated,
	/** Did nothing, and says so (a clink and a spark): e.g. an arrow on a Stone Guardian's shell */
	Blocked,
	/** Did no damage but opened it up for the other player (a Stone Guardian's shell cracks, a Sky Talon falls) */
	Opened,
	/** Invulnerable for now (a dodge roll's grace, god mode, already down) */
	Immune,
	/** Can't affect it at all (it isn't in this player's world). No cue */
	Ignored
};

/** One hit, as the server judges it */
USTRUCT(BlueprintType)
struct FEchoDamageInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo|Combat")
	float Amount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo|Combat")
	EEchoAttackKind Kind = EEchoAttackKind::Debug;

	/** Heavy hits (charged attacks, the combo finisher, big enemy blows) get a hit-stop and a bigger reaction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo|Combat")
	bool bHeavy = false;

	/** Who hit (a player character or an enemy) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo|Combat")
	TObjectPtr<AActor> Instigator = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo|Combat")
	FVector HitLocation = FVector::ZeroVector;

	/** Direction the hit travelled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Echo|Combat")
	FVector Direction = FVector::ForwardVector;

	bool IsArrow() const { return Kind == EEchoAttackKind::QuickShot || Kind == EEchoAttackKind::AimedShot || Kind == EEchoAttackKind::ChargedShot; }
	bool IsWhip() const { return Kind == EEchoAttackKind::WhipLight || Kind == EEchoAttackKind::WhipFinisher || Kind == EEchoAttackKind::WhipCharged; }
};

/** Which players' worlds something exists in */
UENUM(BlueprintType)
enum class EEchoPresence : uint8
{
	/** Both players see it and can fight it */
	Both,
	/** Only in Bat's world (the present): only Bat sees, hits or is attacked by it */
	LivingOnly,
	/** Only in Saraa's world (the past) */
	SpiritOnly
};

class USoundBase;

namespace EchoCombat
{
	/** A default sound for a sound hook, from the engine's own UI sounds (greybox: no audio of our own yet),
	 *  e.g. EngineSound(TEXT("VR_open")) = /Engine/VREditor/Sounds/VR_open_Cue */
	USoundBase* EngineSound(const TCHAR* Name);

	/** Is a player of this realm in the worlds of something with this presence? */
	inline bool PresenceIncludes(EEchoPresence Presence, EEchoRealm Realm)
	{
		return Presence == EEchoPresence::Both
			|| (Presence == EEchoPresence::LivingOnly && Realm == EEchoRealm::Living)
			|| (Presence == EEchoPresence::SpiritOnly && Realm == EEchoRealm::Spirit);
	}
}

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UEchoDamageFilter : public UInterface
{
	GENERATED_BODY()
};

/**
 *  Implemented by actors with special rules for incoming hits (enemies: who can affect them, shells, stuns).
 *  UEchoHealthComponent asks its owner before applying any damage (server only).
 */
class IEchoDamageFilter
{
	GENERATED_BODY()

public:

	/** Decide what this hit does. Return Applied to let it through (Amount may be changed), anything else to stop it */
	virtual EEchoDamageResult FilterIncomingDamage(FEchoDamageInfo& Info) = 0;
};
