// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchoInstancedProps.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 *  Many copies of one mesh with one material, drawn as instances and never colliding with anything. The era levels
 *  are made of these: the visible canyon (copies of the shared layer's hidden collision shapes, instance for
 *  instance), grass, river pieces and other repeated props. Set Mesh, Material and Instances (world transforms, since
 *  the actor sits at the origin); OnConstruction builds the instances. Scripts/build_eras.py fills them.
 */
UCLASS()
class AEchoInstancedProps : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> Props;

public:

	AEchoInstancedProps();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Props")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Props")
	TObjectPtr<UMaterialInterface> Material;

	/** One transform per copy, relative to this actor (which the builder leaves at the world origin) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Props")
	TArray<FTransform> Instances;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Props")
	bool bCastShadow = true;

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintPure, Category="Props")
	int32 GetNumInstances() const;
};
