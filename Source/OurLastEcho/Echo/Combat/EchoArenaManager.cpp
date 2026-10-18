// Our Last Echo

#include "EchoArenaManager.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "EchoEnemy.h"
#include "EchoNetDiagnostics.h"
#include "EchoPlayerCombatComponent.h"
#include "EchoTypes.h"
#include "OurLastEcho.h"
#include "OurLastEchoCharacter.h"

#define LOCTEXT_NAMESPACE "EchoArena"

AEchoArenaManager::AEchoArenaManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	StartZone = CreateDefaultSubobject<UBoxComponent>(TEXT("StartZone"));
	StartZone->InitBoxExtent(FVector(600.0f, 600.0f, 300.0f));
	StartZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StartZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	StartZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	StartZone->SetCollisionResponseToChannel(ECC_SpiritPawn, ECR_Overlap);
	StartZone->SetGenerateOverlapEvents(true);
	RootComponent = StartZone;

	// EnemyClasses empty = each type's Blueprint (BP_DustWraith...), see GetDefaultEnemyClass
	WaveStartSound = EchoCombat::EngineSound(TEXT("VR_confirm"));
	ArenaClearedSound = EchoCombat::EngineSound(TEXT("VR_shep_scale_up_04"));

	auto MakeWave = [](const FText& Title, const FText& Hint, TArray<FEchoWaveEntry> Enemies)
	{
		FEchoArenaWave Wave;
		Wave.Title = Title;
		Wave.Hint = Hint;
		Wave.Enemies = MoveTemp(Enemies);
		return Wave;
	};
	Waves.Add(MakeWave(LOCTEXT("Wave1", "Dust Wraiths and Shades"), LOCTEXT("Hint1", "Each of you fights what only you can see. Tell each other where they are."),
		{ { EEchoEnemyType::DustWraith, 2 }, { EEchoEnemyType::Shade, 2 } }));
	Waves.Add(MakeWave(LOCTEXT("Wave2", "Stone Guardians"), LOCTEXT("Hint2", "Arrows bounce off the shell. Saraa: crack it with the whip. Bat: shoot while it glows."),
		{ { EEchoEnemyType::StoneGuardian, 2 } }));
	Waves.Add(MakeWave(LOCTEXT("Wave3", "Sky Talons"), LOCTEXT("Hint3", "Out of the whip's reach. Bat: shoot one down. Saraa: lash it while it's stunned."),
		{ { EEchoEnemyType::SkyTalon, 2 } }));
	Waves.Add(MakeWave(LOCTEXT("Wave4", "All of them"), LOCTEXT("Hint4", "Everything at once. Keep talking."),
		{ { EEchoEnemyType::DustWraith, 1 }, { EEchoEnemyType::Shade, 1 }, { EEchoEnemyType::StoneGuardian, 1 }, { EEchoEnemyType::SkyTalon, 1 } }));
}

AEchoArenaManager* AEchoArenaManager::Get(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AEchoArenaManager> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AEchoArenaManager::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		StartZone->OnComponentBeginOverlap.AddDynamic(this, &AEchoArenaManager::OnStartZoneOverlap);
	}
}

void AEchoArenaManager::OnStartZoneOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (State == EEchoArenaState::Idle && Cast<AOurLastEchoCharacter>(OtherActor))
	{
		StartArena();
	}
}

TSubclassOf<AEchoEnemy> AEchoArenaManager::GetEnemyClass(EEchoEnemyType Type) const
{
	const TSubclassOf<AEchoEnemy>* Class = EnemyClasses.Find(Type);
	return Class && *Class ? *Class : GetDefaultEnemyClass(Type);
}

TSubclassOf<AEchoEnemy> AEchoArenaManager::GetDefaultEnemyClass(EEchoEnemyType Type)
{
	const TCHAR* Path =
		Type == EEchoEnemyType::Shade ? TEXT("/Game/Echo/Combat/BP_Shade.BP_Shade_C") :
		Type == EEchoEnemyType::StoneGuardian ? TEXT("/Game/Echo/Combat/BP_StoneGuardian.BP_StoneGuardian_C") :
		Type == EEchoEnemyType::SkyTalon ? TEXT("/Game/Echo/Combat/BP_SkyTalon.BP_SkyTalon_C") :
		TEXT("/Game/Echo/Combat/BP_DustWraith.BP_DustWraith_C");
	if (UClass* Blueprint = LoadClass<AEchoEnemy>(nullptr, Path))
	{
		return Blueprint;
	}
	return Type == EEchoEnemyType::Shade ? AEchoShade::StaticClass() : Type == EEchoEnemyType::StoneGuardian ? AEchoStoneGuardian::StaticClass()
		: Type == EEchoEnemyType::SkyTalon ? AEchoSkyTalon::StaticClass() : AEchoDustWraith::StaticClass();
}

AEchoEnemy* AEchoArenaManager::SpawnEnemyAnywhere(UWorld* World, EEchoEnemyType Type, const FVector& Location)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	if (AEchoArenaManager* Arena = Get(World))
	{
		return Arena->SpawnEnemy(Type, Location, false);
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	return World->SpawnActor<AEchoEnemy>(GetDefaultEnemyClass(Type), Location, FRotator::ZeroRotator, Params);
}

void AEchoArenaManager::SetState(EEchoArenaState NewState)
{
	State = NewState;
	ForceNetUpdate();
	OnRep_State();
}

void AEchoArenaManager::OnRep_State()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	USoundBase* Sound = State == EEchoArenaState::Fighting ? WaveStartSound.Get() : State == EEchoArenaState::Complete ? ArenaClearedSound.Get() : nullptr;
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
}

void AEchoArenaManager::StartArena()
{
	if (!HasAuthority())
	{
		return;
	}
	ClearEnemies();
	WaveIndex = -1;
	SetState(EEchoArenaState::Intermission);
	UEchoNetDiagnostics::Log(this, TEXT("Arena: starting"));
	GetWorldTimerManager().SetTimer(WaveTimer, FTimerDelegate::CreateUObject(this, &AEchoArenaManager::BeginWave, 0), FMath::Max(0.1f, IntermissionSeconds), false);
}

void AEchoArenaManager::BeginWave(int32 Index)
{
	if (!Waves.IsValidIndex(Index))
	{
		return;
	}
	WaveIndex = Index;
	SpawnCursor = 0;
	for (const FEchoWaveEntry& Entry : Waves[Index].Enemies)
	{
		for (int32 Count = 0; Count < Entry.Count; ++Count)
		{
			const bool bAir = Entry.Type == EEchoEnemyType::SkyTalon;
			const TArray<TObjectPtr<AActor>>& Points = bAir && AirSpawnPoints.Num() > 0 ? AirSpawnPoints : GroundSpawnPoints;
			FVector Location = GetActorLocation() + FVector(0.0f, 0.0f, bAir ? 600.0f : 100.0f);
			if (Points.Num() > 0)
			{
				const AActor* Point = Points[SpawnCursor % Points.Num()];
				Location = Point ? Point->GetActorLocation() : Location;
			}
			++SpawnCursor;
			SpawnEnemy(Entry.Type, Location, true);
		}
	}
	EnemiesRemaining = LiveEnemies.Num();
	SetState(EEchoArenaState::Fighting);
	UEchoNetDiagnostics::Log(this, FString::Printf(TEXT("Arena: wave %d (%s), %d enemies"), Index + 1, *Waves[Index].Title.ToString(), EnemiesRemaining));
	if (EnemiesRemaining == 0)
	{
		// An empty wave counts as cleared
		HandleEnemyDefeated(nullptr);
	}
}

AEchoEnemy* AEchoArenaManager::SpawnEnemy(EEchoEnemyType Type, FVector Location, bool bCountForWave)
{
	if (!HasAuthority())
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FRotator Facing = (GetActorLocation() - Location).GetSafeNormal2D().Rotation();
	AEchoEnemy* Enemy = GetWorld()->SpawnActorDeferred<AEchoEnemy>(GetEnemyClass(Type), FTransform(Facing, Location), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Enemy)
	{
		return nullptr;
	}
	Enemy->bAlwaysAware = true;
	Enemy->FinishSpawning(FTransform(Facing, Location));
	if (bCountForWave)
	{
		LiveEnemies.Add(Enemy);
		Enemy->OnDefeated.AddUObject(this, &AEchoArenaManager::HandleEnemyDefeated);
	}
	return Enemy;
}

void AEchoArenaManager::HandleEnemyDefeated(AEchoEnemy* Enemy)
{
	LiveEnemies.Remove(Enemy);
	LiveEnemies.RemoveAll([](const AEchoEnemy* Live) { return !IsValid(Live) || Live->IsDefeated(); });
	EnemiesRemaining = LiveEnemies.Num();
	ForceNetUpdate();
	if (EnemiesRemaining > 0 || State != EEchoArenaState::Fighting)
	{
		return;
	}
	if (WaveIndex + 1 >= Waves.Num())
	{
		SetState(EEchoArenaState::Complete);
		UEchoNetDiagnostics::Log(this, TEXT("Arena: cleared"));
		return;
	}
	SetState(EEchoArenaState::Intermission);
	GetWorldTimerManager().SetTimer(WaveTimer, FTimerDelegate::CreateUObject(this, &AEchoArenaManager::BeginWave, WaveIndex + 1), FMath::Max(0.1f, IntermissionSeconds), false);
}

void AEchoArenaManager::ClearEnemies()
{
	for (AEchoEnemy* Enemy : LiveEnemies)
	{
		if (IsValid(Enemy))
		{
			Enemy->RemoveQuietly();
		}
	}
	LiveEnemies.Reset();
	EnemiesRemaining = 0;
	GetWorldTimerManager().ClearTimer(WaveTimer);
}

void AEchoArenaManager::RestartArena()
{
	if (!HasAuthority())
	{
		return;
	}
	ClearEnemies();
	// Also any debug-spawned enemies: a clean start
	for (TActorIterator<AEchoEnemy> It(GetWorld()); It; ++It)
	{
		It->RemoveQuietly();
	}
	++RestartCount;
	WaveIndex = -1;
	SetState(EEchoArenaState::Idle);
	UEchoNetDiagnostics::Log(this, TEXT("Arena: restart"));

	// Both players back at the entrance checkpoint, on their feet
	for (TActorIterator<AOurLastEchoCharacter> It(GetWorld()); It; ++It)
	{
		It->RespawnAtStart();
		It->GetPlayerCombat()->ServerRevive();
	}

	SetState(EEchoArenaState::Intermission);
	GetWorldTimerManager().SetTimer(WaveTimer, FTimerDelegate::CreateUObject(this, &AEchoArenaManager::BeginWave, 0), FMath::Max(0.1f, RestartDelay), false);
}

void AEchoArenaManager::SkipToWave(int32 WaveNumber)
{
	if (!HasAuthority() || !Waves.IsValidIndex(WaveNumber - 1))
	{
		return;
	}
	ClearEnemies();
	BeginWave(WaveNumber - 1);
}

TArray<AEchoEnemy*> AEchoArenaManager::GetLiveEnemies() const
{
	TArray<AEchoEnemy*> Result;
	for (AEchoEnemy* Enemy : LiveEnemies)
	{
		if (IsValid(Enemy) && !Enemy->IsDefeated())
		{
			Result.Add(Enemy);
		}
	}
	return Result;
}

FText AEchoArenaManager::GetStatusText() const
{
	switch (State)
	{
	case EEchoArenaState::Idle:
		return LOCTEXT("Idle", "Arena: step inside to begin");
	case EEchoArenaState::Intermission:
		return Waves.IsValidIndex(WaveIndex + 1)
			? FText::Format(LOCTEXT("Next", "Get ready: wave {0} of {1}, {2}"), FText::AsNumber(WaveIndex + 2), FText::AsNumber(Waves.Num()), Waves[WaveIndex + 1].Title)
			: FText::GetEmpty();
	case EEchoArenaState::Fighting:
		return Waves.IsValidIndex(WaveIndex)
			? FText::Format(LOCTEXT("Fighting", "Wave {0} of {1}: {2}  -  {3} left"), FText::AsNumber(WaveIndex + 1), FText::AsNumber(Waves.Num()), Waves[WaveIndex].Title, FText::AsNumber(EnemiesRemaining))
			: FText::GetEmpty();
	default:
		return LOCTEXT("Complete", "Arena cleared! Use the restart pad to go again.");
	}
}

FText AEchoArenaManager::GetHintText() const
{
	const int32 Index = State == EEchoArenaState::Intermission ? WaveIndex + 1 : WaveIndex;
	return (State == EEchoArenaState::Fighting || State == EEchoArenaState::Intermission) && Waves.IsValidIndex(Index) ? Waves[Index].Hint : FText::GetEmpty();
}

void AEchoArenaManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEchoArenaManager, State);
	DOREPLIFETIME(AEchoArenaManager, WaveIndex);
	DOREPLIFETIME(AEchoArenaManager, EnemiesRemaining);
	DOREPLIFETIME(AEchoArenaManager, RestartCount);
}

#undef LOCTEXT_NAMESPACE
