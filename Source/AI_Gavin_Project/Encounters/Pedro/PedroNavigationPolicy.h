#pragma once
#include "CoreMinimal.h"
#include "Encounters/Pedro/PedroAwarenessState.h"

// No world queries, movement or player-transform access in this policy.
enum class EPedroNavigationMode : uint8 { Disabled, Idle, FollowObserved, ContinueLastKnown, Blocked };
enum class EPedroNavigationReason : uint8 { Disabled, NoObserver, NoKnowledge, TooClose, InBand, Fresh, Remembered, Expired, Invalid, Cooldown, NoNavData, NoRoute, Stalled, Timeout, Arrived, ForeignRequest, PathInvalid, MovementUnavailable };
struct FPedroNavigationSettings
{
    double StartDistance=650, StopDistance=500, PreferredDistance=450, PersonalSpace=250;
    double Speed=180, Acceleration=500, Braking=600, AcceptanceRadius=40;
    double ReplanInterval=.35, ReplanDisplacement=125, FreshAge=.35, HoldAge=2;
    double RetryInterval=1, MaxPathLength=2200, StallWindow=1.25, StallDistance=20, MaxDuration=15;
    double ProjectionXY=100, ProjectionZ=180, MaxVerticalSeparation=180;
    bool IsValid() const;
};
struct FPedroNavigationDecision
{
    EPedroNavigationMode Mode=EPedroNavigationMode::Disabled;
    EPedroNavigationReason Reason=EPedroNavigationReason::Disabled;
    bool bPlan=false;
};
struct FPedroNavIntent
{
    bool bActive=false;
    FVector Sighting=FVector::ZeroVector;
    double SightingTime=0, LastPlan=-1, RetryAfter=0;
};
namespace PedroNavigationPolicy
{
    FPedroNavigationDecision Select(bool bEnabled,bool bObserver,const FPedroAwarenessState& A,
        const APawn* Player,const FVector& Observer,double Now,const FPedroNavIntent& Intent,const FPedroNavigationSettings& S);
    TArray<FVector> Candidates(const FVector& Observer,const FVector& Known,const FPedroNavigationSettings& S);
    bool AcceptProjection(const FVector& Candidate,const FVector& Projected,const FVector& Known,const FPedroNavigationSettings& S);
    bool AcceptPath(bool bComplete,bool bSpecial,double Length,const FPedroNavigationSettings& S);
    bool BetterCandidate(int32 Index,double Cost,int32 BestIndex,double BestCost);
    bool OwnsRequest(FAIRequestID Owned,FAIRequestID Incoming);
    bool Stalled(double Elapsed,double Distance,const FPedroNavigationSettings& S);
}
