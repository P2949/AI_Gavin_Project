#include "Encounters/Pedro/PedroActivePresentation.h"

#include "GameFramework/Actor.h"

FPedroPresentationPlan PedroActivePresentation::Prepare(
    const FPedroCompletionDecision& Completion,
    AActor* GentleActor,
    AActor* SeekingActor,
    AActor* DisruptiveActor)
{
    FPedroPresentationPlan Plan;
    if (!Completion.CanConclude())
    {
        return Plan;
    }

    Plan.Interpretation = Completion.Classification.Interpretation;

    if (!IsValid(GentleActor) || !IsValid(SeekingActor) ||
        !IsValid(DisruptiveActor))
    {
        Plan.Status = EPedroPresentationStatus::MissingOutcomeActor;
        return Plan;
    }

    if (GentleActor == SeekingActor || GentleActor == DisruptiveActor ||
        SeekingActor == DisruptiveActor)
    {
        Plan.Status = EPedroPresentationStatus::DuplicateOutcomeActor;
        return Plan;
    }

    switch (Plan.Interpretation)
    {
    case EPedroInterpretation::Gentle:
        Plan.SelectedActor = GentleActor;
        break;
    case EPedroInterpretation::Seeking:
        Plan.SelectedActor = SeekingActor;
        break;
    case EPedroInterpretation::Disruptive:
        Plan.SelectedActor = DisruptiveActor;
        break;
    default:
        Plan.Status = EPedroPresentationStatus::GateNotReady;
        return Plan;
    }

    Plan.Status = EPedroPresentationStatus::Ready;
    return Plan;
}
