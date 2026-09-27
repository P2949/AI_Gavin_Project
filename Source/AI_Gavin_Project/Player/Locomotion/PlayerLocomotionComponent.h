#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Player/Locomotion/PlayerLocomotionTypes.h"
#include "PlayerLocomotionComponent.generated.h"

class UCharacterMovementComponent;

/**
 * Owns locomotion tuning and movement-speed policy for the main player.
 *
 * Other gameplay systems communicate relevant state to this component
 * instead of directly saving, restoring, or composing movement speeds.
 *
 * The component deliberately relies on Unreal's CharacterMovement
 * implementation rather than replacing movement physics.
 */
UCLASS(ClassGroup = (Player))
class AI_GAVIN_PROJECT_API UPlayerLocomotionComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerLocomotionComponent();

	/**
	 * Records whether sprint is currently requested by the player.
	 *
	 * A request may remain active while another state, such as blocking,
	 * temporarily prevents sprinting.
	 */
	void SetSprintRequested(bool bRequested);

	/**
	 * Clears player-owned transient locomotion intent.
	 *
	 * Used by lifecycle transitions such as death and room reset so
	 * held input state cannot leak into a restored player.
	 */
	void ResetTransientState();

	UFUNCTION(BlueprintPure, Category = "Player|Locomotion")
	bool IsSprintRequested() const
	{
		return bSprintRequested;
	}

	UFUNCTION(BlueprintPure, Category = "Player|Locomotion")
	bool IsSprinting() const
	{
		return bIsSprinting;
	}

	/**
	 * Updates the movement consequence of the player's combat blocking
	 * state.
	 *
	 * Defense remains the source of truth for whether blocking is active.
	 */
	void SetBlockingState(bool bNewBlocking);

protected:
	virtual void BeginPlay() override;

	/**
	 * Shared locomotion tuning.
	 *
	 * Native defaults remain the project baseline while derived player
	 * Blueprints may override values for cheap gameplay experimentation.
	 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Locomotion")
	FPlayerLocomotionSettings LocomotionSettings;

private:
	UCharacterMovementComponent *ResolveCharacterMovement() const;

	void ApplyBaseMovementSettings();
	void RefreshLocomotionState();

	bool CanSprint() const;

	bool bSprintRequested = false;
	bool bIsSprinting = false;
	bool bBlocking = false;
};
