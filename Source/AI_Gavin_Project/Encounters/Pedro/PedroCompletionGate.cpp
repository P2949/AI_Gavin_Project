#include "Encounters/Pedro/PedroCompletionGate.h"

#include "Encounters/Pedro/PedroObservationState.h"

namespace
{
/**
 * Defense in depth: a sampled core may only be counted if the corresponding
 * accepted event really exists in Pedro's EvidenceHistory. Production state
 * normally maintains this invariant in RecordEvidence(). This rejects
 * inconsistent states created by other code mistakenly calling the low-level
 * NoteAcquiredResponse() API directly.
 */
bool EverySampledCoreHasAcceptedEvidence(const FPedroObservationState& State)
{
    for (const FPedroOpportunityProgressEntry& Entry : State.Opportunities.Entries)
    {
        if (Entry.Tier != EPedroOpportunityTier::Core || !Entry.IsSampled())
        {
            continue;
        }

        bool bFoundMatchingEvidence = false;
        for (const FPedroEvidenceRecord& Evidence : State.EvidenceHistory)
        {
            if (Evidence.Event.OpportunityId == Entry.OpportunityId)
            {
                bFoundMatchingEvidence = true;
                break;
            }
        }

        if (!bFoundMatchingEvidence)
        {
            return false;
        }
    }
    return true;
}
} // namespace

FPedroCompletionDecision PedroCompletionGate::Evaluate(
    const FPedroObservationState& State,
    float CoreSampleFraction)
{
    FPedroCompletionDecision Decision;
    const FPedroOpportunityState& Opportunities = State.Opportunities;

    Decision.CoreCount = Opportunities.GetCoreCount();
    Decision.SampledCoreCount = Opportunities.GetSampledCoreCount();
    Decision.SampledBonusCount = Opportunities.GetSampledBonusCount();
    Decision.AcquiredEvidenceCount = State.GetEvidenceCount();

    if (Decision.CoreCount == 0)
    {
        return Decision;
    }

    // NaN or infinity must not accidentally lower the completion threshold.
    // A finite out-of-range value is clamped by the opportunity state API.
    if (!FMath::IsFinite(CoreSampleFraction))
    {
        Decision.Status = EPedroCompletionStatus::InvalidSampleFraction;
        return Decision;
    }

    Decision.RequiredCoreSamples =
        Opportunities.GetRequiredCoreSamples(CoreSampleFraction);

    if (Decision.AcquiredEvidenceCount == 0)
    {
        Decision.Status = EPedroCompletionStatus::NoAcquiredEvidence;
        return Decision;
    }

    if (!Opportunities.HasSufficientCoreSamples(CoreSampleFraction))
    {
        Decision.Status = EPedroCompletionStatus::InsufficientCoreSamples;
        return Decision;
    }

    if (!EverySampledCoreHasAcceptedEvidence(State))
    {
        Decision.Status = EPedroCompletionStatus::InconsistentSampleEvidence;
        return Decision;
    }

    Decision.Classification = PedroClassification::Evaluate(State);
    if (!Decision.Classification.bHasEvidence)
    {
        // Defensive consistency check against future classifier changes.
        Decision.Status = EPedroCompletionStatus::NoAcquiredEvidence;
        return Decision;
    }

    Decision.bHasClassification = true;
    Decision.Status = EPedroCompletionStatus::Ready;
    return Decision;
}
