#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

#include "Encounters/Pedro/PedroCompletionGate.h"
#include "Encounters/Pedro/PedroObservationState.h"

#include "PedroEncounterDirector.generated.h"

/** Legacy remains the default. Evidence-driven is opt-in on a level instance. */
UENUM(BlueprintType)
enum class EPedroCompletionPolicy : uint8
{
    LegacyInteractionCount UMETA(DisplayName = "Legacy interaction count"),
    EvidenceDriven UMETA(DisplayName = "Evidence-driven completion")
};

class UEncounterDefinition;
class UPlayerInteractionComponent;
class UPedroObservationSourceComponent;
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
 *
 * Confirmed authored world events can be acquired in a deliberately
 * provisional mode; that mode does NOT implement observer perception.
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

    /**
     * TEMPORARY PROTOTYPE MODE, NOT ACTUAL PERCEPTION.
     *
     * Valid events from registered Pedro sources become observer knowledge
     * marked PrototypeAssumed. This does NOT mean Pedro physically saw them.
     * Disabled by default. Enable deliberately on the level instance.
     */
    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadWrite,
        Category = "Pedro|Prototype|Acquisition",
        meta = (DisplayName = "Assume Awareness (Prototype Only - No LOS)"))
    bool bUsePrototypeAssumedAcquisition = false;

    /**
     * B2b-4a: Opt-in, line-traced view-cone acquisition of confirmed events.
     * Never enable together with prototype-assumed acquisition: both ON
     * deliberately fails closed instead of silently switching provenance.
     * Existing map instances keep this false; no asset migration required.
     */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Pedro|Observation|Direct Line Of Sight")
    bool bUseDirectObservationAcquisition = false;

    /** A placed actor/TargetPoint representing the observer's eye and facing. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Pedro|Observation|Direct Line Of Sight")
    TObjectPtr<AActor> DirectObservationActor;

    /** Maximum eye-to-event distance, in Unreal centimeters. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Pedro|Observation|Direct Line Of Sight",
        meta=(ClampMin="1.0", UIMin="1.0"))
    float DirectObservationMaxDistance = 1500.0f;

    /** Angle from the actor's forward direction (half of total field of view). */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Pedro|Observation|Direct Line Of Sight",
        meta=(ClampMin="0.0", ClampMax="180.0", UIMin="0.0", UIMax="180.0"))
    float DirectObservationViewHalfAngleDegrees = 75.0f;

    /**
     * Authored core/bonus choices for this Pedro level instance.
     * IDs must match the ObservationSource component OpportunityIds.
     * An unconfigured source can still produce evidence, but won't count
     * toward the future encounter completion threshold.
     */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Pedro|Opportunities")
    TArray<FPedroOpportunityDefinition> OpportunityDefinitions;

    /**
     * Provisional ratio of all authored core opportunities to sample before
     * Pedro has enough information. Calculated for inspection only in this
     * milestone; it DOES NOT yet replace the legacy three-click outcome.
     */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Pedro|Opportunities",
        meta = (ClampMin = "0.01", ClampMax = "1.0", UIMin = "0.01", UIMax = "1.0"))
    float CoreSampleFraction = 0.75f;

    /**
     * Phase B2a: diagnostics only. Evaluation can report readiness, but
     * MUST NOT activate outcomes, call ResolveEncounter, or change the
     * legacy interaction-count completion path. Enabled by default to
     * exercise the existing greybox level without editing binary assets.
     */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Pedro|Diagnostics")
    bool bEnableCompletionGateShadowEvaluation = true;

    /** No production asset changes: legacy stays authoritative by default. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Pedro|Completion")
    EPedroCompletionPolicy CompletionPolicy =
        EPedroCompletionPolicy::LegacyInteractionCount;

    /**
     * Subscribe to a source spawned after BeginPlay. Level-placed sources
     * are found automatically. Returns false on invalid/duplicate sources.
     * Registering does not enable reporting or grant any knowledge.
     * Exposed to Blueprints for dynamically spawned source actors.
     */
    UFUNCTION(BlueprintCallable, Category = "Pedro|Observation")
    bool RegisterObservationSource(UPedroObservationSourceComponent* Source);

    /** Read-only native access for debugging and automated tests. */
    const FPedroObservationState& GetObservationState() const
    {
        return ObservationState;
    }

    /** Shadow snapshot is valid only after an exposure or accepted event. */
    bool HasShadowCompletionDecision() const
    {
        return bHasShadowCompletionDecision;
    }

    const FPedroCompletionDecision& GetShadowCompletionDecision() const
    {
        return ShadowCompletionDecision;
    }

    /** Valid only after successful shared progression resolution. */
    bool HasFinalClassification() const
    {
        return bHasFinalClassification;
    }

    const FPedroClassificationResult& GetFinalClassification() const
    {
        return FinalClassification;
    }

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
     *
     * Queued until the next tick after a qualifying interaction request,
     * allowing the current interaction to finish reporting its world event.
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

    /** Evidence-driven mode requires all three DIFFERENT world-placed actors. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Pedro|Completion")
    TObjectPtr<AActor> GentleOutcomeActor;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Pedro|Completion")
    TObjectPtr<AActor> SeekingOutcomeActor;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Pedro|Completion")
    TObjectPtr<AActor> DisruptiveOutcomeActor;

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

    /** One-time discovery of level-placed Pedro observation components. */
    void DiscoverLevelObservationSources();

    /** Remove every native delegate subscription established by this actor. */
    void UnregisterObservationSources();

    /** Receives world truth; provisional acquisition is a separate decision. */
    void HandleWorldEventProduced(const FPedroBehaviourEvent& Event);

    /** Receives a world fact of meaningful player exposure, NOT a sighting. */
    void HandleOpportunityExposed(FName OpportunityId);

    /** Protects the final action's evidence from the old pre-interaction hook. */
    void QueuePrototypeOutcomeEvaluation();

    /** Read-only, next-tick evaluation after the completed interaction. */
    void QueueShadowCompletionEvaluation();
    void EvaluateShadowCompletionGate();

    /** Active policy uses accepted evidence, never dispatched click count. */
    void QueueActiveCompletionEvaluation();
    void EvaluateActiveCompletion();

    UPROPERTY(Transient)
    TObjectPtr<UPlayerInteractionComponent> InteractionComponent;

    TSet<TWeakObjectPtr<AActor>> UniqueInteractedActors;
    TArray<TWeakObjectPtr<UPedroObservationSourceComponent>> BoundSources;

    FTimerHandle PendingPrototypeOutcomeTimer;
    FTimerHandle PendingShadowCompletionTimer;
    FTimerHandle PendingActiveCompletionTimer;
    FPedroClassificationResult FinalClassification;
    bool bActiveCompletionQueued = false;
    bool bHasFinalClassification = false;
    FPedroCompletionDecision ShadowCompletionDecision;
    bool bShadowCompletionQueued = false;
    bool bHasShadowCompletionDecision = false;
    bool bOutcomeEvaluationQueued = false;
    bool bHasPresentedOutcome = false;

    // Test-only access to simulate a resolved encounter without hub setup.
    friend class FPedroDirectorAcquisitionLifecycleTest;
    friend class FPedroDirectorOpportunityIntegrationTest;
    friend struct FPedroDirectorShadowTestAccess;
    friend class FPedroDirectorShadowAcquisitionRequiredTest;
    friend class FPedroDirectorShadowExposureFirstTest;
    friend class FPedroDirectorShadowEvidenceFirstTest;
    friend class FPedroDirectorShadowDisabledTest;
    friend class FPedroDirectorShadowDuplicateLifecycleTest;
    friend class FPedroDirectorShadowLegacyTest;
    friend struct FPedroActiveDirectorTestAccess;
    friend class APedroActiveWorldValidationHarness;
};
