#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RoomResettable.generated.h"

/**
 * Marks an actor as participating in room reset lifecycle.
 *
 * The room owns when a reset happens and which transform should be
 * restored. The participant owns how its internal gameplay state is
 * restored.
 */
UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class AI_GAVIN_PROJECT_API URoomResettable : public UInterface
{
    GENERATED_BODY()
};

/**
 * Native room-reset contract.
 *
 * Kept intentionally small so unrelated actor types can participate
 * without inheriting from a shared gameplay base class.
 */
class AI_GAVIN_PROJECT_API IRoomResettable
{
    GENERATED_BODY()

public:
    virtual void ResetForRoom(
        const FTransform &ResetTransform) = 0;
};
