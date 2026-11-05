
#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

#include "PedroObservationTypes.generated.h"

class AActor;

/**
 * A concrete, authored fact about something the player actually did.
 *
 * These values describe world events, not personality scores or final roles.
 * An interaction request alone does not prove that an action succeeded.
 */
UENUM(BlueprintType)
enum class EPedroEvidenceKind : uint8
{
    None,

    // Exploration and experimentation.
    InspectedOptionalObject,
    RepeatedInteraction,
    EnteredOptionalArea,
    DiscoveredSecret,

    // NPC requests and contextual item decisions.
    HelpedNPC,
    IgnoredNPCRequest,
    KeptRequestedItemSelfishly,
    KeptRequestedItemCruelly,
    KeptRequestedItemNeutrally,

    // Puzzle choices.
    SolvedPuzzleMethodically,
    SolvedPuzzleByTrialAndError,
    AbandonedPuzzle,

    // Destruction and aggression.
    DestroyedOwnedProperty,
    DestroyedUnownedProperty,
    AttackedNPC,
    AttackedObserver
};

/**
 * How an event became part of Pedro's knowledge.
 *
 * Only DirectObservation is planned for the first prototype; the other
 * sources reserve a vocabulary for possible future functionality.
 */
UENUM(BlueprintType)
enum class EPedroKnowledgeSource : uint8
{
    None,
    DirectObservation,
    Reported,
    Inferred
};

/**
 * World truth: one confirmed player action, whether Pedro knows about it or not.
 *
 * Instances must be created when the action actually happens. EventId must
 * be generated once per occurrence and preserved if the event is forwarded.
 */
USTRUCT(BlueprintType)
struct FPedroBehaviourEvent
{
    GENERATED_BODY()

    /** Unique identity for this occurrence, used for duplicate detection. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Event")
    FGuid EventId;

    /** Concrete authored fact, never the resulting personality category. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Event")
    EPedroEvidenceKind Kind = EPedroEvidenceKind::None;

    /**
     * Actor the action concerned (item, NPC, prop, etc.).
     * Non-owning: this must not keep destroyed actors alive.
     * Not Blueprint-exposed; use SubjectId for durable identification.
     */
    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> SubjectActor;

    /** Authored subject identifier retained even if SubjectActor disappears. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Event")
    FName SubjectId = NAME_None;

    /** Optional encounter-local opportunity associated with this event. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Event")
    FName OpportunityId = NAME_None;

    /** Location of the action at occurrence time, not the actor's later location. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Event")
    FVector EventLocation = FVector::ZeroVector;

    /** GetWorld()->GetTimeSeconds() captured when the action occurred. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Event")
    float WorldTimeSeconds = 0.0f;
};

/**
 * Observer knowledge: a world event that Pedro has actually acquired.
 *
 * An event should enter this structure only after an observation/acquisition
 * policy accepts it. The current prototype may use an explicitly temporary
 * permissive policy until physical awareness checking is implemented.
 */
USTRUCT(BlueprintType)
struct FPedroEvidenceRecord
{
    GENERATED_BODY()

    /** The original world event; its ID and occurrence details are preserved. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Evidence")
    FPedroBehaviourEvent Event;

    /** Acquisition provenance, intentionally invalid until assigned. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Evidence")
    EPedroKnowledgeSource KnowledgeSource = EPedroKnowledgeSource::None;

    /** Time Pedro learned about it; may differ from the event time. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Evidence")
    float AcquiredAtWorldTimeSeconds = 0.0f;
};

/**
 * Derived, encounter-local behavioural dimensions.
 *
 * This type contains data only. The evidence reducer will update these
 * values later; authored world objects must not award trait points directly.
 */
USTRUCT(BlueprintType)
struct FPedroTraitScores
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Traits")
    int32 Curiosity = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Traits")
    int32 Care = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Traits")
    int32 Disruption = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Traits")
    int32 Aggression = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Traits")
    int32 Patience = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Traits")
    int32 Play = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Traits")
    int32 Repetition = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Traits")
    int32 Apathy = 0;
};
