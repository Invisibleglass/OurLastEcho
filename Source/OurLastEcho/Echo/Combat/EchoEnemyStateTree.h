// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"
#include "EchoEnemyStateTree.generated.h"

class AEchoEnemy;

/**
 *  StateTree nodes for the enemy brain (Milestone 8). ST_EchoEnemy (built by UEchoEditorLibrary::BuildEnemyStateTree)
 *  is: Root > Stunned [Is Stunned] | Attack [Can Attack] | Chase [Has Target] | Idle, each going back to Root when
 *  its task finishes, so the first state whose condition holds runs next. The tasks are thin: the decisions about
 *  how to chase, attack and recover live in AEchoEnemy (and its subclasses), so every enemy type shares this tree.
 *  The enemy is the StateTree owner's (the AI controller's) pawn.
 */

UENUM()
enum class EEchoEnemyCheck : uint8
{
	HasTarget,
	CanAttack,
	IsStunned,
	IsDefeated
};

USTRUCT()
struct FEchoEnemyConditionInstanceData
{
	GENERATED_BODY()
};

/** Checks the enemy's state (target, attack ready, stunned, defeated) */
USTRUCT(meta = (DisplayName = "Echo Enemy Check", Category = "Echo"))
struct FEchoEnemyCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FEchoEnemyConditionInstanceData;

	FEchoEnemyCondition() = default;
	explicit FEchoEnemyCondition(EEchoEnemyCheck InCheck, bool bInInvert = false) : Check(InCheck), bInvertCheck(bInInvert) {}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Echo")
	EEchoEnemyCheck Check = EEchoEnemyCheck::HasTarget;

	UPROPERTY(EditAnywhere, Category = "Echo")
	bool bInvertCheck = false;
};

USTRUCT()
struct FEchoEnemyTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY()
	float Elapsed = 0.0f;
};

/** What an enemy task does each tick */
UENUM()
enum class EEchoEnemyTaskKind : uint8
{
	/** Follow the target until in attack range (succeeds), or lose it (fails) */
	Chase,
	/** Wind up, strike, recover (succeeds when done; fails if stunned or the target's gone) */
	Attack,
	/** Wait out a stun */
	Stunned,
	/** No target: stand guard (succeeds after a moment, or as soon as there's a target) */
	Idle
};

/** One enemy behaviour (chase, attack, stunned, idle), carried out by the enemy's own functions */
USTRUCT(meta = (DisplayName = "Echo Enemy Task", Category = "Echo"))
struct FEchoEnemyTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FEchoEnemyTaskInstanceData;

	FEchoEnemyTask() = default;
	explicit FEchoEnemyTask(EEchoEnemyTaskKind InKind) : Kind(InKind) {}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	UPROPERTY(EditAnywhere, Category = "Echo")
	EEchoEnemyTaskKind Kind = EEchoEnemyTaskKind::Idle;

	/** Idle: seconds before looking again */
	UPROPERTY(EditAnywhere, Category = "Echo")
	float IdleRecheck = 0.3f;
};

namespace EchoEnemyStateTree
{
	/** The enemy this StateTree runs for (the owning AI controller's pawn) */
	AEchoEnemy* GetEnemy(FStateTreeExecutionContext& Context);
}
