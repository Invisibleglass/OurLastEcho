// Our Last Echo

#include "EchoWeaponAttach.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "GameFramework/Character.h"

bool EchoWeaponAttach::AttachToBone(USceneComponent* Root, ACharacter* Character, FName Bone, const FTransform& ActorPlacement)
{
	if (!Root || !Character)
	{
		return false;
	}

	USkeletalMeshComponent* Mesh = Character->GetMesh();
	const USkinnedAsset* Asset = Mesh ? Mesh->GetSkinnedAsset() : nullptr;
	const int32 BoneIndex = Asset && !Bone.IsNone() ? Asset->GetRefSkeleton().FindBoneIndex(Bone) : INDEX_NONE;
	if (BoneIndex == INDEX_NONE)
	{
		if (Root->GetAttachParent() != Character->GetRootComponent() || Root->GetAttachSocketName() != NAME_None)
		{
			Root->AttachToComponent(Character->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		}
		Root->SetRelativeTransform(ActorPlacement);
		return false;
	}

	// Actor space -> the mesh component's space -> the bone's space, in the reference pose. The mesh's own offset comes
	// from the class defaults: the live one is tilted for a moment by flinches, rolls and the downed pose
	const ACharacter* Defaults = Character->GetClass()->GetDefaultObject<ACharacter>();
	const FTransform MeshOffset = Defaults && Defaults->GetMesh() ? Defaults->GetMesh()->GetRelativeTransform() : Mesh->GetRelativeTransform();
	const FTransform PlacementInMesh = ActorPlacement.GetRelativeTransform(MeshOffset);
	const FTransform BoneInMesh = FAnimationRuntime::GetComponentSpaceTransformRefPose(Asset->GetRefSkeleton(), BoneIndex);
	const FTransform PlacementInBone = PlacementInMesh.GetRelativeTransform(BoneInMesh);

	if (Root->GetAttachParent() != Mesh || Root->GetAttachSocketName() != Bone)
	{
		Root->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform, Bone);
	}
	Root->SetRelativeTransform(PlacementInBone);
	return true;
}
