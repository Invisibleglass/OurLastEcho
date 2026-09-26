// Our Last Echo

#include "EchoTitleScene.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

// ------------------------------------------------------------------ camera

AEchoTitleCamera::AEchoTitleCamera(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}

void AEchoTitleCamera::BeginPlay()
{
	Super::BeginPlay();
	BaseLocation = GetActorLocation();
	BaseRotation = GetActorRotation();
}

void AEchoTitleCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Cine cameras also tick in editor viewports: only drift in play, or the placed position would wander off
	if (!HasActorBegunPlay() || !GetWorld() || !GetWorld()->IsGameWorld())
	{
		return;
	}

	Time += DeltaSeconds;
	auto Wave = [this](float Period, float Phase)
	{
		return FMath::Sin((Time / FMath::Max(1.0f, Period) + Phase) * UE_TWO_PI);
	};

	const FVector Offset(DriftAmplitude.X * Wave(DriftPeriod.X, 0.0f), DriftAmplitude.Y * Wave(DriftPeriod.Y, 0.3f), DriftAmplitude.Z * Wave(DriftPeriod.Z, 0.7f));
	SetActorLocation(BaseLocation + BaseRotation.RotateVector(Offset));
	SetActorRotation(BaseRotation + FRotator(SwayDegrees.Y * Wave(DriftPeriod.Z * 1.3f, 0.2f), SwayDegrees.X * Wave(DriftPeriod.X * 1.1f, 0.5f), 0.0f));
}

// ------------------------------------------------------------------ Bat

AEchoTitleBat::AEchoTitleBat()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// The mannequin faces +Y; turn it so the actor's +X is Bat's forward, like the character Blueprint does
	PosedMesh = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("PosedMesh"));
	PosedMesh->SetupAttachment(Root);
	PosedMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	PosedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	AnimatedMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AnimatedMesh"));
	AnimatedMesh->SetupAttachment(Root);
	AnimatedMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	AnimatedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AnimatedMesh->SetVisibility(false);

	Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));

	// Sitting on a rock: thighs forward, shins down, a slight slump, head bowed, hands resting on the knees
	SeatedPose =
	{
		{ TEXT("thigh_l"), FRotator(0.0f, 0.0f, -88.0f) },
		{ TEXT("thigh_r"), FRotator(0.0f, 0.0f, -88.0f) },
		{ TEXT("calf_l"), FRotator(0.0f, 0.0f, 84.0f) },
		{ TEXT("calf_r"), FRotator(0.0f, 0.0f, 84.0f) },
		{ TEXT("foot_l"), FRotator(0.0f, 0.0f, 4.0f) },
		{ TEXT("foot_r"), FRotator(0.0f, 0.0f, 4.0f) },
		{ TEXT("spine_02"), FRotator(0.0f, 0.0f, 10.0f) },
		{ TEXT("spine_04"), FRotator(0.0f, 0.0f, 8.0f) },
		{ TEXT("neck_01"), FRotator(0.0f, 0.0f, 10.0f) },
		{ TEXT("head"), FRotator(0.0f, 0.0f, 12.0f) },
		{ TEXT("upperarm_l"), FRotator(-40.0f, 0.0f, -15.0f) },
		{ TEXT("upperarm_r"), FRotator(40.0f, 0.0f, -15.0f) },
		{ TEXT("lowerarm_l"), FRotator(0.0f, 0.0f, -30.0f) },
		{ TEXT("lowerarm_r"), FRotator(0.0f, 0.0f, -30.0f) },
	};
}

void AEchoTitleBat::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPose();
}

void AEchoTitleBat::BeginPlay()
{
	Super::BeginPlay();

	if (UAnimSequenceBase* Animation = LoopAnimation.LoadSynchronous())
	{
		AnimatedMesh->SetSkeletalMesh(Mesh.LoadSynchronous());
		AnimatedMesh->SetVisibility(true);
		PosedMesh->SetVisibility(false);
		AnimatedMesh->PlayAnimation(Animation, true);
	}
	else
	{
		ApplyPose();
	}
}

bool AEchoTitleBat::IsPlayingLoopAnimation() const
{
	return AnimatedMesh->IsVisible() && AnimatedMesh->IsPlaying();
}

void AEchoTitleBat::ApplyPose()
{
	USkeletalMesh* SkeletalMesh = Mesh.LoadSynchronous();
	if (!SkeletalMesh)
	{
		return;
	}
	if (PosedMesh->GetSkinnedAsset() != SkeletalMesh)
	{
		PosedMesh->SetSkinnedAssetAndUpdate(SkeletalMesh);
	}

	// From the reference pose each time, so editing the list in the Details panel doesn't stack turns
	const int32 NumBones = PosedMesh->GetNumBones();
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		PosedMesh->ResetBoneTransformByName(PosedMesh->GetBoneName(BoneIndex));
	}

	for (const FEchoPoseBone& Entry : SeatedPose)
	{
		if (PosedMesh->GetBoneIndex(Entry.Bone) == INDEX_NONE)
		{
			continue;
		}
		const FTransform Current = PosedMesh->GetBoneTransformByName(Entry.Bone, EBoneSpaces::ComponentSpace);
		const FQuat Turned = FQuat(Entry.Rotation) * Current.GetRotation();
		PosedMesh->SetBoneRotationByName(Entry.Bone, Turned.Rotator(), EBoneSpaces::ComponentSpace);
	}
}

FVector AEchoTitleBat::GetBoneLocation(FName Bone) const
{
	return PosedMesh->GetBoneLocationByName(Bone, EBoneSpaces::WorldSpace);
}
