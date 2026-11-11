#pragma once

#include "CoreMinimal.h"
#include "Encounters/Pedro/PedroObservationTypes.h"

class AActor;
class UWorld;

/** A view-cone and collision-trace RESULT, not an AI alertness state. */
enum class EPedroDirectObservationStatus : uint8
{
    Visible,
    MissingWorld,
    MissingObserver,
    ObserverWorldMismatch,
    InvalidEvent,
    InvalidSettings,
    InvalidGeometry,
    TooFar,
    OutsideViewCone,
    Occluded
};

/**
 * One fixed observer eye point, forward direction, and Visibility channel ray.
 * This does not implement AI Perception, hearing, memory or observer tracking.
 */
namespace PedroDirectObservation
{
    const TCHAR* StatusName(EPedroDirectObservationStatus Status);

    /** Pure geometry check. This function does NOT test physical occlusion. */
    EPedroDirectObservationStatus CheckGeometry(
        const FVector& EyeLocation,
        const FVector& EyeForward,
        const FVector& EventLocation,
        float MaxDistance,
        float ViewHalfAngleDegrees);

    /**
     * Fail closed. Test the recorded EVENT position at emission time.
     * The line trace uses ECC_Visibility. A first hit on the event's
     * subject is considered visible; any other blocking hit is occlusion.
     * The actual event source is NOT ignored when tracing.
     */
    EPedroDirectObservationStatus Evaluate(
        const UWorld* World,
        const AActor* Observer,
        const FPedroBehaviourEvent& Event,
        float MaxDistance,
        float ViewHalfAngleDegrees);
}
