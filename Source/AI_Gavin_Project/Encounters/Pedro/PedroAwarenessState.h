#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AITypes.h"

// Encounter-local spatial knowledge, NOT behavioural evidence.
enum class EPedroAwarenessStatus : uint8 { Unaware, Perceived, LostSight };

struct FPedroAwarenessState
{
    EPedroAwarenessStatus Status = EPedroAwarenessStatus::Unaware;
    TWeakObjectPtr<APawn> ObservedPlayer;
    FVector LastKnownPlayerLocation = FVector::ZeroVector;
    bool bHasLastKnownPlayerLocation = false;
    double LastSuccessfulSightingTime = 0.0;

    // Called only after the controller's native sight + current physical query.
    bool AcceptSighting(APawn* Player, const FVector& Location, double Time)
    {
        if (!IsValid(Player) || Location.ContainsNaN() ||
            !FAISystem::IsValidLocation(Location) || !FMath::IsFinite(Time) || Time < 0.0)
            return false;
        if (bHasLastKnownPlayerLocation && ObservedPlayer.Get() == Player &&
            Time < LastSuccessfulSightingTime)
            return false;
        ObservedPlayer = Player;
        LastKnownPlayerLocation = Location;
        bHasLastKnownPlayerLocation = true;
        LastSuccessfulSightingTime = Time;
        Status = EPedroAwarenessStatus::Perceived;
        return true;
    }

    // No location parameter: failure can never disclose a position.
    void LoseSight()
    {
        if (bHasLastKnownPlayerLocation && ObservedPlayer.IsValid())
            Status = EPedroAwarenessStatus::LostSight;
        else
            Reset();
    }

    void Reset() { *this = FPedroAwarenessState{}; }
};
