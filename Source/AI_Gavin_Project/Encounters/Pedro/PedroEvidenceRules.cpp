#include "Encounters/Pedro/PedroEvidenceRules.h"

#include "Encounters/Pedro/PedroObservationState.h"

namespace
{
// Temporary prototype weights: one point per relevant dimension.
// Caps prevent one authored action/source from dominating through spam.
constexpr int32 DefaultTraitContributionCap = 1;
constexpr int32 RepeatedTraitContributionCap = 3;

FName GetEvidenceIdentity(const FPedroBehaviourEvent& Event)
{
    // Distinct NPC requests may share an NPC but have distinct opportunities.
    return !Event.OpportunityId.IsNone()
        ? Event.OpportunityId
        : Event.SubjectId;
}

int32 CountEarlierMatchingEvidence(
    const FPedroObservationState& State,
    const FPedroEvidenceRecord& Evidence)
{
    // RecordEvidence appends the new record before invoking this reducer.
    // Only compare the earlier records, excluding the newly added final one.
    const FName Identity = GetEvidenceIdentity(Evidence.Event);
    int32 Count = 0;

    for (int32 Index = 0; Index + 1 < State.EvidenceHistory.Num(); ++Index)
    {
        const FPedroBehaviourEvent& Earlier =
            State.EvidenceHistory[Index].Event;

        if (Earlier.Kind == Evidence.Event.Kind &&
            GetEvidenceIdentity(Earlier) == Identity)
        {
            ++Count;
        }
    }

    return Count;
}

int32 GetTraitContributionCap(EPedroEvidenceKind Kind)
{
    switch (Kind)
    {
    case EPedroEvidenceKind::RepeatedInteraction:
    case EPedroEvidenceKind::AttackedNPC:
    case EPedroEvidenceKind::AttackedObserver:
        return RepeatedTraitContributionCap;

    default:
        return DefaultTraitContributionCap;
    }
}
}

void PedroEvidenceRules::ApplyAcceptedEvidence(
    FPedroObservationState& State,
    const FPedroEvidenceRecord& Evidence)
{
    // Only the observation state should invoke this, after successful
    // validation and insertion. It owns duplicate-event protection.
    check(State.EvidenceHistory.Num() > 0);
    check(State.EvidenceHistory.Last().Event.EventId == Evidence.Event.EventId);

    const bool bScoreTraits =
        CountEarlierMatchingEvidence(State, Evidence) <
        GetTraitContributionCap(Evidence.Event.Kind);

    FPedroTraitScores& Scores = State.TraitScores;

    switch (Evidence.Event.Kind)
    {
    case EPedroEvidenceKind::InspectedOptionalObject:
    case EPedroEvidenceKind::EnteredOptionalArea:
        if (bScoreTraits)
        {
            ++Scores.Curiosity;
        }
        break;

    case EPedroEvidenceKind::RepeatedInteraction:
        if (bScoreTraits)
        {
            ++Scores.Repetition;
        }
        break;

    case EPedroEvidenceKind::DiscoveredSecret:
        ++State.SecretsDiscoveredCount;
        if (bScoreTraits)
        {
            ++Scores.Curiosity;
        }
        break;

    case EPedroEvidenceKind::HelpedNPC:
        ++State.HelpedNPCCount;
        if (bScoreTraits)
        {
            ++Scores.Care;
        }
        break;

    case EPedroEvidenceKind::IgnoredNPCRequest:
        ++State.IgnoredNPCRequestCount;
        if (bScoreTraits)
        {
            ++Scores.Apathy;
        }
        break;

    case EPedroEvidenceKind::KeptRequestedItemSelfishly:
        ++State.SelfishActionCount;
        if (bScoreTraits)
        {
            --Scores.Care;
        }
        break;

    case EPedroEvidenceKind::KeptRequestedItemCruelly:
        ++State.CruelActionCount;
        if (bScoreTraits)
        {
            --Scores.Care;
            ++Scores.Disruption;
        }
        break;

    case EPedroEvidenceKind::KeptRequestedItemNeutrally:
        // Preserve the fact, but assign no personality interpretation.
        break;

    case EPedroEvidenceKind::SolvedPuzzleMethodically:
        if (bScoreTraits)
        {
            ++Scores.Patience;
        }
        break;

    case EPedroEvidenceKind::SolvedPuzzleByTrialAndError:
        if (bScoreTraits)
        {
            ++Scores.Play;
        }
        break;

    case EPedroEvidenceKind::AbandonedPuzzle:
        if (bScoreTraits)
        {
            ++Scores.Apathy;
        }
        break;

    case EPedroEvidenceKind::DestroyedOwnedProperty:
        ++State.OwnedPropertyDestroyedCount;
        if (bScoreTraits)
        {
            ++Scores.Disruption;
        }
        break;

    case EPedroEvidenceKind::DestroyedUnownedProperty:
        ++State.UnownedPropertyDestroyedCount;
        if (bScoreTraits)
        {
            ++Scores.Disruption;
        }
        break;

    case EPedroEvidenceKind::AttackedNPC:
        ++State.NPCAttackCount;
        if (bScoreTraits)
        {
            ++Scores.Aggression;
        }
        break;

    case EPedroEvidenceKind::AttackedObserver:
        ++State.ObserverAttackCount;
        if (bScoreTraits)
        {
            ++Scores.Aggression;
        }
        break;

    case EPedroEvidenceKind::None:
        // RecordEvidence rejects None, but keep the switch exhaustive.
        break;
    }
}
