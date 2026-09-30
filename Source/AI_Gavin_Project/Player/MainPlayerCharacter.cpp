// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/MainPlayerCharacter.h"

#include "AI_Gavin_Project.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "EnhancedInputComponent.h"
#include "InputActionValue.h"

#include "Combat/Attack/MeleeAttackComponent.h"
#include "Combat/Defense/DefenseComponent.h"
#include "Combat/Health/HealthComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Controller.h"
#include "Player/Interaction/PlayerInteractionComponent.h"
#include "Player/Locomotion/PlayerLocomotionComponent.h"

// Sets default values
AMainPlayerCharacter::AMainPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// First-person character follows controller yaw.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Do not automatically rotate toward movement direction.
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// Player locomotion policy owns movement tuning and speed resolution.
	LocomotionComponent =
		CreateDefaultSubobject<UPlayerLocomotionComponent>(
			TEXT("LocomotionComponent"));

	// First-person camera.
	FirstPersonCamera =
		CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));

	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());

	FirstPersonCamera->SetRelativeLocation(
		FVector(0.0f, 0.0f, BaseEyeHeight));

	FirstPersonCamera->bUsePawnControlRotation = true;

	// Player interactions originate from the first-person view.
	InteractionComponent =
		CreateDefaultSubobject<UPlayerInteractionComponent>(
			TEXT("InteractionComponent"));

	InteractionComponent->SetInteractionOriginComponent(
		FirstPersonCamera);

	// Player melee attacks originate from the first-person view.
	MeleeAttackOrigin =
		CreateDefaultSubobject<USceneComponent>(TEXT("MeleeAttackOrigin"));

	MeleeAttackOrigin->SetupAttachment(FirstPersonCamera);

	// Reusable attack mechanics.
	MeleeAttackComponent =
		CreateDefaultSubobject<UMeleeAttackComponent>(
			TEXT("MeleeAttackComponent"));

	MeleeAttackComponent->SetAttackOriginComponent(MeleeAttackOrigin);
}

void AMainPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UHealthComponent *Health = GetHealthComponent())
	{
		Health->OnDeath.AddDynamic(
			this,
			&AMainPlayerCharacter::HandleDeath);
	}

	if (UDefenseComponent *Defense = GetDefenseComponent())
	{
		Defense->OnBlockingChanged.AddDynamic(
			this,
			&AMainPlayerCharacter::HandleBlockingChanged);

		if (LocomotionComponent)
		{
			LocomotionComponent->SetBlockingState(
				Defense->IsBlocking());
		}
	}

	if (MeleeAttackComponent)
	{
		MeleeAttackComponent->OnAttackStateChanged.AddDynamic(
			this,
			&AMainPlayerCharacter::HandleAttackStateChanged);
	}
}

void AMainPlayerCharacter::ResetForRoom_Implementation(
	const FTransform &ResetTransform)
{
	if (LocomotionComponent)
	{
		LocomotionComponent->ResetTransientState();
	}

	// Clear held block input before cancelling an attack so returning
	// to Ready cannot cause HandleAttackStateChanged to restore blocking.
	bBlockInputHeld = false;

	if (MeleeAttackComponent)
	{
		MeleeAttackComponent->CancelAttack();
	}

	if (UDefenseComponent *Defense = GetDefenseComponent())
	{
		Defense->SetBlocking(false);
	}

	StopJumping();
	ConsumeMovementInputVector();

	UCharacterMovementComponent *Movement =
		GetCharacterMovement();

	if (Movement)
	{
		Movement->StopMovementImmediately();
		Movement->ClearAccumulatedForces();
	}

	SetActorTransform(
		ResetTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	if (Controller)
	{
		Controller->SetControlRotation(
			ResetTransform.Rotator());
	}

	if (Movement)
	{
		Movement->SetMovementMode(MOVE_Walking);
		Movement->StopMovementImmediately();
	}

	if (UHealthComponent *Health = GetHealthComponent())
	{
		Health->ResetHealth();
	}
}

// Called to bind player input
void AMainPlayerCharacter::SetupPlayerInputComponent(
	UInputComponent *PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent *EnhancedInputComponent =
		Cast<UEnhancedInputComponent>(PlayerInputComponent);

	if (!EnhancedInputComponent)
	{
		UE_LOG(
			LogAI_Gavin_Project,
			Error,
			TEXT("MainPlayerCharacter requires an EnhancedInputComponent."));

		return;
	}

	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(
			MoveAction,
			ETriggerEvent::Triggered,
			this,
			&AMainPlayerCharacter::Move);
	}

	if (LookAction)
	{
		EnhancedInputComponent->BindAction(
			LookAction,
			ETriggerEvent::Triggered,
			this,
			&AMainPlayerCharacter::Look);
	}

	if (JumpAction)
	{
		EnhancedInputComponent->BindAction(
			JumpAction,
			ETriggerEvent::Started,
			this,
			&ACharacter::Jump);

		EnhancedInputComponent->BindAction(
			JumpAction,
			ETriggerEvent::Completed,
			this,
			&ACharacter::StopJumping);
	}

	if (SprintAction)
	{
		EnhancedInputComponent->BindAction(
			SprintAction,
			ETriggerEvent::Started,
			this,
			&AMainPlayerCharacter::StartSprinting);

		EnhancedInputComponent->BindAction(
			SprintAction,
			ETriggerEvent::Completed,
			this,
			&AMainPlayerCharacter::StopSprinting);

		EnhancedInputComponent->BindAction(
			SprintAction,
			ETriggerEvent::Canceled,
			this,
			&AMainPlayerCharacter::StopSprinting);
	}

	if (CrouchAction)
	{
		EnhancedInputComponent->BindAction(
			CrouchAction,
			ETriggerEvent::Started,
			this,
			&AMainPlayerCharacter::StartCrouching);

		EnhancedInputComponent->BindAction(
			CrouchAction,
			ETriggerEvent::Completed,
			this,
			&AMainPlayerCharacter::StopCrouching);

		EnhancedInputComponent->BindAction(
			CrouchAction,
			ETriggerEvent::Canceled,
			this,
			&AMainPlayerCharacter::StopCrouching);
	}

	if (AttackAction)
	{
		EnhancedInputComponent->BindAction(
			AttackAction,
			ETriggerEvent::Started,
			this,
			&AMainPlayerCharacter::StartAttack);
	}

	if (BlockAction)
	{
		EnhancedInputComponent->BindAction(
			BlockAction,
			ETriggerEvent::Started,
			this,
			&AMainPlayerCharacter::StartBlocking);

		EnhancedInputComponent->BindAction(
			BlockAction,
			ETriggerEvent::Completed,
			this,
			&AMainPlayerCharacter::StopBlocking);

		EnhancedInputComponent->BindAction(
			BlockAction,
			ETriggerEvent::Canceled,
			this,
			&AMainPlayerCharacter::StopBlocking);
	}

	if (InteractAction)
	{
		EnhancedInputComponent->BindAction(
			InteractAction,
			ETriggerEvent::Started,
			this,
			&AMainPlayerCharacter::StartInteraction);
	}
}

void AMainPlayerCharacter::Move(const FInputActionValue &Value)
{
	const FVector2D MovementInput = Value.Get<FVector2D>();

	if (!Controller)
	{
		return;
	}

	const FRotator ControlRotation = Controller->GetControlRotation();

	const FRotator YawRotation(
		0.0f,
		ControlRotation.Yaw,
		0.0f);

	const FVector ForwardDirection =
		FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

	const FVector RightDirection =
		FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(
		ForwardDirection,
		MovementInput.Y);

	AddMovementInput(
		RightDirection,
		MovementInput.X);
}

void AMainPlayerCharacter::Look(const FInputActionValue &Value)
{
	const FVector2D LookInput = Value.Get<FVector2D>();

	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
}

void AMainPlayerCharacter::HandleDeath(
	AActor *DamageCauser)
{
	static_cast<void>(DamageCauser);

	if (LocomotionComponent)
	{
		LocomotionComponent->ResetTransientState();
	}

	// Clear held block input before cancelling the attack so returning
	// to Ready cannot re-enable blocking through HandleAttackStateChanged.
	bBlockInputHeld = false;

	if (MeleeAttackComponent)
	{
		MeleeAttackComponent->CancelAttack();
	}

	if (UDefenseComponent *Defense = GetDefenseComponent())
	{
		Defense->SetBlocking(false);
	}

	StopJumping();
	ConsumeMovementInputVector();

	if (UCharacterMovementComponent *Movement =
			GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->ClearAccumulatedForces();
		Movement->DisableMovement();
	}
}

void AMainPlayerCharacter::StartAttack()
{
	const UHealthComponent *Health =
		GetHealthComponent();

	if ((Health && Health->IsDead()) ||
		!MeleeAttackComponent)
	{
		return;
	}

	MeleeAttackComponent->TryStartAttack();
}

void AMainPlayerCharacter::StartInteraction()
{
	const UHealthComponent *Health =
		GetHealthComponent();

	if ((Health && Health->IsDead()) ||
		!InteractionComponent)
	{
		return;
	}

	InteractionComponent->TryInteract();
}

void AMainPlayerCharacter::StartSprinting()
{
	const UHealthComponent *Health =
		GetHealthComponent();

	if ((Health && Health->IsDead()) ||
		!LocomotionComponent)
	{
		return;
	}

	LocomotionComponent->SetSprintRequested(true);
}

void AMainPlayerCharacter::StopSprinting()
{
	if (LocomotionComponent)
	{
		LocomotionComponent->SetSprintRequested(false);
	}
}

void AMainPlayerCharacter::StartCrouching()
{
	const UHealthComponent *Health =
		GetHealthComponent();

	if ((Health && Health->IsDead()) ||
		!LocomotionComponent)
	{
		return;
	}

	LocomotionComponent->SetCrouchRequested(true);
}

void AMainPlayerCharacter::StopCrouching()
{
	if (LocomotionComponent)
	{
		LocomotionComponent->SetCrouchRequested(false);
	}
}

void AMainPlayerCharacter::StartBlocking()
{
	const UHealthComponent *Health =
		GetHealthComponent();

	if (Health && Health->IsDead())
	{
		return;
	}

	bBlockInputHeld = true;

	if (MeleeAttackComponent &&
		MeleeAttackComponent->IsAttackInProgress())
	{
		return;
	}

	if (UDefenseComponent *Defense = GetDefenseComponent())
	{
		Defense->SetBlocking(true);
	}
}

void AMainPlayerCharacter::StopBlocking()
{
	bBlockInputHeld = false;

	if (UDefenseComponent *Defense = GetDefenseComponent())
	{
		Defense->SetBlocking(false);
	}
}

void AMainPlayerCharacter::HandleAttackStateChanged(
	EMeleeAttackState OldState,
	EMeleeAttackState NewState)
{
	static_cast<void>(OldState);

	UDefenseComponent *Defense = GetDefenseComponent();

	if (!Defense)
	{
		return;
	}

	if (NewState == EMeleeAttackState::Windup)
	{
		Defense->SetBlocking(false);
		return;
	}

	if (NewState == EMeleeAttackState::Ready &&
		bBlockInputHeld)
	{
		Defense->SetBlocking(true);
	}
}

void AMainPlayerCharacter::HandleBlockingChanged(bool bIsBlocking)
{
	if (LocomotionComponent)
	{
		LocomotionComponent->SetBlockingState(
			bIsBlocking);
	}
}
