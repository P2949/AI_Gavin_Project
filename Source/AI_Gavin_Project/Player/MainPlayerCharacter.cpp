// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/MainPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "EnhancedInputComponent.h"
#include "InputActionValue.h"

#include "Combat/Attack/MeleeAttackComponent.h"
#include "Combat/Defense/DefenseComponent.h"
#include "Components/SceneComponent.h"

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

	// Basic movement defaults.
	GetCharacterMovement()->MaxWalkSpeed = 500.0f;
	GetCharacterMovement()->JumpZVelocity = 500.0f;
	GetCharacterMovement()->AirControl = 0.35f;

	// First-person camera.
	FirstPersonCamera =
		CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));

	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());

	FirstPersonCamera->SetRelativeLocation(
		FVector(0.0f, 0.0f, BaseEyeHeight));

	FirstPersonCamera->bUsePawnControlRotation = true;

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

	if (UDefenseComponent *Defense = GetDefenseComponent())
	{
		Defense->OnBlockingChanged.AddDynamic(
			this,
			&AMainPlayerCharacter::HandleBlockingChanged);
	}

	if (MeleeAttackComponent)
	{
		MeleeAttackComponent->OnAttackStateChanged.AddDynamic(
			this,
			&AMainPlayerCharacter::HandleAttackStateChanged);
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
			LogTemp,
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

void AMainPlayerCharacter::StartAttack()
{
	if (MeleeAttackComponent)
	{
		MeleeAttackComponent->TryStartAttack();
	}
}

void AMainPlayerCharacter::StartBlocking()
{
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
	UCharacterMovementComponent *Movement =
		GetCharacterMovement();

	if (!Movement)
	{
		return;
	}

	if (bIsBlocking)
	{
		WalkSpeedBeforeBlocking = Movement->MaxWalkSpeed;

		Movement->MaxWalkSpeed =
			WalkSpeedBeforeBlocking * BlockingMovementSpeedMultiplier;

		return;
	}

	if (WalkSpeedBeforeBlocking > 0.0f)
	{
		Movement->MaxWalkSpeed = WalkSpeedBeforeBlocking;
		WalkSpeedBeforeBlocking = 0.0f;
	}
}
