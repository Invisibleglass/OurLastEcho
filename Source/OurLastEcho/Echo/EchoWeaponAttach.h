// Our Last Echo

#pragma once

#include "CoreMinimal.h"

class ACharacter;
class USceneComponent;

/**
 *  Puts a weapon (the spirit bow, the sword) on a bone of the character's skeleton, so it moves with the animation:
 *  the bow on Bat's back sways with his chest, the sword on Saraa's hip with her hips.
 *
 *  The placement is still given relative to the character (the capsule centre), as the weapons always were, and is
 *  converted to the bone's space using the skeleton's reference pose. So a weapon looks where it did before when the
 *  character stands still, and nobody has to tune bone-space offsets by hand.
 */
namespace EchoWeaponAttach
{
	/**
	 *  Attaches Root to Bone on Character's mesh, at ActorPlacement (relative to the actor) in the reference pose.
	 *  With no such bone (or Bone = None) it attaches to the capsule at ActorPlacement, as before; returns false then.
	 */
	bool AttachToBone(USceneComponent* Root, ACharacter* Character, FName Bone, const FTransform& ActorPlacement);
}
