// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EchoEnemyController.generated.h"

class AEchoEnemy;
class UAISenseConfig_Sight;
class UStateTree;
class UStateTreeAIComponent;

/**
 *  The brain of every enemy (server only), Milestone 8:
 *  - AI Perception (sight) finds players; only players the enemy may target count (EchoEnemy::CanTarget: in its
 *    world and standing). Arena enemies, and anyone within half the sight radius, are noticed without being seen,
 *    so wave fights never stall behind a pillar.
 *  - A StateTree (ST_EchoEnemy, built by Scripts/build_combat.py through UEchoEditorLibrary) picks what to do:
 *    stunned, attack, chase or idle. Its tasks (EchoEnemyStateTree.h) call the enemy's ChaseTick/AttackTick/etc.
 *  - Movement: path-following MoveTo on the level's navmesh, or straight at the target where there's no path
 *    (flying enemies steer themselves).
 */
UCLASS()
class AEchoEnemyController : public AAIController
{
	GENERATED_BODY()

public:

	AEchoEnemyController();

	/** The enemy decision tree */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Brain")
	TSoftObjectPtr<UStateTree> StateTree;

	/** Seconds between target choices */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Brain", meta = (ClampMin = 0.05))
	float TargetInterval = 0.25f;

	/** Move towards an actor, re-pathing now and then; falls back to steering straight at it */
	void ChaseActor(AActor* Goal, float AcceptanceRadius);

	/** Stops the StateTree and all movement (defeated) */
	void StopBrain();

	/** The StateTree is running (for tests) */
	UFUNCTION(BlueprintPure, Category="Enemy|Brain")
	bool IsBrainRunning() const;

	/** Also stops steering straight at a chase goal */
	virtual void StopMovement() override;

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void OnPossess(APawn* InPawn) override;

	void ChooseTarget();

	AEchoEnemy* GetEnemy() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStateTreeAIComponent> Brain;

	UPROPERTY(Transient)
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	float TargetTimer = 0.0f;
	float RepathTimer = 0.0f;
	TWeakObjectPtr<AActor> ChaseGoal;
	bool bDirectChase = false;
};
