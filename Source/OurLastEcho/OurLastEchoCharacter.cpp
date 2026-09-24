// Copyright Epic Games, Inc. All Rights Reserved.

#include "OurLastEchoCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "OurLastEcho.h"
#include "EchoGameState.h"
#include "EchoSpiritBowComponent.h"

AOurLastEchoCharacter::AOurLastEchoCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	SpiritBow = CreateDefaultSubobject<UEchoSpiritBowComponent>(TEXT("SpiritBow"));

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character)
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AOurLastEchoCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (Realm == EEchoRealm::Spirit)
	{
		// Spirit capsules use their own object channel so spirit platforms can block them and nothing else.
		// This runs identically on server and clients, so movement prediction stays in agreement.
		UCapsuleComponent* Capsule = GetCapsuleComponent();
		Capsule->SetCollisionObjectType(ECC_SpiritPawn);

		// Saraa exists in the past: she and Bat pass through each other
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
}

void AOurLastEchoCharacter::BeginPlay()
{
	Super::BeginPlay();

	RespawnTransform = GetActorTransform();

	if (GhostMaterial)
	{
		USkeletalMeshComponent* MeshComp = GetMesh();
		for (int32 Index = 0; Index < MeshComp->GetNumMaterials(); ++Index)
		{
			MeshComp->SetMaterial(Index, GhostMaterial);
		}
		MeshComp->SetCastShadow(false);
	}
}

void AOurLastEchoCharacter::RespawnAtStart()
{
	if (!HasAuthority())
	{
		return;
	}

	GetCharacterMovement()->StopMovementImmediately();
	TeleportTo(RespawnTransform.GetLocation(), RespawnTransform.Rotator(), false, true);
}

void AOurLastEchoCharacter::SetRespawnTransform(const FTransform& NewRespawnTransform)
{
	if (HasAuthority())
	{
		RespawnTransform = NewRespawnTransform;
	}
}

void AOurLastEchoCharacter::EchoShowAllPlatforms()
{
	const AEchoGameState* GameState = GetWorld()->GetGameState<AEchoGameState>();
	RequestShowAllPlatforms(!(GameState && GameState->IsDebugShowAllPlatforms()));
}

void AOurLastEchoCharacter::RequestShowAllPlatforms(bool bShow)
{
	if (HasAuthority())
	{
		ServerSetShowAllPlatforms_Implementation(bShow);
	}
	else
	{
		ServerSetShowAllPlatforms(bShow);
	}
}

void AOurLastEchoCharacter::ServerSetShowAllPlatforms_Implementation(bool bShow)
{
	if (AEchoGameState* GameState = GetWorld()->GetGameState<AEchoGameState>())
	{
		GameState->SetDebugShowAllPlatforms(bShow);
	}
}

void AOurLastEchoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AOurLastEchoCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AOurLastEchoCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AOurLastEchoCharacter::Look);

		// Spirit bow (only binds for Bat)
		SpiritBow->SetupPlayerInput(EnhancedInputComponent);
	}
	else
	{
		UE_LOG(LogOurLastEcho, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AOurLastEchoCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AOurLastEchoCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AOurLastEchoCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AOurLastEchoCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AOurLastEchoCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AOurLastEchoCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}
