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
 * World fact that the player was meaningfully presented with a choice.
 * This is NOT observer awareness, and it is NOT a sampled opportunity.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(
    FOnPedroOpportunityExposed,
    FName);

/**
 * Encounter-local authored source of factual behaviour events.
 *
 * Gameplay actors call ReportConfirmedAction only AFTER an action succeeds.
 * They call ReportOpportunityExposed only after the player has actually
 * encountered a meaningful choice (not just because an actor exists).
 *
 * The source does not grant observer knowledge or personality points.
 * Disabled by default to preserve existing prototype-map behaviour.
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

    /**
     * Signals meaningful player exposure to this authored opportunity.
     * Called by the activity/trigger at actual exposure; mere level presence
     * must NOT call this. Does not claim Pedro witnessed the exposure.
     * Only valid when enabled and OpportunityId is authored.
     */
    UFUNCTION(BlueprintCallable, Category = "Pedro|Observation")
    bool ReportOpportunityExposed();

    /** Native subscription points for the Director and C++ tests. */
    FOnPedroWorldEventProduced OnWorldEventProduced;
    FOnPedroOpportunityExposed OnOpportunityExposed;
};
