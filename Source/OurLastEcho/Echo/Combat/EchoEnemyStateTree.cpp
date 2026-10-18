// Our Last Echo

#include "EchoEnemyStateTree.h"
#include "AIController.h"
#include "StateTreeExecutionContext.h"
#include "EchoEnemy.h"

AEchoEnemy* EchoEnemyStateTree::GetEnemy(FStateTreeExecutionContext& Context)
{
	UObject* Owner = Context.GetOwner();
	if (const AController* Controller = Cast<AController>(Owner))
	{
		return Cast<AEchoEnemy>(Controller->GetPawn());
	}
	if (const UActorComponent* Component = Cast<UActorComponent>(Owner))
	{
		if (const AController* ComponentController = Cast<AController>(Component->GetOwner()))
		{
			return Cast<AEchoEnemy>(ComponentController->GetPawn());
		}
	}
	return Cast<AEchoEnemy>(Owner);
}

bool FEchoEnemyCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const AEchoEnemy* Enemy = EchoEnemyStateTree::GetEnemy(Context);
	bool bResult = false;
	if (Enemy)
	{
		switch (Check)
		{
		case EEchoEnemyCheck::HasTarget: bResult = Enemy->GetTarget() != nullptr; break;
		case EEchoEnemyCheck::CanAttack: bResult = Enemy->CanStartAttack(); break;
		case EEchoEnemyCheck::IsStunned: bResult = Enemy->IsStunned(); break;
		case EEchoEnemyCheck::IsDefeated: bResult = Enemy->IsDefeated(); break;
		}
	}
	return bResult != bInvertCheck;
}

EStateTreeRunStatus FEchoEnemyTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AEchoEnemy* Enemy = EchoEnemyStateTree::GetEnemy(Context);
	if (!Enemy || Enemy->IsDefeated())
	{
		return EStateTreeRunStatus::Failed;
	}
	Context.GetInstanceData(*this).Elapsed = 0.0f;
	Enemy->SetBrainState(Kind == EEchoEnemyTaskKind::Chase ? FName(TEXT("Chase")) : Kind == EEchoEnemyTaskKind::Attack ? FName(TEXT("Attack"))
		: Kind == EEchoEnemyTaskKind::Stunned ? FName(TEXT("Stunned")) : FName(TEXT("Idle")));
	if (Kind == EEchoEnemyTaskKind::Attack)
	{
		if (!Enemy->CanStartAttack())
		{
			return EStateTreeRunStatus::Failed;
		}
		Enemy->StartAttack();
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FEchoEnemyTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	AEchoEnemy* Enemy = EchoEnemyStateTree::GetEnemy(Context);
	if (!Enemy || Enemy->IsDefeated())
	{
		return EStateTreeRunStatus::Failed;
	}
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.Elapsed += DeltaTime;

	// Anything but waiting out the stun gives way to a stun at once
	if (Kind != EEchoEnemyTaskKind::Stunned && Enemy->IsStunned())
	{
		return EStateTreeRunStatus::Failed;
	}

	switch (Kind)
	{
	case EEchoEnemyTaskKind::Chase:
		if (!Enemy->GetTarget())
		{
			return EStateTreeRunStatus::Failed;
		}
		return Enemy->ChaseTick(DeltaTime) ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;

	case EEchoEnemyTaskKind::Attack:
		return Enemy->AttackTick(DeltaTime) ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;

	case EEchoEnemyTaskKind::Stunned:
		return Enemy->StunnedTick(DeltaTime) ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;

	default:
		Enemy->IdleTick(DeltaTime);
		return Enemy->GetTarget() || Data.Elapsed >= IdleRecheck ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
	}
}

void FEchoEnemyTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// Leaving an attack part-way (stunned, target lost): stop it cleanly
	if (Kind == EEchoEnemyTaskKind::Attack)
	{
		if (AEchoEnemy* Enemy = EchoEnemyStateTree::GetEnemy(Context))
		{
			if (Enemy->GetAttackPhase() != EEchoAttackPhase::None)
			{
				Enemy->CancelAttack();
			}
		}
	}
}
