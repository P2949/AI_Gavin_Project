#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Encounters/Pedro/PedroObservationState.h"

#include "PedroEncounterDirector.generated.h"

class UEncounterDefinition;
class UPlayerInteractionComponent;
class FDataValidationContext;

/**
 * Coordinates the prototype Pedro encounter.
 *
 * The director observes neutral player actions, stores encounter-local
 * behavioural facts, derives a temporary prototype interpretation, and
 * presents the corresponding outcome.
 *
 * Observation, interpretation, and presentation are deliberately kept
 * separate so each can evolve without changing the shared player systems.
 */
UCLASS()
class AI_GAVIN_PROJECT_API APedroEncounterDirector : public AActor
{
    GENERATED_BODY()

public:
    APedroEncounterDirector();

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(
        FDataValidationContext &Context) const override;
#endif

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION()
    void HandleInteractionDispatched(
        AActor *InteractionTarget,
        AActor *Interactor);

    /**
     * Evaluates whether enough observations have been collected for this
     * prototype to present an outcome.
     */
    void EvaluatePrototypeOutcome();

    /**
     * Presents the temporary prototype outcome and marks the encounter
     * resolved for this game session.
     */
    void PresentPrototypeOutcome(
        bool bExploratoryOutcome);

    /**
     * Encounter metadata used when reporting shared resolution state.
     */
    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "Pedro|Encounter")
    TObjectPtr<UEncounterDefinition> EncounterDefinition;

    /**
     * Temporary prototype presentation actor used when the player interacted
     * with multiple unique objects.
     */
    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "Pedro|Prototype")
    TObjectPtr<AActor> ExploratoryOutcomeActor;

    /**
     * Temporary prototype presentation actor used when the player repeatedly
     * interacted with a smaller set of objects.
     */
    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "Pedro|Prototype")
    TObjectPtr<AActor> RepetitiveOutcomeActor;

    /**
     * Number of total interaction requests required before this prototype
     * produces an interpretation.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Pedro|Prototype",
        meta = (ClampMin = "1", UIMin = "1"))
    int32 InteractionsBeforeOutcome = 3;

    /**
     * Number of unique interacted actors required for the temporary
     * exploratory outcome.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Pedro|Prototype",
        meta = (ClampMin = "1", UIMin = "1"))
    int32 UniqueInteractionsForExploratoryOutcome = 2;

    UPROPERTY(
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Pedro|Observation")
    FPedroObservationState ObservationState;

private:
    void SetOutcomeActorActive(
        AActor *OutcomeActor,
        bool bActive);

    UPROPERTY(Transient)
    TObjectPtr<UPlayerInteractionComponent> InteractionComponent;

    TSet<TWeakObjectPtr<AActor>> UniqueInteractedActors;

    bool bHasPresentedOutcome = false;
};
