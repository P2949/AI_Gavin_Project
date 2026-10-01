#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"

#include "PlayerInteractionComponent.generated.h"

class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnInteractionDispatchedSignature,
    AActor *,
    InteractionTarget,
    AActor *,
    Interactor);

/**
 * Owns player-side interaction target acquisition and dispatch.
 *
 * The component does not own input bindings or target-specific behaviour.
 * An owning actor supplies the interaction origin and decides when
 * TryInteract() should be called.
 */
UCLASS(
    ClassGroup = (Player),
    meta = (BlueprintSpawnableComponent))
class AI_GAVIN_PROJECT_API UPlayerInteractionComponent
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UPlayerInteractionComponent();

    /**
     * Sets the component whose position and forward vector define
     * interaction traces.
     */
    void SetInteractionOriginComponent(
        USceneComponent *NewOriginComponent);

    /**
     * Attempts to interact with the first valid interactable hit by
     * the configured trace.
     *
     * Returns true when an interaction request was dispatched.
     */
    bool TryInteract();

    /**
     * Broadcast when this component dispatches an interaction request to an
     * interactable actor.
     *
     * This reports the player interaction action; it does not imply that the
     * target accepted the interaction or changed state.
     */
    UPROPERTY(
        BlueprintAssignable,
        Category = "Interaction")
    FOnInteractionDispatchedSignature OnInteractionDispatched;

protected:
    /**
     * Maximum distance, in Unreal units/centimetres, at which an
     * interaction target can be reached.
     *
     * This is an initial tuning value and is intentionally editable.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Interaction",
        meta = (ClampMin = "0.0", UIMin = "0.0"))
    float InteractionDistance = 250.0f;

    /**
     * Collision channel used to acquire interaction targets.
     *
     * Visibility is the baseline so the foundation requires no
     * project-wide collision configuration. A dedicated interaction
     * channel can be selected later without changing this API.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Interaction")
    TEnumAsByte<ECollisionChannel> InteractionTraceChannel =
        ECC_Visibility;

private:
    AActor *FindInteractionTarget() const;

    UPROPERTY(Transient)
    TObjectPtr<USceneComponent> InteractionOriginComponent;
};
