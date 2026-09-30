#include "Player/Locomotion/PlayerLocomotionComponent.h"

#include "AI_Gavin_Project.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UPlayerLocomotionComponent::UPlayerLocomotionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPlayerLocomotionComponent::BeginPlay()
{
	Super::BeginPlay();

	ApplyBaseMovementSettings();
}

void UPlayerLocomotionComponent::SetSprintRequested(
	bool bRequested)
{
	if (bSprintRequested == bRequested)
	{
		return;
	}

	bSprintRequested = bRequested;

	RefreshLocomotionState();
}

bool UPlayerLocomotionComponent::IsSprinting() const
{
	const UCharacterMovementComponent *Movement =
		ResolveCharacterMovement();

	return Movement &&
		CanSprint() &&
		!Movement->IsCrouching();
}

void UPlayerLocomotionComponent::SetCrouchRequested(
	bool bRequested)
{
	if (bCrouchRequested == bRequested)
	{
		return;
	}

	bCrouchRequested = bRequested;

	if (ACharacter *CharacterOwner = ResolveCharacterOwner())
	{
		if (bCrouchRequested)
		{
			CharacterOwner->Crouch();
		}
		else
		{
			CharacterOwner->UnCrouch();
		}
	}

	RefreshLocomotionState();
}

void UPlayerLocomotionComponent::ResetTransientState()
{
	SetSprintRequested(false);
	SetCrouchRequested(false);
}

void UPlayerLocomotionComponent::SetBlockingState(
	bool bNewBlocking)
{
	if (bBlocking == bNewBlocking)
	{
		return;
	}

	bBlocking = bNewBlocking;

	RefreshLocomotionState();
}

ACharacter *
UPlayerLocomotionComponent::ResolveCharacterOwner() const
{
	return Cast<ACharacter>(GetOwner());
}

UCharacterMovementComponent *
UPlayerLocomotionComponent::ResolveCharacterMovement() const
{
	ACharacter *CharacterOwner =
		ResolveCharacterOwner();

	return CharacterOwner
		? CharacterOwner->GetCharacterMovement()
		: nullptr;
}

void UPlayerLocomotionComponent::ApplyBaseMovementSettings()
{
	UCharacterMovementComponent *Movement =
		ResolveCharacterMovement();

	if (!Movement)
	{
		UE_LOG(
			LogAI_Gavin_Project,
			Error,
			TEXT(
				"PlayerLocomotionComponent on '%s' requires "
				"an ACharacter owner with CharacterMovement."),
			*GetNameSafe(GetOwner()));

		return;
	}

	Movement->JumpZVelocity =
		LocomotionSettings.JumpZVelocity;

	Movement->AirControl =
		LocomotionSettings.AirControl;

	Movement->GetNavAgentPropertiesRef().bCanCrouch = true;

	RefreshLocomotionState();
}

bool UPlayerLocomotionComponent::CanSprint() const
{
	return bSprintRequested &&
		!bCrouchRequested &&
		!bBlocking;
}

void UPlayerLocomotionComponent::RefreshLocomotionState()
{
	UCharacterMovementComponent *Movement =
		ResolveCharacterMovement();

	if (!Movement)
	{
		return;
	}

	const bool bUseSprintSpeed =
		CanSprint();

	float ResolvedWalkSpeed =
		bUseSprintSpeed
			? LocomotionSettings.SprintSpeed
			: LocomotionSettings.WalkSpeed;

	float ResolvedCrouchSpeed =
		LocomotionSettings.CrouchSpeed;

	if (bBlocking)
	{
		const float BlockingMultiplier =
			LocomotionSettings.BlockingMovementSpeedMultiplier;

		ResolvedWalkSpeed *= BlockingMultiplier;
		ResolvedCrouchSpeed *= BlockingMultiplier;
	}

	Movement->MaxWalkSpeed =
		FMath::Max(ResolvedWalkSpeed, 0.0f);

	Movement->MaxWalkSpeedCrouched =
		FMath::Max(ResolvedCrouchSpeed, 0.0f);
}
