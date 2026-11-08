#pragma once

#include "CoreMinimal.h"

#include "Encounters/Pedro/PedroClassification.h"

struct FPedroObservationState;

/**
 * Readiness only. None of these states implies that an encounter was resolved.
 * Kept native-only, like the Phase A classifier, until a Blueprint consumer
 * actually needs a reflected API.
 */
enum class EPedroCompletionStatus : uint8
{
    NoCoreOpportunities,
    InvalidSampleFraction,
    NoAcquiredEvidence,
    InsufficientCoreSamples,
    InconsistentSampleEvidence,
    Ready
};

/**
 * Side-effect-free snapshot of the completion decision.
 * Classification is meaningful ONLY when bHasClassification is true.
 */
struct FPedroCompletionDecision
{
    EPedroCompletionStatus Status = EPedroCompletionStatus::NoCoreOpportunities;

    int32 CoreCount = 0;
    int32 RequiredCoreSamples = 0;
    int32 SampledCoreCount = 0;
    int32 SampledBonusCount = 0;
    int32 AcquiredEvidenceCount = 0;

    bool bHasClassification = false;
    FPedroClassificationResult Classification;

    bool CanConclude() const
    {
        return Status == EPedroCompletionStatus::Ready && bHasClassification;
    }
};

namespace PedroCompletionGate
{
    /**
     * Evaluate whether Pedro possesses enough accepted evidence to form a
     * candidate interpretation. A core opportunity counts only if the player
     * was exposed AND Pedro acquired a response for that opportunity.
     *
     * The configured fraction follows FPedroOpportunityState's existing
     * clamping (0.01..1.0) and upward rounding. Bonus opportunities never
     * substitute for core opportunities. A non-finite fraction fails closed.
     *
     * Readiness does NOT present an outcome, mark shared progress resolved,
     * run a timer, or check the Director's one-time presentation guard.
     * The future Director integration owns those operations.
     */
    FPedroCompletionDecision Evaluate(
        const FPedroObservationState& State,
        float CoreSampleFraction);
}
