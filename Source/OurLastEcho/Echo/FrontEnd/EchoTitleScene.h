// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "CineCameraActor.h"
#include "GameFramework/Actor.h"
#include "EchoTitleScene.generated.h"

class UAnimSequenceBase;
class UPoseableMeshComponent;
class USkeletalMesh;
class USkeletalMeshComponent;

/**
 *  The title scene's cinematic camera: a cine camera that drifts slowly around where it was placed, with a
 *  gentle sway, like a hand-held shot on a still evening. The front-end player controller looks through it.
 */
UCLASS()
class AEchoTitleCamera : public ACineCameraActor
{
	GENERATED_BODY()

public:

	AEchoTitleCamera(const FObjectInitializer& ObjectInitializer);

	/** How far the camera drifts from its placed position on each axis, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Title Camera")
	FVector DriftAmplitude = FVector(45.0f, 30.0f, 14.0f);

	/** Seconds for one full drift on each axis (different periods keep it from looking like a loop) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Title Camera")
	FVector DriftPeriod = FVector(29.0f, 37.0f, 23.0f);

	/** Gentle turn (yaw) and nod (pitch) in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Title Camera")
	FVector2D SwayDegrees = FVector2D(1.6f, 0.7f);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Current offset from the placed position (for tests) */
	UFUNCTION(BlueprintPure, Category="Title Camera")
	FVector GetDriftOffset() const { return GetActorLocation() - BaseLocation; }

protected:

	FVector BaseLocation = FVector::ZeroVector;
	FRotator BaseRotation = FRotator::ZeroRotator;
	float Time = 0.0f;
};

/** One bone's turn for the seated pose, applied in component space after its parents */
USTRUCT(BlueprintType)
struct FEchoPoseBone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pose")
	FName Bone;

	/** Pitch turns about Y (left/right), Yaw about Z (up), Roll about X; the mannequin faces +Y, and a negative Roll
	 *  swings a leg forward (Unreal rotators turn the opposite way to the right-hand rule) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pose")
	FRotator Rotation = FRotator::ZeroRotator;
};

/**
 *  Bat on the title screen, sitting on a rock.
 *
 *  The seated pose is made procedurally on a poseable mesh from SeatedPose: a list of bone turns you can edit in
 *  the Details panel (no animation asset needed).
 *  Future hook: set LoopAnimation to any animation for the mannequin skeleton (e.g. "picking leaves") and it plays
 *  looping instead of the static pose; nothing else needs to change.
 */
UCLASS()
class AEchoTitleBat : public AActor
{
	GENERATED_BODY()

public:

	AEchoTitleBat();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Title Bat")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/** If set, plays looping instead of the static seated pose */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Title Bat")
	TSoftObjectPtr<UAnimSequenceBase> LoopAnimation;

	/** The seated pose: bone turns, parents before children */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Title Bat")
	TArray<FEchoPoseBone> SeatedPose;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** Is the loop animation playing (rather than the static pose)? */
	UFUNCTION(BlueprintPure, Category="Title Bat")
	bool IsPlayingLoopAnimation() const;

	/** World location of a bone in the current pose (for tests and placement) */
	UFUNCTION(BlueprintPure, Category="Title Bat")
	FVector GetBoneLocation(FName Bone) const;

protected:

	void ApplyPose();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UPoseableMeshComponent> PosedMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USkeletalMeshComponent> AnimatedMesh;
};
