#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Interactable.generated.h"

/**
 * Capability contract for objects that can receive an interaction request.
 *
 * The interaction system owns target acquisition and request dispatch.
 * The target owns what interaction actually means.
 *
 * This interface deliberately contains no assumptions about dialogue,
 * collectables, encounters, room completion, persistence, or AI.
 */
UINTERFACE(BlueprintType, Blueprintable)
class AI_GAVIN_PROJECT_API UInteractable : public UInterface
{
    GENERATED_BODY()
};

/**
 * Native interaction contract that can also be implemented by
 * encounter-local Blueprint classes.
 */
class AI_GAVIN_PROJECT_API IInteractable
{
    GENERATED_BODY()

public:
    /**
     * Handles an interaction request from the provided actor.
     *
     * Implementations own all target-specific interaction behaviour.
     */
    UFUNCTION(
        BlueprintCallable,
        BlueprintNativeEvent,
        Category = "Interaction")
    void Interact(AActor *Interactor);
};
