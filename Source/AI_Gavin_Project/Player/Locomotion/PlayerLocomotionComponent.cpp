#include "Player/Locomotion/PlayerLocomotionComponent.h"

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

void UPlayerLocomotionComponent::ResetTransientState()
{
	SetSprintRequested(false);
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

UCharacterMovementComponent *
UPlayerLocomotionComponent::ResolveCharacterMovement() const
{
	const ACharacter *CharacterOwner =
		Cast<ACharacter>(GetOwner());

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
			LogTemp,
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

	RefreshLocomotionState();
}

bool UPlayerLocomotionComponent::CanSprint() const
{
	return bSprintRequested &&
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

	bIsSprinting = CanSprint();

	float ResolvedWalkSpeed =
		bIsSprinting
			? LocomotionSettings.SprintSpeed
			: LocomotionSettings.WalkSpeed;

	if (bBlocking)
	{
		ResolvedWalkSpeed *=
			LocomotionSettings.BlockingMovementSpeedMultiplier;
	}

	Movement->MaxWalkSpeed =
		FMath::Max(ResolvedWalkSpeed, 0.0f);
}
