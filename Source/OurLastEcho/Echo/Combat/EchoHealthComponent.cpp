// Our Last Echo

#include "EchoHealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "OurLastEcho.h"

namespace
{
	/** A heavy hit freezes the struck actor this long on every machine (a small hit-stop), in real seconds */
	constexpr float HitStopSeconds = 0.07f;
}

USoundBase* EchoCombat::EngineSound(const TCHAR* Name)
{
	return LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Engine/VREditor/Sounds/%s_Cue.%s_Cue"), Name, Name));
}

UEchoHealthComponent::UEchoHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	HitSound = EchoCombat::EngineSound(TEXT("VR_click2"));
	BlockedSound = EchoCombat::EngineSound(TEXT("VR_negative"));
	DefeatSound = EchoCombat::EngineSound(TEXT("VR_shep_scale_down_02"));
}

UEchoHealthComponent* UEchoHealthComponent::FindOn(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UEchoHealthComponent>() : nullptr;
}

void UEchoHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority())
	{
		Health = MaxHealth;
	}
}

bool UEchoHealthComponent::IsInvulnerable() const
{
	return bGodMode || (GetWorld() && GetWorld()->GetTimeSeconds() < InvulnerableUntil);
}

EEchoDamageResult UEchoHealthComponent::ApplyDamage(const FEchoDamageInfo& InInfo)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return EEchoDamageResult::Ignored;
	}

	FEchoDamageInfo Info = InInfo;
	if (const UEchoHealthComponent* InstigatorHealth = FindOn(Info.Instigator); InstigatorHealth && InstigatorHealth->Team == Team)
	{
		return EEchoDamageResult::Ignored;
	}
	if (bDepleted)
	{
		return EEchoDamageResult::Ignored;
	}

	// The owner's own rules first: who can affect it at all, shells, stuns
	EEchoDamageResult Result = EEchoDamageResult::Applied;
	if (IEchoDamageFilter* Filter = Cast<IEchoDamageFilter>(Owner))
	{
		Result = Filter->FilterIncomingDamage(Info);
	}
	if (Result == EEchoDamageResult::Applied && IsInvulnerable())
	{
		Result = EEchoDamageResult::Immune;
	}

	if (Result == EEchoDamageResult::Applied)
	{
		Health = FMath::Max(0.0f, Health - FMath::Max(0.0f, Info.Amount));
		if (Health <= 0.0f)
		{
			bDepleted = true;
			Result = EEchoDamageResult::Defeated;
		}
	}

	HitCounts.FindOrAdd(Result)++;
	if (Result != EEchoDamageResult::Ignored)
	{
		SendCue(Info, Result);
	}
	UE_LOG(LogOurLastEcho, Verbose, TEXT("Combat: %s hit %s (%.0f, kind %d) -> %d, health %.0f"), *GetNameSafe(Info.Instigator), *GetNameSafe(Owner), Info.Amount, static_cast<int32>(Info.Kind), static_cast<int32>(Result), Health);

	OnDamaged.Broadcast(this, Info, Result);
	if (Result == EEchoDamageResult::Defeated)
	{
		OnDepleted.Broadcast(this, Info);
	}
	return Result;
}

void UEchoHealthComponent::SendCue(const FEchoDamageInfo& Info, EEchoDamageResult Result)
{
	LastHitCue.Counter++;
	LastHitCue.Result = Result;
	LastHitCue.Kind = Info.Kind;
	LastHitCue.bHeavy = Info.bHeavy;
	LastHitCue.Location = Info.HitLocation.IsZero() ? GetOwner()->GetActorLocation() : Info.HitLocation;
	LastHitCue.Instigator = Info.Instigator;
	GetOwner()->ForceNetUpdate();
	PlayCue();
}

void UEchoHealthComponent::Revive()
{
	if (GetOwner()->HasAuthority())
	{
		Health = MaxHealth;
		bDepleted = false;
		GetOwner()->ForceNetUpdate();
	}
}

void UEchoHealthComponent::SetInvulnerableFor(float Seconds)
{
	if (GetOwner()->HasAuthority())
	{
		InvulnerableUntil = FMath::Max(InvulnerableUntil, GetWorld()->GetTimeSeconds() + Seconds);
	}
}

void UEchoHealthComponent::SetGodMode(bool bOn)
{
	if (GetOwner()->HasAuthority())
	{
		bGodMode = bOn;
		GetOwner()->ForceNetUpdate();
	}
}

int32 UEchoHealthComponent::GetHitCount(EEchoDamageResult Result) const
{
	const int32* Count = HitCounts.Find(Result);
	return Count ? *Count : 0;
}

void UEchoHealthComponent::OnRep_HitCue()
{
	PlayCue();
}

void UEchoHealthComponent::PlayCue()
{
	LastCueTime = GetWorld()->GetTimeSeconds();
	AActor* Owner = GetOwner();

	if (GetNetMode() != NM_DedicatedServer)
	{
		USoundBase* Sound =
			LastHitCue.Result == EEchoDamageResult::Blocked || LastHitCue.Result == EEchoDamageResult::Immune ? BlockedSound.Get() :
			LastHitCue.Result == EEchoDamageResult::Defeated ? DefeatSound.Get() : HitSound.Get();
		if (Sound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, LastHitCue.Location);
		}

		// Hit-stop: a heavy hit that landed freezes the enemy it struck for a moment. Never a player: slowing a
		// player's own character would desync its predicted movement
		if (Team == EEchoTeam::Enemies && LastHitCue.bHeavy && (LastHitCue.Result == EEchoDamageResult::Applied || LastHitCue.Result == EEchoDamageResult::Defeated))
		{
			Owner->CustomTimeDilation = 0.02f;
			TWeakObjectPtr<AActor> WeakOwner(Owner);
			FTimerHandle Handle;
			// The timer runs in world time, which the dilation doesn't slow (it only affects the actor)
			GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakOwner]()
			{
				if (WeakOwner.IsValid())
				{
					WeakOwner->CustomTimeDilation = 1.0f;
				}
			}), HitStopSeconds, false);
		}
	}

	OnHitCue.Broadcast(this, LastHitCue);
}

void UEchoHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEchoHealthComponent, Health);
	DOREPLIFETIME(UEchoHealthComponent, bDepleted);
	DOREPLIFETIME(UEchoHealthComponent, bGodMode);
	DOREPLIFETIME(UEchoHealthComponent, LastHitCue);
}
