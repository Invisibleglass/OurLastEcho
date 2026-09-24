// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "EchoTypes.h"
#include "OurLastEchoCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UMaterialInterface;
class UEchoSpiritBowComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AOurLastEchoCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** Spirit bow. Every character has one, but only a Living-realm character (Bat) can use it; tune it on BP_ThirdPersonCharacter */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEchoSpiritBowComponent> SpiritBow;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Which realm this character lives in. Set per Blueprint: BP_ThirdPersonCharacter = Living (Bat), BP_Saraa = Spirit */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Echo")
	EEchoRealm Realm = EEchoRealm::Living;

	/** If set, replaces every material slot on the mesh (Saraa's ghost look) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Echo")
	TObjectPtr<UMaterialInterface> GhostMaterial;

	/** Where this character spawned; used by RespawnAtStart (server only) */
	FTransform RespawnTransform;

public:

	/** Constructor */
	AOurLastEchoCharacter();

	/** Returns which realm this character lives in */
	UFUNCTION(BlueprintPure, Category="Echo")
	EEchoRealm GetRealm() const { return Realm; }

	/** Teleports the character back to its respawn point (its spawn, or the last checkpoint). Server only; movement replication corrects the client */
	void RespawnAtStart();

	/** Server: where RespawnAtStart sends this character from now on (used by AEchoCheckpoint) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Echo")
	void SetRespawnTransform(const FTransform& NewRespawnTransform);

	UFUNCTION(BlueprintPure, Category="Echo")
	FTransform GetRespawnTransform() const { return RespawnTransform; }

	/** Debug console command: toggles showing every spirit and echo platform to BOTH players */
	UFUNCTION(Exec)
	void EchoShowAllPlatforms();

	/** Turns the show-all-platforms debug view on or off for both players (asks the server if called on a client) */
	UFUNCTION(BlueprintCallable, Category="Echo|Debug")
	void RequestShowAllPlatforms(bool bShow);

protected:

	UFUNCTION(Server, Reliable)
	void ServerSetShowAllPlatforms(bool bShow);

	/** Applies realm-specific collision once components exist */
	virtual void PostInitializeComponents() override;

	/** Records the spawn point and applies the ghost material */
	virtual void BeginPlay() override;

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** Returns the spirit bow component */
	FORCEINLINE UEchoSpiritBowComponent* GetSpiritBow() const { return SpiritBow; }
};

