// Our Last Echo

#include "EchoEnemyController.h"
#include "Components/StateTreeAIComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "StateTree.h"
#include "EchoEnemy.h"
#include "OurLastEcho.h"
#include "OurLastEchoCharacter.h"

AEchoEnemyController::AEchoEnemyController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = false;

	Brain = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("Brain"));
	Brain->SetStartLogicAutomatically(false);

	UAIPerceptionComponent* Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight"));
	SightConfig->SightRadius = 3500.0f;
	SightConfig->LoseSightRadius = 4200.0f;
	SightConfig->PeripheralVisionAngleDegrees = 80.0f;
	SightConfig->SetMaxAge(4.0f);
	// The players aren't on a team the perception system knows about: count everyone (CanTarget filters)
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	Perception->ConfigureSense(*SightConfig);
	Perception->SetDominantSense(UAISense_Sight::StaticClass());
	SetPerceptionComponent(*Perception);

	StateTree = TSoftObjectPtr<UStateTree>(FSoftObjectPath(TEXT("/Game/Echo/Combat/ST_EchoEnemy.ST_EchoEnemy")));
}

AEchoEnemy* AEchoEnemyController::GetEnemy() const
{
	return Cast<AEchoEnemy>(GetPawn());
}

void AEchoEnemyController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (const AEchoEnemy* Enemy = GetEnemy())
	{
		SightConfig->SightRadius = Enemy->SightRadius;
		SightConfig->LoseSightRadius = Enemy->SightRadius * 1.2f;
		GetPerceptionComponent()->ConfigureSense(*SightConfig);
	}

	if (UStateTree* Tree = StateTree.LoadSynchronous())
	{
		Brain->SetStateTree(Tree);
		Brain->StartLogic();
	}
	else
	{
		UE_LOG(LogOurLastEcho, Warning, TEXT("Enemy brain: no StateTree at %s (run Scripts/build_combat.py)"), *StateTree.ToString());
	}
}

void AEchoEnemyController::StopBrain()
{
	Brain->StopLogic(TEXT("Defeated"));
	StopMovement();
	SetActorTickEnabled(false);
}

bool AEchoEnemyController::IsBrainRunning() const
{
	return Brain && Brain->IsRunning();
}

void AEchoEnemyController::StopMovement()
{
	Super::StopMovement();
	bDirectChase = false;
	ChaseGoal = nullptr;
}

void AEchoEnemyController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TargetTimer -= DeltaSeconds;
	if (TargetTimer <= 0.0f)
	{
		TargetTimer = TargetInterval;
		ChooseTarget();
	}

	// Steering straight at the goal where there's no path (and for flyers, which do their own thing in ChaseTick)
	RepathTimer -= DeltaSeconds;
	APawn* Body = GetPawn();
	if (bDirectChase && ChaseGoal.IsValid() && Body)
	{
		const FVector Direction = (ChaseGoal->GetActorLocation() - Body->GetActorLocation()).GetSafeNormal2D();
		Body->AddMovementInput(Direction, 1.0f);
	}
}

void AEchoEnemyController::ChooseTarget()
{
	AEchoEnemy* Enemy = GetEnemy();
	if (!Enemy || Enemy->IsDefeated())
	{
		return;
	}

	// Seen players (AI Perception), plus anyone close or any player at all for arena enemies; nearest allowed wins
	TArray<AActor*> Seen;
	GetPerceptionComponent()->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Seen);
	AOurLastEchoCharacter* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	const float NoticeRadius = Enemy->SightRadius * 0.5f;
	for (TActorIterator<AOurLastEchoCharacter> It(GetWorld()); It; ++It)
	{
		AOurLastEchoCharacter* Player = *It;
		if (!Enemy->CanTarget(Player))
		{
			continue;
		}
		const float Distance = FVector::Dist(Player->GetActorLocation(), Enemy->GetActorLocation());
		const bool bNoticed = Seen.Contains(Player) || Enemy->bAlwaysAware || Distance <= NoticeRadius
			|| (Player == Enemy->GetTarget() && Distance <= Enemy->SightRadius * 1.2f);
		if (bNoticed && Distance < BestDistance)
		{
			Best = Player;
			BestDistance = Distance;
		}
	}
	Enemy->SetTarget(Best);
}

void AEchoEnemyController::ChaseActor(AActor* Goal, float AcceptanceRadius)
{
	if (!Goal)
	{
		return;
	}
	if (Goal != ChaseGoal.Get() || RepathTimer <= 0.0f)
	{
		ChaseGoal = Goal;
		RepathTimer = 0.5f;
		const EPathFollowingRequestResult::Type Result = MoveToActor(Goal, AcceptanceRadius, true, true, false);
		// No navmesh or no path: steer straight at it instead
		bDirectChase = Result == EPathFollowingRequestResult::Failed;
	}
}
