// Our Last Echo

#include "EchoAnchorPoint.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "EchoVisibility.h"

namespace
{
	const FName GlowParam(TEXT("Glow"));

	/** Latch flash: glow and light multiplier at its peak, and how long it takes to fade */
	constexpr float LatchFlashGlow = 4.0f;
	constexpr float LatchFlashDuration = 0.5f;

	UStaticMeshComponent* SetupPart(UStaticMeshComponent* Part, UStaticMesh* Mesh, USceneComponent* Parent)
	{
		Part->SetStaticMesh(Mesh);
		Part->SetupAttachment(Parent);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(false);
		return Part;
	}
}

AEchoAnchorPoint::AEchoAnchorPoint()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	// Anchors never move; the spawn transform arrives with the actor
	SetReplicatingMovement(false);
	// Saraa's whip must find anchors wherever Bat put them
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(2.0f);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// The arrow like AEchoArrow's, but with its tip buried in the surface (the actor sits at the impact point, +X = flight direction)
	ArrowShaft = SetupPart(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowShaft")), CylinderMesh.Object, Root);
	ArrowShaft->SetRelativeLocation(FVector(-37.0f, 0.0f, 0.0f));
	ArrowShaft->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	ArrowShaft->SetRelativeScale3D(FVector(0.025f, 0.025f, 0.75f));

	ArrowFletching = SetupPart(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowFletching")), ConeMesh.Object, Root);
	ArrowFletching->SetRelativeLocation(FVector(-70.0f, 0.0f, 0.0f));
	ArrowFletching->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	ArrowFletching->SetRelativeScale3D(FVector(0.07f, 0.07f, 0.1f));

	// Saraa's spirit anchor; placed at the swing point in PlaceSpiritAnchor
	SpiritOrb = SetupPart(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpiritOrb")), SphereMesh.Object, Root);
	SpiritOrb->SetUsingAbsoluteRotation(true);
	SpiritOrb->SetUsingAbsoluteScale(true);

	SpiritHalo = SetupPart(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpiritHalo")), CylinderMesh.Object, Root);
	SpiritHalo->SetUsingAbsoluteRotation(true);
	SpiritHalo->SetUsingAbsoluteScale(true);

	SpiritLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("SpiritLight"));
	SpiritLight->SetupAttachment(Root);
	SpiritLight->SetIntensityUnits(ELightUnits::Candelas);
	SpiritLight->SetIntensity(LightIntensity);
	SpiritLight->SetAttenuationRadius(600.0f);
	SpiritLight->SetLightColor(FLinearColor(0.25f, 0.5f, 1.0f));
	SpiritLight->SetCastShadows(false);

	ArrowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Materials/MI_Arrow.MI_Arrow")));
	SpiritMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Materials/MI_SpiritAnchor.MI_SpiritAnchor")));
	HaloMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Materials/MI_SpiritAnchorHalo.MI_SpiritAnchorHalo")));
	AppearSound =TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Engine/VREditor/Sounds/VR_teleport_Cue.VR_teleport_Cue")));
	LatchSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Engine/VREditor/Sounds/VR_grab_Cue.VR_grab_Cue")));
}

void AEchoAnchorPoint::InitSurface(const FVector& Normal)
{
	check(HasAuthority());

	SurfaceNormal = Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	if (const AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		CreationServerTime = GameState->GetServerWorldTimeSeconds();
	}
	PlaceSpiritAnchor();
}

void AEchoAnchorPoint::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInterface* Material = ArrowMaterial.LoadSynchronous())
	{
		ArrowShaft->SetMaterial(0, Material);
		ArrowFletching->SetMaterial(0, Material);
	}
	if (UMaterialInterface* Material = SpiritMaterial.LoadSynchronous())
	{
		SpiritMID = UMaterialInstanceDynamic::Create(Material, this);
		SpiritMID->GetScalarParameterValue(GlowParam, RestingGlow);
		SpiritOrb->SetMaterial(0, SpiritMID);
	}
	if (UMaterialInterface* Material = HaloMaterial.LoadSynchronous())
	{
		SpiritHalo->SetMaterial(0, Material);
	}

	PlaceSpiritAnchor();
	UpdateLocalVisibility();

	if (GetNetMode() != NM_DedicatedServer)
	{
		if (USoundBase* Sound = AppearSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, GetSwingPoint(), 0.7f, 1.3f);
		}
	}
}

void AEchoAnchorPoint::OnRep_SurfaceNormal()
{
	PlaceSpiritAnchor();
}

FVector AEchoAnchorPoint::GetSwingPoint() const
{
	return GetActorLocation() + FVector(SurfaceNormal) * SwingPointOffset;
}

void AEchoAnchorPoint::PlaceSpiritAnchor()
{
	const FVector Point = GetSwingPoint();
	SpiritOrb->SetWorldLocation(Point);
	SpiritLight->SetWorldLocation(Point);

	// A faint disc behind the orb, facing out of the surface
	SpiritHalo->SetWorldLocation(GetActorLocation() + FVector(SurfaceNormal) * 3.0f);
	SpiritHalo->SetWorldRotation(FRotationMatrix::MakeFromZ(SurfaceNormal).Rotator());

	const float Scale = (bHighlighted ? HighlightScale : 1.0f);
	SpiritOrb->SetWorldScale3D(FVector(OrbDiameter / 100.0f * Scale));
	SpiritHalo->SetWorldScale3D(FVector(OrbDiameter * 2.2f / 100.0f * Scale, OrbDiameter * 2.2f / 100.0f * Scale, 0.01f));
}

void AEchoAnchorPoint::SetHighlighted(bool bNewHighlighted)
{
	if (bHighlighted == bNewHighlighted)
	{
		return;
	}
	bHighlighted = bNewHighlighted;
	PlaceSpiritAnchor();
}

void AEchoAnchorPoint::PlayLatchCue()
{
	++LocalLatchCues;
	FlashRemaining = LatchFlashDuration;

	if (GetNetMode() != NM_DedicatedServer)
	{
		if (USoundBase* Sound = LatchSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, GetSwingPoint(), 1.0f, 1.2f);
		}
	}
}

bool AEchoAnchorPoint::IsSpiritAnchorVisibleLocally() const
{
	return bSpiritVisible;
}

bool AEchoAnchorPoint::IsArrowVisibleLocally() const
{
	return bArrowVisible;
}

void AEchoAnchorPoint::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	UpdateLocalVisibility();

	if (!bSpiritVisible)
	{
		return;
	}

	// A slow breathing glow, brighter while targeted, with a burst when a whip latches on
	FlashRemaining = FMath::Max(0.0f, FlashRemaining - DeltaSeconds);
	const float Flash = FlashRemaining / LatchFlashDuration;
	const float Breath = 0.85f + 0.15f * FMath::Sin(Age * 3.0f);
	const float Glow = RestingGlow * Breath * (bHighlighted ? HighlightGlow : 1.0f) * FMath::Lerp(1.0f, LatchFlashGlow, Flash);
	if (SpiritMID)
	{
		SpiritMID->SetScalarParameterValue(GlowParam, Glow);
	}
	SpiritLight->SetIntensity(LightIntensity * Breath * (bHighlighted ? 2.0f : 1.0f) * FMath::Lerp(1.0f, LatchFlashGlow, Flash));
}

void AEchoAnchorPoint::UpdateLocalVisibility()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	EEchoRealm Viewer = EEchoRealm::Living;
	const bool bHasViewer = EchoVisibility::GetLocalViewerRealm(GetWorld(), Viewer);
	const bool bShowAll = EchoVisibility::IsDebugShowAll(GetWorld());
	const bool bShowSpirit = (bHasViewer && Viewer == EEchoRealm::Spirit) || bShowAll;
	const bool bShowArrow = !bHasViewer || Viewer == EEchoRealm::Living || bShowAll;

	if (bVisibilityKnown && bShowSpirit == bSpiritVisible && bShowArrow == bArrowVisible)
	{
		return;
	}
	bVisibilityKnown = true;
	bSpiritVisible = bShowSpirit;
	bArrowVisible = bShowArrow;

	// Local only (never SetActorHiddenInGame, which replicates)
	SpiritOrb->SetVisibility(bShowSpirit);
	SpiritHalo->SetVisibility(bShowSpirit);
	SpiritLight->SetVisibility(bShowSpirit);
	ArrowShaft->SetVisibility(bShowArrow);
	ArrowFletching->SetVisibility(bShowArrow);
}

void AEchoAnchorPoint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AEchoAnchorPoint, SurfaceNormal, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AEchoAnchorPoint, CreationServerTime, COND_InitialOnly);
}
