#pragma once

#include "CoreMinimal.h"
#include "PlayerLocomotionTypes.generated.h"

/**
 * Baseline tuning for the main player's locomotion.
 *
 * These values describe locomotion itself rather than input or combat.
 * Additional locomotion states can extend this structure when they
 * become real gameplay requirements.
 */
USTRUCT(BlueprintType)
struct AI_GAVIN_PROJECT_API FPlayerLocomotionSettings
{
	GENERATED_BODY()

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Locomotion",
		meta = (ClampMin = "0.0", Units = "cm/s"))
	float WalkSpeed = 500.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Locomotion",
		meta = (ClampMin = "0.0", Units = "cm/s"))
	float JumpZVelocity = 500.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Locomotion",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirControl = 0.35f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Locomotion|Blocking",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlockingMovementSpeedMultiplier = 0.65f;
};
