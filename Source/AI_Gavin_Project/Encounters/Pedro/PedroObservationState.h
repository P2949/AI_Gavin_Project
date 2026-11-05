
#pragma once

#include "CoreMinimal.h"

#include "Encounters/Pedro/PedroObservationTypes.h"

#include "PedroObservationState.generated.h"

/**
 * Stores the encounter-local observation state for Pedro.
 *
 * EvidenceHistory contains only information that Pedro has acquired,
 * rather than every event that happened in the world.
 *
 * Behavioural scores and structural facts are derived from that
 * knowledge. Their updates will be implemented by the evidence
 * interpretation layer in a later commit.
 *
 * This state is transient and resets for a fresh encounter.
 */
USTRUCT(BlueprintType)
struct FPedroObservationState
{
    GENERATED_BODY()

    // ------------------------------------------------------------
    // Temporary prototype compatibility
    // ------------------------------------------------------------

    /**
     * Legacy interaction counters still used by the current
     * three-click prototype outcome.
     *
     * Remove these when the real encounter completion and
     * classification logic replace the placeholder.
     */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Pedro|Prototype")
    int32 InteractionCount = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Pedro|Prototype")
    int32 UniqueInteractionCount = 0;

    // ------------------------------------------------------------
    // Acquired evidence
    // ------------------------------------------------------------

    /**
     * Complete history of evidence Pedro has accepted.
     *
     * This is observer knowledge, not the complete world history.
     */
    UPROPERTY(
        Transient,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Pedro|Observation")
    TArray<FPedroEvidenceRecord> EvidenceHistory;

    // ------------------------------------------------------------
    // Derived behavioural dimensions
    // ------------------------------------------------------------

    /**
     * Accumulated behavioural dimensions.
     *
     * Evidence rules will update this in a later commit.
     * Gameplay actors must not assign personality scores directly.
     */
    UPROPERTY(
        Transient,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Pedro|Observation")
    FPedroTraitScores TraitScores;

    // ------------------------------------------------------------
    // Structural facts
    // ------------------------------------------------------------

    /**
     * These counts preserve distinctions that cannot reliably
     * be inferred from aggregate behavioural scores.
     *
     * Examples include actions that may disqualify Gentle.
     * The evidence reducer will populate these later.
     */
    UPROPERTY(
        Transient,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Pedro|Facts")
    int32 HelpedNPCCount = 0;

    UPROPERTY(
        Transient,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Pedro|Facts")
    int32 IgnoredNPCRequestCount = 0;

    UPROPERTY(
        Transient,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Pedro|Facts")
    int32 SelfishActionCount = 0;

    UPROPERTY(
        Transient,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Pedro|Facts")
    int32 CruelActionCount = 0;

    UPROPERTY(
        Transient,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Pedro|Facts")
    int32 OwnedPropertyDestroyedCount = 0;

    UPROPERTY(
        Transient,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Pedro|Facts")
    int32 UnownedPropertyDestroyedCount = 0;

    UPROPERTY(
        Transient,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Pedro|Facts")
    int32 NPCAttackCount = 0;

    UPROPERTY(
        Transient,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Pedro|Facts")
    int32 ObserverAttackCount = 0;

    UPROPERTY(
        Transient,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Pedro|Facts")
    int32 SecretsDiscoveredCount = 0;

    // ------------------------------------------------------------
    // Evidence recording
    // ------------------------------------------------------------

    /**
     * Records one acquired piece of evidence.
     *
     * Returns true only when a new, valid record is accepted.
     *
     * Rejects:
     * - missing event identity;
     * - unspecified evidence kind;
     * - unspecified knowledge source;
     * - an event that was already recorded.
     *
     * Does not perform perception checks or derive scores.
     */
    bool RecordEvidence(const FPedroEvidenceRecord& Evidence)
    {
        if (!Evidence.Event.EventId.IsValid() ||
            Evidence.Event.Kind == EPedroEvidenceKind::None ||
            Evidence.KnowledgeSource == EPedroKnowledgeSource::None)
        {
            return false;
        }

        // For this small encounter, linear duplicate checking is
        // sufficient and avoids maintaining a second index that
        // could become inconsistent with EvidenceHistory.
        for (const FPedroEvidenceRecord& Existing : EvidenceHistory)
        {
            if (Existing.Event.EventId == Evidence.Event.EventId)
            {
                return false;
            }
        }

        EvidenceHistory.Add(Evidence);
        return true;
    }

    int32 GetEvidenceCount() const
    {
        return EvidenceHistory.Num();
    }

    // ------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------

    /**
     * Restores the complete observation state to its initial values.
     *
     * No evidence or classification history should survive a fresh
     * Pedro encounter.
     */
    void Reset()
    {
        InteractionCount = 0;
        UniqueInteractionCount = 0;

        EvidenceHistory.Reset();
        TraitScores = FPedroTraitScores{};

        HelpedNPCCount = 0;
        IgnoredNPCRequestCount = 0;

        SelfishActionCount = 0;
        CruelActionCount = 0;

        OwnedPropertyDestroyedCount = 0;
        UnownedPropertyDestroyedCount = 0;

        NPCAttackCount = 0;
        ObserverAttackCount = 0;

        SecretsDiscoveredCount = 0;
    }
};
