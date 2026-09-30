#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "RoomResettable.generated.h"

/**
 * Capability contract for actors that participate in room reset lifecycle.
 *
 * The room owns when a reset happens and which transform should be
 * restored. The participant owns how its internal gameplay state is
 * restored.
 *
 * The contract is native C++ but may also be implemented by
 * encounter-local Blueprint classes.
 */
UINTERFACE(BlueprintType, Blueprintable)
class AI_GAVIN_PROJECT_API URoomResettable : public UInterface
{
    GENERATED_BODY()
};

/**
 * Room-reset capability shared by otherwise unrelated actor types.
 *
 * Kept intentionally small so reset participation does not require
 * inheriting from a shared gameplay base class.
 */
class AI_GAVIN_PROJECT_API IRoomResettable
{
    GENERATED_BODY()

public:
    /**
     * Restores this participant for a new attempt using the transform
     * supplied by the room lifecycle.
     *
     * Implementations own restoration of all participant-specific state.
     */
    UFUNCTION(
        BlueprintNativeEvent,
        Category = "Room|Reset")
    void ResetForRoom(const FTransform &ResetTransform);
};
