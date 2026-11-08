#pragma once

#include "CoreMinimal.h"
#include "Encounters/Pedro/PedroCompletionGate.h"

class AActor;

/** Native-only presentation validation. Does not mutate actors or progression. */
enum class EPedroPresentationStatus : uint8
{
    GateNotReady,
    MissingOutcomeActor,
    DuplicateOutcomeActor,
    Ready
};

struct FPedroPresentationPlan
{
    EPedroPresentationStatus Status = EPedroPresentationStatus::GateNotReady;
    EPedroInterpretation Interpretation = EPedroInterpretation::Seeking;
    AActor* SelectedActor = nullptr;

    bool IsReady() const
    {
        return Status == EPedroPresentationStatus::Ready && SelectedActor != nullptr;
    }
};

namespace PedroActivePresentation
{
    /**
     * All three outcome actors must exist and be different, regardless of the
     * chosen candidate. The caller separately validates world/progression.
     */
    FPedroPresentationPlan Prepare(
        const FPedroCompletionDecision& Completion,
        AActor* GentleActor,
        AActor* SeekingActor,
        AActor* DisruptiveActor);
}
