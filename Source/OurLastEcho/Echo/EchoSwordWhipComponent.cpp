// Our Last Echo

#include "EchoSwordWhipComponent.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "EchoAnchorPoint.h"
#include "EchoAnchorTarget.h"
#include "EchoCharacterMovementComponent.h"
#include "EchoTypes.h"
#include "OurLastEcho.h"
#include "OurLastEchoCharacter.h"

bool UEchoSwordWhipComponent::bDebugDraw = false;

namespace
{
	struct FSwordPart
	{
		const TCHAR* Mesh;
		FVector Location;
		FVector Scale;
	};

	/** Placeholder sword from engine shapes, in the sword's own space: +Z = from the pommel to the tip */
	const FSwordPart SwordShape[] =
	{
		{ TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(0.0f, 0.0f,  8.0f), FVector(0.035f, 0.035f, 0.18f) },   // grip
		{ TEXT("/Engine/BasicShapes/Sphere.Sphere"),     FVector(0.0f, 0.0f, -2.0f), FVector(0.06f, 0.06f, 0.06f) },     // pommel
		{ TEXT("/Engine/BasicShapes/Cube.Cube"),         FVector(0.0f, 0.0f, 18.5f), FVector(0.05f, 0.22f, 0.03f) },     // guard
		{ TEXT("/Engine/BasicShapes/Cube.Cube"),         FVector(0.0f, 0.0f, 45.0f), FVector(0.012f, 0.05f, 0.5f) },     // blade
	};

	/** Tip of the blade, where the whip line starts */
	constexpr float SwordTipZ = 70.0f;
}

UEchoSwordWhipComponent::UEchoSwordWhipComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After movement, so the whip line follows this frame's position
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(true);

	WhipAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_Whip.IA_Whip")));
	WhipMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Input/IMC_Whip.IMC_Whip")));
	SwordMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Materials/MI_SpiritSword.MI_SpiritSword")));
	WhipLineMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Echo/Materials/MI_WhipLine.MI_WhipLine")));
	ReleaseSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Engine/VREditor/Sounds/VR_ungrab_Cue.VR_ungrab_Cue")));
	MissSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Engine/VREditor/Sounds/VR_click3_Cue.VR_click3_Cue")));
}

AOurLastEchoCharacter* UEchoSwordWhipComponent::GetCharacter() const
{
	return Cast<AOurLastEchoCharacter>(GetOwner());
}

UEchoCharacterMovementComponent* UEchoSwordWhipComponent::GetMovement() const
{
	const AOurLastEchoCharacter* Character = GetCharacter();
	return Character ? Cast<UEchoCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
}

bool UEchoSwordWhipComponent::CanUseWhip() const
{
	const AOurLastEchoCharacter* Character = GetCharacter();
	return Character && Character->GetRealm() == EEchoRealm::Spirit;
}

bool UEchoSwordWhipComponent::IsSwinging() const
{
	const UEchoCharacterMovementComponent* Movement = GetMovement();
	return Movement && Movement->IsSwinging();
}

bool UEchoSwordWhipComponent::IsWhipLineVisible() const
{
	return WhipLine && WhipLine->IsVisible();
}

void UEchoSwordWhipComponent::ToggleDebugDraw()
{
	bDebugDraw = !bDebugDraw;
	UE_LOG(LogOurLastEcho, Log, TEXT("Whip debug drawing %s"), bDebugDraw ? TEXT("on") : TEXT("off"));
}

void UEchoSwordWhipComponent::BeginPlay()
{
	Super::BeginPlay();

	// Bat's copy keeps ticking too, so the debug view can be drawn on his machine
	if (CanUseWhip() && GetNetMode() != NM_DedicatedServer)
	{
		BuildWhipMesh();
	}
}

void UEchoSwordWhipComponent::BuildWhipMesh()
{
	AOurLastEchoCharacter* Character = GetCharacter();

	SwordRoot = NewObject<USceneComponent>(Character, TEXT("SwordWhipRoot"));
	SwordRoot->SetupAttachment(Character->GetRootComponent());
	SwordRoot->RegisterComponent();
	SwordRoot->SetRelativeTransform(HolsterPlacement);

	UMaterialInterface* Material = SwordMaterial.LoadSynchronous();
	for (const FSwordPart& Part : SwordShape)
	{
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Character);
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Part.Mesh));
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetCastShadow(false);
		Mesh->SetRelativeTransform(FTransform(FRotator::ZeroRotator, Part.Location, Part.Scale));
		if (Material)
		{
			Mesh->SetMaterial(0, Material);
		}
		Mesh->SetupAttachment(SwordRoot);
		Mesh->RegisterComponent();
		SwordParts.Add(Mesh);
	}

	// The whip line: a thin cylinder stretched in world space from the blade tip to the anchor
	WhipLine = NewObject<UStaticMeshComponent>(Character, TEXT("WhipLine"));
	WhipLine->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	WhipLine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WhipLine->SetGenerateOverlapEvents(false);
	WhipLine->SetCanEverAffectNavigation(false);
	WhipLine->SetCastShadow(false);
	WhipLine->SetUsingAbsoluteLocation(true);
	WhipLine->SetUsingAbsoluteRotation(true);
	WhipLine->SetUsingAbsoluteScale(true);
	if (UMaterialInterface* LineMaterial = WhipLineMaterial.LoadSynchronous())
	{
		WhipLine->SetMaterial(0, LineMaterial);
	}
	WhipLine->SetupAttachment(Character->GetRootComponent());
	WhipLine->SetVisibility(false);
	WhipLine->RegisterComponent();
}

void UEchoSwordWhipComponent::SetupPlayerInput(UEnhancedInputComponent* Input)
{
	if (!Input || !CanUseWhip())
	{
		return;
	}

	UInputAction* Action = WhipAction.LoadSynchronous();
	if (!Action)
	{
		UE_LOG(LogOurLastEcho, Warning, TEXT("Sword whip: input action missing (run Scripts/build_sword_whip.py)"));
		return;
	}

	Input->BindAction(Action, ETriggerEvent::Started, this, &UEchoSwordWhipComponent::OnWhipPressed);
	Input->BindAction(Action, ETriggerEvent::Completed, this, &UEchoSwordWhipComponent::OnWhipReleased);
	Input->BindAction(Action, ETriggerEvent::Canceled, this, &UEchoSwordWhipComponent::OnWhipReleased);

	const APawn* Pawn = GetCharacter();
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr)
	{
		if (UInputMappingContext* Context = WhipMappingContext.LoadSynchronous())
		{
			Subsystem->AddMappingContext(Context, MappingPriority);
		}
	}
}

void UEchoSwordWhipComponent::OnWhipPressed()
{
	PressWhip();
}

void UEchoSwordWhipComponent::OnWhipReleased()
{
	ReleaseWhip();
}

bool UEchoSwordWhipComponent::PressWhip()
{
	AOurLastEchoCharacter* Character = GetCharacter();
	UEchoCharacterMovementComponent* Movement = GetMovement();
	if (!Character || !Movement || !CanUseWhip())
	{
		return false;
	}

	AEchoAnchorPoint* Target = HighlightedAnchor.Get();
	if (!Target)
	{
		Target = FindBestAnchor();
	}
	if (!Target)
	{
		if (USoundBase* Sound = MissSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, Character->GetActorLocation(), 0.5f, 0.8f);
		}
		return false;
	}

	// The movement component latches on its next update (and the server replays the same request)
	const FVector Point = Target->GetSwingPoint();
	Movement->RequestSwing(Point, FVector::Dist(Character->GetActorLocation(), Point));
	return true;
}

void UEchoSwordWhipComponent::ReleaseWhip()
{
	if (UEchoCharacterMovementComponent* Movement = GetMovement())
	{
		Movement->StopSwingRequest();
	}
}

void UEchoSwordWhipComponent::NotifySwingStarted(AEchoAnchorPoint* Anchor)
{
	LineExtend = 0.0f;
	SwingTrail.Reset();
	if (Anchor)
	{
		Anchor->PlayLatchCue();
	}

	if (GetOwner()->HasAuthority())
	{
		bLatched = true;
		LatchedAnchor = Anchor ? Anchor->GetSwingPoint() : FVector::ZeroVector;
		++LatchCount;
	}
}

void UEchoSwordWhipComponent::NotifySwingEnded(bool bLaunched)
{
	if (const UEchoCharacterMovementComponent* Movement = GetMovement())
	{
		LastReleasedAnchor = Movement->GetSwingAnchorActor();
	}
	LastReleaseTime = GetWorld()->GetTimeSeconds();

	if (GetOwner()->HasAuthority())
	{
		bLatched = false;
	}

	if (GetNetMode() != NM_DedicatedServer && bLaunched)
	{
		if (USoundBase* Sound = ReleaseSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, GetOwner()->GetActorLocation(), 0.6f, 1.2f);
		}
	}
}

void UEchoSwordWhipComponent::OnRep_LatchCount()
{
	// The other player's machine: lash the line out and play the snap at the anchor
	LineExtend = 0.0f;
	for (TActorIterator<AEchoAnchorPoint> It(GetWorld()); It; ++It)
	{
		if (FVector::DistSquared(It->GetSwingPoint(), LatchedAnchor) < FMath::Square(50.0f))
		{
			It->PlayLatchCue();
			break;
		}
	}
}

void UEchoSwordWhipComponent::GetView(FVector& OutLocation, FVector& OutDirection) const
{
	const AOurLastEchoCharacter* Character = GetCharacter();
	FRotator Rotation;
	if (const APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr)
	{
		PC->GetPlayerViewPoint(OutLocation, Rotation);
	}
	else if (Character)
	{
		OutLocation = Character->GetFollowCamera()->GetComponentLocation();
		Rotation = Character->GetFollowCamera()->GetComponentRotation();
	}
	OutDirection = Rotation.Vector();
}

AEchoAnchorPoint* UEchoSwordWhipComponent::FindBestAnchor() const
{
	const AOurLastEchoCharacter* Character = GetCharacter();
	if (!Character || !CanUseWhip())
	{
		return nullptr;
	}

	FVector ViewLocation;
	FVector ViewDirection;
	GetView(ViewLocation, ViewDirection);

	const FVector From = Character->GetActorLocation();
	const UEchoCharacterMovementComponent* Movement = GetMovement();
	const AEchoAnchorPoint* Current = Movement ? Movement->GetSwingAnchorActor() : nullptr;
	const bool bRecentlyReleased = GetWorld()->GetTimeSeconds() - LastReleaseTime < RelatchDelay;
	const float CosLimit = FMath::Cos(FMath::DegreesToRadians(TargetingAngle));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SwordWhipSight), false, Character);

	AEchoAnchorPoint* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (TActorIterator<AEchoAnchorPoint> It(GetWorld()); It; ++It)
	{
		AEchoAnchorPoint* Anchor = *It;
		if (!IsValid(Anchor) || Anchor == Current || (bRecentlyReleased && Anchor == LastReleasedAnchor.Get()))
		{
			continue;
		}

		const FVector Point = Anchor->GetSwingPoint();
		const float Distance = FVector::Dist(From, Point);
		if (Distance > WhipRange)
		{
			continue;
		}

		const float Cos = FVector::DotProduct((Point - ViewLocation).GetSafeNormal(), ViewDirection);
		if (Cos < CosLimit)
		{
			continue;
		}

		// Nothing solid between her and the anchor (stop just short of the surface it's stuck in)
		Params.ClearIgnoredSourceObjects();
		Params.AddIgnoredActor(Character);
		Params.AddIgnoredActor(Anchor);
		const FVector SightEnd = Point - (Point - From).GetSafeNormal() * 20.0f;
		if (GetWorld()->LineTraceTestByChannel(From, SightEnd, ECC_Visibility, Params))
		{
			continue;
		}

		// The nearest wins, with a nudge towards the centre of the view
		const float Angle = FMath::Acos(FMath::Clamp(Cos, -1.0f, 1.0f));
		const float Score = Distance / WhipRange + 0.35f * Angle / FMath::DegreesToRadians(TargetingAngle);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Anchor;
		}
	}
	return Best;
}

void UEchoSwordWhipComponent::UpdateTargeting()
{
	AEchoAnchorPoint* Best = FindBestAnchor();
	if (Best != HighlightedAnchor.Get())
	{
		if (AEchoAnchorPoint* Old = HighlightedAnchor.Get())
		{
			Old->SetHighlighted(false);
		}
		HighlightedAnchor = Best;
		if (Best)
		{
			Best->SetHighlighted(true);
		}
	}
}

void UEchoSwordWhipComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AOurLastEchoCharacter* Character = GetCharacter();
	if (!Character)
	{
		return;
	}

	if (CanUseWhip())
	{
		if (Character->IsLocallyControlled())
		{
			UpdateTargeting();
		}
		UpdateWhipLook(DeltaTime);

		if (IsSwinging())
		{
			SwingTrail.Add(Character->GetActorLocation());
			if (SwingTrail.Num() > 240)
			{
				SwingTrail.RemoveAt(0);
			}
		}
	}

	if (bDebugDraw && Character->IsLocallyControlled())
	{
		DrawDebug();
	}
}

void UEchoSwordWhipComponent::UpdateWhipLook(float DeltaTime)
{
	const AOurLastEchoCharacter* Character = GetCharacter();
	if (!SwordRoot || !WhipLine)
	{
		return;
	}

	// The owner and the server know the swing from movement; the other player's machine from replication
	bool bShow = false;
	FVector Anchor = FVector::ZeroVector;
	if (Character->GetLocalRole() == ROLE_SimulatedProxy)
	{
		bShow = bLatched;
		Anchor = LatchedAnchor;
	}
	else if (const UEchoCharacterMovementComponent* Movement = GetMovement())
	{
		bShow = Movement->IsSwinging();
		Anchor = Movement->GetSwingAnchor();
	}

	if (!bShow)
	{
		if (bLineShown)
		{
			bLineShown = false;
			WhipLine->SetVisibility(false);
			SwordRoot->SetRelativeTransform(HolsterPlacement);
		}
		return;
	}
	bLineShown = true;

	// Sword raised at the shoulder, pointing at the anchor; the line runs from its tip
	const FVector Hand = Character->GetActorTransform().TransformPosition(HeldLocation);
	const FVector ToAnchor = (Anchor - Hand).GetSafeNormal();
	SwordRoot->SetWorldLocationAndRotation(Hand, FRotationMatrix::MakeFromZ(ToAnchor).Rotator());

	LineExtend = FMath::Min(1.0f, LineExtend + DeltaTime / WhipExtendTime);
	const FVector Tip = Hand + ToAnchor * SwordTipZ;
	const FVector End = FMath::Lerp(Tip, Anchor, LineExtend);
	const FVector Span = End - Tip;
	const float Length = Span.Size();
	if (Length < 1.0f)
	{
		WhipLine->SetVisibility(false);
		return;
	}

	const float Width = WhipLineThickness / 100.0f;
	WhipLine->SetWorldLocationAndRotation((Tip + End) * 0.5f, FRotationMatrix::MakeFromZ(Span / Length).Rotator());
	WhipLine->SetWorldScale3D(FVector(Width, Width, Length / 100.0f));
	WhipLine->SetVisibility(true);
}

void UEchoSwordWhipComponent::PredictSwingPath(FVector Location, FVector Velocity, const FVector& Anchor, float Rope, float Duration, TArray<FVector>& OutPoints) const
{
	// The same pendulum as UEchoCharacterMovementComponent::PhysSwing, without collisions or input
	const float Step = 1.0f / 60.0f;
	const float GravityZ = GetWorld()->GetGravityZ() * SwingGravityScale;
	OutPoints.Add(Location);
	for (float Time = 0.0f; Time < Duration; Time += Step)
	{
		Velocity = (Velocity + FVector(0.0f, 0.0f, GravityZ * Step)).GetClampedToMaxSize(MaxSwingSpeed);
		FVector Next = Location + Velocity * Step;
		const FVector FromAnchor = Next - Anchor;
		const float Distance = FromAnchor.Size();
		if (Distance > Rope && Distance > KINDA_SMALL_NUMBER)
		{
			const FVector Dir = FromAnchor / Distance;
			Next = Anchor + Dir * Rope;
			const float Outward = FVector::DotProduct(Velocity, Dir);
			if (Outward > 0.0f)
			{
				Velocity -= Dir * Outward;
			}
		}
		Location = Next;
		if (OutPoints.Num() < 400 && FMath::Fmod(Time, 4.0f * Step) < Step)
		{
			OutPoints.Add(Location);
		}
	}
}

void UEchoSwordWhipComponent::DrawDebug()
{
	UWorld* World = GetWorld();
	const AOurLastEchoCharacter* Character = GetCharacter();
	const FVector From = Character->GetActorLocation();

	// Anchor targets (Bat's boards) and anchor points, on both players' machines
	for (TActorIterator<AEchoAnchorTarget> It(World); It; ++It)
	{
		const FVector Face = It->GetFaceCenter();
		DrawDebugCircle(World, Face, It->Diameter * 0.5f + 10.0f, 24, FColor::Orange, false, -1.0f, 0, 3.0f, It->GetActorRightVector(), It->GetActorUpVector(), false);
		DrawDebugString(World, Face + It->GetActorForwardVector() * 30.0f, TEXT("anchor target"), nullptr, FColor::Orange, 0.0f, true);
	}

	for (TActorIterator<AEchoAnchorPoint> It(World); It; ++It)
	{
		const FVector Point = It->GetSwingPoint();
		const bool bInRange = FVector::Dist(From, Point) <= WhipRange;
		const FColor Color = It->IsHighlighted() ? FColor::Yellow : (bInRange ? FColor::Green : FColor::Red);
		DrawDebugSphere(World, Point, 25.0f, 12, Color, false, -1.0f, 0, 2.0f);
	}

	if (!CanUseWhip())
	{
		return;
	}

	// Whip range and the targeting cone
	DrawDebugSphere(World, From, WhipRange, 32, FColor(80, 140, 255), false, -1.0f, 0, 1.0f);
	FVector ViewLocation;
	FVector ViewDirection;
	GetView(ViewLocation, ViewDirection);
	const float AngleRad = FMath::DegreesToRadians(TargetingAngle);
	DrawDebugCone(World, ViewLocation, ViewDirection, WhipRange, AngleRad, AngleRad, 24, FColor(120, 200, 255), false, -1.0f, 0, 1.0f);

	const UEchoCharacterMovementComponent* Movement = GetMovement();
	TArray<FVector> Arc;
	if (Movement && Movement->IsSwinging())
	{
		// The rope, the arc so far, and where the swing goes from here
		DrawDebugLine(World, From, Movement->GetSwingAnchor(), FColor::Cyan, false, -1.0f, 0, 2.0f);
		PredictSwingPath(From, Movement->Velocity, Movement->GetSwingAnchor(), Movement->GetSwingRopeLength(), 2.5f, Arc);
	}
	else if (const AEchoAnchorPoint* Target = HighlightedAnchor.Get())
	{
		// Where a latch right now would swing her (a rope the length of the current distance)
		const FVector Point = Target->GetSwingPoint();
		DrawDebugLine(World, From, Point, FColor::Yellow, false, -1.0f, 0, 1.0f);
		FVector Velocity = Movement ? Movement->Velocity : FVector::ZeroVector;
		Velocity += (Point - From).GetSafeNormal2D() * LatchBoost;
		PredictSwingPath(From, Velocity, Point, FMath::Max(MinRopeLength, FVector::Dist(From, Point)), 2.5f, Arc);
	}
	for (int32 Index = 1; Index < Arc.Num(); ++Index)
	{
		DrawDebugLine(World, Arc[Index - 1], Arc[Index], FColor(255, 120, 255), false, -1.0f, 0, 2.0f);
	}
	for (int32 Index = 1; Index < SwingTrail.Num(); ++Index)
	{
		DrawDebugLine(World, SwingTrail[Index - 1], SwingTrail[Index], FColor::White, false, -1.0f, 0, 1.5f);
	}
}

void UEchoSwordWhipComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UEchoSwordWhipComponent, bLatched, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UEchoSwordWhipComponent, LatchedAnchor, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UEchoSwordWhipComponent, LatchCount, COND_SkipOwner);
}
