#pragma once

struct FPedroEvidenceRecord;
struct FPedroObservationState;

/**
 * Pedro-local rules translating accepted facts into provisional trait scores
 * and structural counters. No perception or final classification occurs here.
 */
namespace PedroEvidenceRules
{
    /**
     * Called once by FPedroObservationState::RecordEvidence(), immediately
     * after the record is appended to EvidenceHistory.
     *
     * Do not call directly from gameplay actors: duplicate validation and
     * evidence acquisition must happen before interpreting the fact.
     */
    void ApplyAcceptedEvidence(
        FPedroObservationState& State,
        const FPedroEvidenceRecord& Evidence);
}
