#pragma once

#include "CoreMinimal.h"

#include "PedroObservationState.generated.h"

USTRUCT(BlueprintType)
struct FPedroObservationState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 InteractionCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 UniqueInteractionCount = 0;

    void Reset()
    {
        InteractionCount = 0;
        UniqueInteractionCount = 0;
    }
};
