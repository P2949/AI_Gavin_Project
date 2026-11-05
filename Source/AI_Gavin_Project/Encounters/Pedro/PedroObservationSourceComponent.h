#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Encounters/Pedro/PedroObservationTypes.h"

#include "PedroObservationSourceComponent.generated.h"

/**
 * Native, synchronous notification about a confirmed world event.
 * Listeners must copy the event if they need to keep it after the callback.
 * This is world truth, NOT proof Pedro witnessed the action.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(
    FOnPedroWorldEventProduced,
    const FPedroBehaviourEvent&);

/**
 * Encounter-local authored source of factual behaviour events.
 *
 * Gameplay actors call ReportConfirmedAction only AFTER an action succeeds.
 * It does not award traits, decide observation, or resolve an encounter.
 *
 * Disabled by default to preserve existing prototype-map behaviour until
 * an individual actor is configured deliberately in the Editor.
 */
UCLASS(ClassGroup = (Pedro), meta = (BlueprintSpawnableComponent))
class AI_GAVIN_PROJECT_API UPedroObservationSourceComponent
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UPedroObservationSourceComponent();

    /** Whether this instance currently reports authored gameplay facts. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pedro|Observation")
    bool bEvidenceEnabled = false;

    /** Stable encounter-local identity of the item, prop, or NPC involved. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pedro|Observation")
    FName SubjectId = NAME_None;

    /** Stable encounter-local ID of the choice, task, or optional opportunity. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pedro|Observation")
    FName OpportunityId = NAME_None;

    /**
     * Reports one action that was actually completed, never an input request.
     * Each successful call creates exactly one new occurrence ID.
     * Returns false if disabled, unowned, missing identity, or Kind is None.
     *
     * The event is broadcast synchronously. An observer/Director must decide
     * whether Pedro can acquire it; it must NOT be marked observed here.
     */
    UFUNCTION(BlueprintCallable, Category = "Pedro|Observation")
    bool ReportConfirmedAction(EPedroEvidenceKind Kind);

    /** Native subscription point for the Director and C++ tests. */
    FOnPedroWorldEventProduced OnWorldEventProduced;
};
