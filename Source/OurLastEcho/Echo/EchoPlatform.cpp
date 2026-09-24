// Our Last Echo

#include "EchoPlatform.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "EchoTypes.h"
#include "EchoVisibility.h"
#include "OurLastEcho.h"

namespace
{
	constexpr int32 NumEdges = 12;

	/** Scalar parameters on the materials made by Scripts/build_spirit_bow.py */
	const FName GlowParam(TEXT("Glow"));
	const FName FlickerParam(TEXT("Flicker"));

	/** Base glow of MI_SpiritPlatform, and how much brighter the wake-up flash starts */
	constexpr float SlabBaseGlow = 4.0f;
	constexpr float SlabFlashGlow = 40.0f;
	constexpr float OutlineFlashGlow = 25.0f;
	constexpr float FlashLightCandelas = 400.0f;
}

AEchoPlatform::AEchoPlatform()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	bAlwaysRelevant = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Slab = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Slab"));
	Slab->SetupAttachment(RootComponent);
	Slab->SetStaticMesh(CubeMesh.Object);
	// Dormant: nothing collides. ApplyAwakeState makes it block SpiritPawn (Saraa) when awake - never Pawn (Bat)
	Slab->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Slab->SetCollisionObjectType(ECC_WorldStatic);
	Slab->SetCollisionResponseToAllChannels(ECR_Ignore);
	Slab->SetGenerateOverlapEvents(false);
	Slab->SetCanEverAffectNavigation(false);

	// What arrows (and the bow's aim trace) hit, awake or not
	HitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
	HitBox->SetupAttachment(RootComponent);
	HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitBox->SetCollisionObjectType(ECC_WorldStatic);
	HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitBox->SetCollisionResponseToChannel(ECC_EchoArrow, ECR_Block);
	HitBox->SetGenerateOverlapEvents(false);
	HitBox->SetCanEverAffectNavigation(false);

	FlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FlashLight"));
	FlashLight->SetupAttachment(RootComponent);
	FlashLight->SetIntensityUnits(ELightUnits::Candelas);
	FlashLight->SetIntensity(0.0f);
	FlashLight->SetAttenuationRadius(900.0f);
	FlashLight->SetLightColor(FLinearColor(0.45f, 0.75f, 1.0f));
	FlashLight->SetCastShadows(false);
	FlashLight->SetVisibility(false);

	for (int32 Index = 0; Index < NumEdges; ++Index)
	{
		UStaticMeshComponent* Edge = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Edge%d"), Index));
		Edge->SetupAttachment(RootComponent);
		Edge->SetStaticMesh(CubeMesh.Object);
		Edge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Edge->SetCastShadow(false);
		Edge->SetGenerateOverlapEvents(false);
		Edge->SetCanEverAffectNavigation(false);
		OutlineEdges.Add(Edge);
	}

	SlabMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Materials/MI_SpiritPlatform.MI_SpiritPlatform")));
	OutlineMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Materials/MI_EchoOutline.MI_EchoOutline")));
	AwakenSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Engine/VREditor/Sounds/VR_confirm_Cue.VR_confirm_Cue")));
	SleepSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Engine/VREditor/Sounds/VR_negative_Cue.VR_negative_Cue")));
}

void AEchoPlatform::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// The engine cube is 100 cm and centred; the actor origin is the top surface centre
	const FVector Half = PlatformSize * 0.5f;
	const FVector Centre(0.0f, 0.0f, -Half.Z);

	Slab->SetRelativeScale3D(PlatformSize / 100.0f);
	Slab->SetRelativeLocation(Centre);

	HitBox->SetBoxExtent(Half + FVector(2.0f));
	HitBox->SetRelativeLocation(Centre);

	FlashLight->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));

	// Outline: 4 bars along each axis, on the edges of the slab's box
	const float T = OutlineThickness / 100.0f;
	int32 EdgeIndex = 0;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const int32 A = (Axis + 1) % 3;
		const int32 B = (Axis + 2) % 3;
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			FVector Location = Centre;
			Location[A] += (Corner & 1 ? 1.0f : -1.0f) * Half[A];
			Location[B] += (Corner & 2 ? 1.0f : -1.0f) * Half[B];

			FVector Scale(T, T, T);
			Scale[Axis] = (PlatformSize[Axis] + OutlineThickness) / 100.0f;

			OutlineEdges[EdgeIndex]->SetRelativeLocation(Location);
			OutlineEdges[EdgeIndex]->SetRelativeScale3D(Scale);
			++EdgeIndex;
		}
	}

	if (UMaterialInterface* Material = SlabMaterial.LoadSynchronous())
	{
		Slab->SetMaterial(0, Material);
	}
	if (UMaterialInterface* Material = OutlineMaterial.LoadSynchronous())
	{
		for (UStaticMeshComponent* Edge : OutlineEdges)
		{
			Edge->SetMaterial(0, Material);
		}
	}
}

void AEchoPlatform::BeginPlay()
{
	Super::BeginPlay();

	SlabMID = Slab->CreateDynamicMaterialInstance(0);
	if (UMaterialInterface* Material = OutlineMaterial.Get())
	{
		OutlineMID = UMaterialInstanceDynamic::Create(Material, this);
		for (UStaticMeshComponent* Edge : OutlineEdges)
		{
			Edge->SetMaterial(0, OutlineMID);
		}
	}

	bLocalAwakeApplied = bAwake;   // adopt the current state without playing a transition cue
	ApplyAwakeState();
	UpdateLocalVisibility();
}

void AEchoPlatform::Awaken()
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogOurLastEcho, Log, TEXT("%s awakened%s"), *GetName(), bTimed ? *FString::Printf(TEXT(" for %.1fs"), AwakeDuration) : TEXT(""));

	bAwake = true;
	++AwakenCount;

	GetWorldTimerManager().ClearTimer(SleepTimer);
	if (bTimed)
	{
		const AGameStateBase* GameState = GetWorld()->GetGameState();
		AwakeUntilServerTime = (GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()) + AwakeDuration;
		GetWorldTimerManager().SetTimer(SleepTimer, this, &AEchoPlatform::Sleep, AwakeDuration, false);
	}
	else
	{
		AwakeUntilServerTime = 0.0f;
	}

	// The listen server is also a player, so run the same client-side reactions here
	OnRep_Awake();
	OnRep_AwakenCount();
	ForceNetUpdate();
}

void AEchoPlatform::Sleep()
{
	if (!HasAuthority() || !bAwake)
	{
		return;
	}

	UE_LOG(LogOurLastEcho, Log, TEXT("%s went dormant"), *GetName());

	bAwake = false;
	AwakeUntilServerTime = 0.0f;
	GetWorldTimerManager().ClearTimer(SleepTimer);
	OnRep_Awake();
	ForceNetUpdate();
}

void AEchoPlatform::OnRep_Awake()
{
	ApplyAwakeState();
	UpdateLocalVisibility();
}

void AEchoPlatform::OnRep_AwakenCount()
{
	PlayAwakenCue();
}

void AEchoPlatform::ApplyAwakeState()
{
	// Same response on every machine, so Saraa's client-side movement prediction agrees with the server
	Slab->SetCollisionResponseToChannel(ECC_SpiritPawn, bAwake ? ECR_Block : ECR_Ignore);

	if (OutlineMID)
	{
		OutlineMID->SetScalarParameterValue(FlickerParam, bAwake ? 0.0f : 1.0f);
		OutlineMID->SetScalarParameterValue(GlowParam, bAwake ? AwakeOutlineGlow : DormantOutlineGlow);
	}

	const bool bWentToSleep = bLocalAwakeApplied && !bAwake;
	bLocalAwakeApplied = bAwake;

	if (bWentToSleep && HasActorBegunPlay() && GetNetMode() != NM_DedicatedServer)
	{
		if (USoundBase* Sound = SleepSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), 0.7f, 0.8f);
		}
	}
}

void AEchoPlatform::PlayAwakenCue()
{
	if (GetNetMode() == NM_DedicatedServer || !HasActorBegunPlay())
	{
		return;
	}

	++LocalAwakenCues;
	FlashRemaining = FlashDuration;
	FlashLight->SetVisibility(true);

	if (USoundBase* Sound = AwakenSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
}

float AEchoPlatform::GetRemainingAwakeTime() const
{
	if (!bAwake || AwakeUntilServerTime <= 0.0f)
	{
		return 0.0f;
	}

	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const float Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	return FMath::Max(0.0f, AwakeUntilServerTime - Now);
}

void AEchoPlatform::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Wake-up flash: the light and both materials spike, then settle back
	if (FlashRemaining > 0.0f)
	{
		FlashRemaining = FMath::Max(0.0f, FlashRemaining - DeltaSeconds);
		const float Alpha = FlashDuration > 0.0f ? FMath::Square(FlashRemaining / FlashDuration) : 0.0f;

		FlashLight->SetIntensity(FlashLightCandelas * Alpha);
		FlashLight->SetVisibility(Alpha > 0.0f);
		if (SlabMID)
		{
			SlabMID->SetScalarParameterValue(GlowParam, FMath::Lerp(SlabBaseGlow, SlabFlashGlow, Alpha));
		}
		if (OutlineMID)
		{
			const float Resting = bAwake ? AwakeOutlineGlow : DormantOutlineGlow;
			OutlineMID->SetScalarParameterValue(GlowParam, FMath::Lerp(Resting, OutlineFlashGlow, Alpha));
		}
	}

	UpdateLocalVisibility();
}

bool AEchoPlatform::IsSlabVisibleLocally() const
{
	return Slab->IsVisible();
}

bool AEchoPlatform::IsOutlineVisibleLocally() const
{
	return OutlineEdges.Num() > 0 && OutlineEdges[0]->IsVisible();
}

void AEchoPlatform::UpdateLocalVisibility()
{
	EEchoRealm Viewer = EEchoRealm::Living;
	const bool bHasViewer = EchoVisibility::GetLocalViewerRealm(GetWorld(), Viewer);
	const bool bDebug = EchoVisibility::IsDebugShowAll(GetWorld());

	// Saraa sees the awake slab (blinking in a timed platform's last seconds); Bat never sees the slab
	bool bShowSlab = bAwake && ((bHasViewer && Viewer == EEchoRealm::Spirit) || bDebug);
	if (bShowSlab && bTimed)
	{
		const float Remaining = GetRemainingAwakeTime();
		if (Remaining > 0.0f && Remaining < WarningDuration)
		{
			bShowSlab = FMath::Frac(Remaining * 3.0f) > 0.35f;
		}
	}

	// Bat always sees the outline: flickering while dormant, steady and fainter once awake
	const bool bShowOutline = (bHasViewer && Viewer == EEchoRealm::Living) || bDebug;

	// Component visibility is local-only (unlike SetActorHiddenInGame, which replicates)
	if (Slab->IsVisible() != bShowSlab)
	{
		Slab->SetVisibility(bShowSlab);
	}
	if (OutlineEdges.Num() > 0 && OutlineEdges[0]->IsVisible() != bShowOutline)
	{
		for (UStaticMeshComponent* Edge : OutlineEdges)
		{
			Edge->SetVisibility(bShowOutline);
		}
	}
}

void AEchoPlatform::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEchoPlatform, bAwake);
	DOREPLIFETIME(AEchoPlatform, AwakenCount);
	DOREPLIFETIME(AEchoPlatform, AwakeUntilServerTime);
}
