// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/MainPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Combat/Health/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "EnhancedInputComponent.h"
#include "InputActionValue.h"

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

	// Health.
	HealthComponent =
		CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

	// First-person camera.
	FirstPersonCamera =
		CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));

	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());

	FirstPersonCamera->SetRelativeLocation(
		FVector(0.0f, 0.0f, BaseEyeHeight)
	);

	FirstPersonCamera->bUsePawnControlRotation = true;
}

// Called to bind player input
void AMainPlayerCharacter::SetupPlayerInputComponent(
	UInputComponent* PlayerInputComponent
)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent =
		Cast<UEnhancedInputComponent>(PlayerInputComponent);

	if (!EnhancedInputComponent)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("MainPlayerCharacter requires an EnhancedInputComponent.")
		);

		return;
	}

	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(
			MoveAction,
			ETriggerEvent::Triggered,
			this,
			&AMainPlayerCharacter::Move
		);
	}

	if (LookAction)
	{
		EnhancedInputComponent->BindAction(
			LookAction,
			ETriggerEvent::Triggered,
			this,
			&AMainPlayerCharacter::Look
		);
	}

	if (JumpAction)
	{
		EnhancedInputComponent->BindAction(
			JumpAction,
			ETriggerEvent::Started,
			this,
			&ACharacter::Jump
		);

		EnhancedInputComponent->BindAction(
			JumpAction,
			ETriggerEvent::Completed,
			this,
			&ACharacter::StopJumping
		);
	}
}

void AMainPlayerCharacter::Move(const FInputActionValue& Value)
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
		0.0f
	);

	const FVector ForwardDirection =
		FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

	const FVector RightDirection =
		FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(
		ForwardDirection,
		MovementInput.Y
	);

	AddMovementInput(
		RightDirection,
		MovementInput.X
	);
}

void AMainPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookInput = Value.Get<FVector2D>();

	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
}
