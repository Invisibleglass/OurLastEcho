// Our Last Echo

#include "EchoEraDirector.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "OurLastEchoCharacter.h"
#include "OurLastEcho.h"

namespace
{
	int32 EraIndex(EEchoEra Era)
	{
		return Era == EEchoEra::Present ? 0 : 1;
	}

	const TCHAR* EraName(EEchoEra Era)
	{
		return Era == EEchoEra::Present ? TEXT("present") : TEXT("past");
	}
}

AEchoEraDirector::AEchoEraDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	// Keeps loading and fading while the in-game menu has the game paused
	PrimaryActorTick.bTickEvenWhenPaused = true;
	SetActorTickEnabled(true);

	// Each machine decides for itself; nothing here is shared
	bReplicates = false;
	SetCanBeDamaged(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	PresentLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Echo/Maps/Lvl_SpiritPath_Present.Lvl_SpiritPath_Present")));
	PastLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Echo/Maps/Lvl_SpiritPath_Past.Lvl_SpiritPath_Past")));
	EraCollection = TSoftObjectPtr<UMaterialParameterCollection>(FSoftObjectPath(TEXT("/Game/Echo/Eras/MPC_Era.MPC_Era")));
}

AEchoEraDirector* AEchoEraDirector::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AEchoEraDirector> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AEchoEraDirector::BeginPlay()
{
	Super::BeginPlay();

	// Black from the first frame until the local era is in, so the bare shared layer is never seen
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld()))
		{
			HoldBlack(PC);
		}
	}
}

void AEchoEraDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Leaving the level (e.g. back to the title screen): let the era go with it
	UnloadEra(EEchoEra::Present);
	UnloadEra(EEchoEra::Past);
	Super::EndPlay(EndPlayReason);
}

bool AEchoEraDirector::ResolveRoleEra(const APlayerController* PC)
{
	if (const AOurLastEchoCharacter* Character = PC ? Cast<AOurLastEchoCharacter>(PC->GetPawn()) : nullptr)
	{
		const EEchoEra Era = EchoEraForRealm(Character->GetRealm());
		if (!RoleEra.IsSet() || RoleEra.GetValue() != Era)
		{
			UE_LOG(LogOurLastEcho, Log, TEXT("Eras: the local player plays %s, showing the %s"), *GetNameSafe(Character), EraName(Era));
			RoleEra = Era;
		}
	}
	return RoleEra.IsSet();
}

bool AEchoEraDirector::GetShownEra(EEchoEra& OutEra) const
{
	if (DebugEra.IsSet())
	{
		OutEra = DebugEra.GetValue();
		return true;
	}
	if (RoleEra.IsSet())
	{
		OutEra = RoleEra.GetValue();
		return true;
	}
	return false;
}

void AEchoEraDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
	if (!PC)
	{
		return;
	}

	ResolveRoleEra(PC);
	EEchoEra Shown;
	if (!GetShownEra(Shown))
	{
		// Role not known yet (a client still waiting for its character): stay black
		HoldBlack(PC);
		return;
	}

	const bool bWant[2] = { bShowBoth || Shown == EEchoEra::Present, bShowBoth || Shown == EEchoEra::Past };

	// Load what's wanted; only once it's all visible, drop what isn't, then fade in (so the view never goes empty)
	bool bAllVisible = true;
	for (const EEchoEra Era : { EEchoEra::Present, EEchoEra::Past })
	{
		if (bWant[EraIndex(Era)])
		{
			LoadEra(Era);
			bAllVisible &= IsEraVisible(Era);
			if (IsEraVisible(Era) && !bCollisionStripped[EraIndex(Era)])
			{
				StripCollision(Era);
			}
		}
	}
	if (!bAllVisible)
	{
		HoldBlack(PC);
		return;
	}
	for (const EEchoEra Era : { EEchoEra::Present, EEchoEra::Past })
	{
		if (!bWant[EraIndex(Era)])
		{
			UnloadEra(Era);
		}
	}
	ApplyEraParameter(bWant[0], bWant[1]);

	if (bHeldBlack)
	{
		bHeldBlack = false;
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, FadeInTime, FLinearColor::Black, false, false);
		}
		UE_LOG(LogOurLastEcho, Log, TEXT("Eras: %s ready, fading in"), bShowBoth ? TEXT("both eras") : EraName(Shown));
	}
	bReady = true;
}

void AEchoEraDirector::LoadEra(EEchoEra Era)
{
	TObjectPtr<ULevelStreamingDynamic>& Slot = Streaming[EraIndex(Era)];
	if (Slot)
	{
		return;
	}
	const TSoftObjectPtr<UWorld>& Level = LevelFor(Era);
	if (Level.IsNull())
	{
		return;
	}
	bool bSuccess = false;
	Slot = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(this, Level, FVector::ZeroVector, FRotator::ZeroRotator, bSuccess);
	UE_LOG(LogOurLastEcho, Log, TEXT("Eras: loading the %s (%s) on this machine: %s"), EraName(Era), *Level.ToString(), bSuccess ? TEXT("ok") : TEXT("FAILED"));
	if (!bSuccess)
	{
		Slot = nullptr;
	}
	bReady = false;
}

void AEchoEraDirector::StripCollision(EEchoEra Era)
{
	// An era layer exists on one machine only, so anything in it that collided would be felt by that player alone
	// and fight the host's movement: whatever the level holds (even the editor's builder brush), no collision
	int32 Stripped = 0;
	for (AActor* Actor : GetEraActors(Era))
	{
		TInlineComponentArray<UPrimitiveComponent*> Primitives(Actor);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				++Stripped;
			}
		}
	}
	bCollisionStripped[EraIndex(Era)] = true;
	UE_LOG(LogOurLastEcho, Log, TEXT("Eras: the %s layer is visual only (%d component(s) had collision switched off)"), EraName(Era), Stripped);
}

void AEchoEraDirector::UnloadEra(EEchoEra Era)
{
	TObjectPtr<ULevelStreamingDynamic>& Slot = Streaming[EraIndex(Era)];
	bCollisionStripped[EraIndex(Era)] = false;
	if (!Slot)
	{
		return;
	}
	UE_LOG(LogOurLastEcho, Log, TEXT("Eras: unloading the %s"), EraName(Era));
	Slot->SetShouldBeLoaded(false);
	Slot->SetShouldBeVisible(false);
	Slot->SetIsRequestingUnloadAndRemoval(true);
	Slot = nullptr;
}

bool AEchoEraDirector::IsEraVisible(EEchoEra Era) const
{
	const ULevelStreamingDynamic* Slot = Streaming[EraIndex(Era)];
	return Slot && Slot->IsLevelVisible();
}

bool AEchoEraDirector::IsEraLoaded(EEchoEra Era) const
{
	const ULevelStreamingDynamic* Slot = Streaming[EraIndex(Era)];
	return Slot && Slot->IsLevelLoaded();
}

TArray<AActor*> AEchoEraDirector::GetEraActors(EEchoEra Era) const
{
	TArray<AActor*> Result;
	const ULevelStreamingDynamic* Slot = Streaming[EraIndex(Era)];
	if (const ULevel* Level = Slot ? Slot->GetLoadedLevel() : nullptr)
	{
		for (AActor* Actor : Level->Actors)
		{
			if (Actor)
			{
				Result.Add(Actor);
			}
		}
	}
	return Result;
}

void AEchoEraDirector::HoldBlack(APlayerController* PC)
{
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->SetManualCameraFade(1.0f, FLinearColor::Black, false);
		bHeldBlack = true;
	}
}

void AEchoEraDirector::ApplyEraParameter(bool bWantPresent, bool bWantPast)
{
	if (UMaterialParameterCollection* Collection = EraCollection.LoadSynchronous())
	{
		const float Past = bWantPast ? (bWantPresent ? 0.5f : 1.0f) : 0.0f;
		UKismetMaterialLibrary::SetScalarParameterValue(this, Collection, TEXT("Past"), Past);
	}
}

void AEchoEraDirector::SetDebugEra(bool bOverride, EEchoEra Era)
{
	if (bOverride)
	{
		DebugEra = Era;
	}
	else
	{
		DebugEra.Reset();
	}
	UE_LOG(LogOurLastEcho, Log, TEXT("Eras: debug view %s"), bOverride ? EraName(Era) : TEXT("off (the player's own era)"));
}

void AEchoEraDirector::SwapEra()
{
	EEchoEra Shown;
	if (GetShownEra(Shown))
	{
		SetDebugEra(true, Shown == EEchoEra::Present ? EEchoEra::Past : EEchoEra::Present);
	}
}

void AEchoEraDirector::SetShowBothEras(bool bInShowBoth)
{
	bShowBoth = bInShowBoth;
	UE_LOG(LogOurLastEcho, Log, TEXT("Eras: show both %s"), bShowBoth ? TEXT("ON") : TEXT("OFF"));
}
