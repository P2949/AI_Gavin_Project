#pragma once

#include "CoreMinimal.h"
#include "Encounters/Pedro/PedroAwarenessState.h"

// Attention consumes legitimate memory; it never queries the world's targets.
enum class EPedroAttentionMode : uint8 { Disabled, Idle, TrackObserved, HoldLastKnown };

struct FPedroAttentionSettings
{
    double TrackDegreesPerSecond = 90.0;
    double IdleDegreesPerSecond = 45.0;
    double PersistenceSeconds = 2.0;
    double FreshSeconds = 0.35;
    double DeadZoneDegrees = 2.0;
    double MinimumHorizontalDistance = 10.0;
    double MaxDeltaSeconds = 0.1;
    bool IsValid() const;
};

struct FPedroAttentionDecision
{
    EPedroAttentionMode Mode = EPedroAttentionMode::Disabled;
    TOptional<FVector> LookPoint;
    double IdleYaw = 0.0;
    // Last evaluated physical rotation, for inspection only.
    double AngularError = 0.0;
    double RequestedYawStep = 0.0;
};

namespace PedroAttentionPolicy
{
    FPedroAttentionDecision Select(bool bEnabled, bool bObserverValid,
        const FPedroAwarenessState& Knowledge, const APawn* CurrentPlayer,
        double Now, double IdleYaw, const FPedroAttentionSettings& Settings);
    double ShortestError(double From, double To);
    // Solve the horizontal ray from an offset eye as the root rotates.
    // Degenerate or unreachable horizontal geometry yields no request.
    TOptional<double> LookError(const FVector& Root, const FVector& Eye,
        const FVector& Forward, const FVector& Point, double MinimumDistance);
    double TurnStep(double Error, double DeltaSeconds, double Speed,
        const FPedroAttentionSettings& Settings);
}
