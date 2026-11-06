#include "Encounters/Pedro/PedroClassification.h"

#include "Encounters/Pedro/PedroObservationState.h"

namespace PedroClassification
{
FPedroClassificationResult Evaluate(const FPedroObservationState& State)
{
    FPedroClassificationResult Result;
    Result.bHasEvidence = State.GetEvidenceCount() > 0;

    // These are structural facts from ACCEPTED evidence, not guessed from
    // aggregate scores. For example, helping afterward cannot cancel an
    // actual attack against a character.
    Result.bGentleEligible =
        State.NPCAttackCount == 0 &&
        State.ObserverAttackCount == 0 &&
        State.CruelActionCount == 0 &&
        State.OwnedPropertyDestroyedCount == 0;

    const FPedroTraitScores& Scores = State.TraitScores;

    // Provisional, deliberately simple linear scores. These consume the
    // capped trait contributions already produced by PedroEvidenceRules.
    // Do NOT add raw counts again: that would double-count evidence and
    // bypass the reducer's repetition caps.
    Result.GentleScore =
        2 * Scores.Care + Scores.Patience - Scores.Apathy;
    Result.SeekingScore =
        2 * Scores.Curiosity + Scores.Play + Scores.Repetition;
    Result.DisruptiveScore =
        2 * Scores.Disruption + 2 * Scores.Aggression - Scores.Care;

    // No finalization occurs here. Seeking is the specified tie fallback,
    // including the all-zero/no-evidence case. Callers must independently
    // enforce the core opportunity sampling gate before presenting a result.
    const bool bSeekingBeatsGentle =
        !Result.bGentleEligible ||
        Result.SeekingScore >= Result.GentleScore;

    if (bSeekingBeatsGentle &&
        Result.SeekingScore >= Result.DisruptiveScore)
    {
        Result.Interpretation = EPedroInterpretation::Seeking;
    }
    else if (Result.bGentleEligible &&
             Result.GentleScore >= Result.DisruptiveScore)
    {
        Result.Interpretation = EPedroInterpretation::Gentle;
    }
    else
    {
        Result.Interpretation = EPedroInterpretation::Disruptive;
    }

    return Result;
}
} // namespace PedroClassification
