#pragma once

#include "CoreMinimal.h"
#include "RoomLifecycleTypes.generated.h"

/**
 * High-level lifecycle state for one gameplay room.
 *
 * Reset is intentionally not represented as a separate state. A room
 * remains Active while its participants are being restored.
 */
UENUM(BlueprintType)
enum class ERoomLifecycleState : uint8
{
    Inactive,
    Active,
    Completed
};
