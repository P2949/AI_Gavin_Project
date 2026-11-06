#pragma once

#include "CoreMinimal.h"

struct FPedroObservationState;

/**
 * Pedro's provisional INTERNAL interpretations. The player-facing role names
 * and presentations are separate, and are not decided by this type.
 * Deliberately not reflected to Blueprint until there is a real consumer.
 */
enum class EPedroInterpretation : uint8
{
    Gentle,
    Seeking,
    Disruptive
};

/**
 * Pure scoring output. A candidate with bHasEvidence=false is a deterministic
 * fallback, NOT permission to finish the encounter.
 */
struct FPedroClassificationResult
{
    EPedroInterpretation Interpretation = EPedroInterpretation::Seeking;

    int32 GentleScore = 0;
    int32 SeekingScore = 0;
    int32 DisruptiveScore = 0;

    bool bGentleEligible = true;
    bool bHasEvidence = false;
};

namespace PedroClassification
{
    /**
     * Score only facts Pedro has acquired in FPedroObservationState.
     * Does not mutate State, inspect the World, complete the encounter,
     * or claim that prototype-assumed evidence was actually witnessed.
     *
     * PROVISIONAL WEIGHTS AND TIE ORDER: Seeking > Gentle > Disruptive.
     * Revisit these after authoring diverse observation activities.
     */
    FPedroClassificationResult Evaluate(
        const FPedroObservationState& State);
}
