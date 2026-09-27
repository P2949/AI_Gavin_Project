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

void UPlayerLocomotionComponent::SetBlockingState(
	bool bNewBlocking)
{
	if (bBlocking == bNewBlocking)
	{
		return;
	}

	bBlocking = bNewBlocking;

	RefreshMovementSpeed();
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

	RefreshMovementSpeed();
}

void UPlayerLocomotionComponent::RefreshMovementSpeed()
{
	UCharacterMovementComponent *Movement =
		ResolveCharacterMovement();

	if (!Movement)
	{
		return;
	}

	float ResolvedWalkSpeed =
		LocomotionSettings.WalkSpeed;

	if (bBlocking)
	{
		ResolvedWalkSpeed *=
			LocomotionSettings.BlockingMovementSpeedMultiplier;
	}

	Movement->MaxWalkSpeed =
		FMath::Max(ResolvedWalkSpeed, 0.0f);
}
