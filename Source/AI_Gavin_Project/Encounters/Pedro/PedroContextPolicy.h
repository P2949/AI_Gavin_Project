#pragma once
#include "CoreMinimal.h"
#include "Encounters/Pedro/PedroObservationTypes.h"

// No world queries; all inputs are already approved facts or candidate summaries.
enum class EPedroContextMode : uint8 { Disabled, ObserveIdle, FollowPlayer, ObserveActivity, TravelToWatchPoint, WatchFromPosition, HoldLastKnown, Withdraw, Disengaged, Satisfied };
enum class EPedroContextReason : uint8 { Disabled, Invalid, Resolved, Close, Cooldown, Lost, Activity, Point, Distant, InBand };
enum class EPedroContextMovementIntent : uint8 { Stop, Follow, RetainFollow, WatchPoint, Withdraw };
enum class EPedroContextAttentionIntent : uint8 { Default, Activity, Idle };
enum class EPedroMovePurpose : uint8 { None, Follow, WatchPoint, Withdraw };
struct FPedroContextSettings
{
    double Interval=.1, Persistence=.5, ActivityCap=3, ActivityRecovery=1, DepartureDistance=300;
    double Dwell=2, PointCooldown=1, PointInterval=.5, Improvement=10, MemoryDecay=15;
    double CloseDistance=220, CloseDuration=.5, RetreatDistance=450, DisengageDuration=3;
    bool IsValid() const;
};
struct FPedroContextActivity
{
    FGuid EventId;
    FName SubjectId, OpportunityId;
    EPedroEvidenceKind Kind=EPedroEvidenceKind::None;
    FVector ObservedLocation=FVector::ZeroVector, PlayerAtObservation=FVector::ZeroVector;
    double ObservedAt=0, Expires=0;
    int32 Priority=0;
};
struct FPedroContextCandidate
{
    FName Id;
    FVector Goal=FVector::ZeroVector;
    double Visibility=0, DistanceFit=0, Interest=0, Priority=0, Cost=0, Penalty=0;
    bool bEligible=false;
};
struct FPedroContextDecision
{
    EPedroContextMode Mode=EPedroContextMode::Disabled;
    EPedroContextReason Reason=EPedroContextReason::Disabled;
    EPedroContextMovementIntent MovementIntent=EPedroContextMovementIntent::Stop;
    EPedroContextAttentionIntent AttentionIntent=EPedroContextAttentionIntent::Default;
    TOptional<FVector> LookPoint, Destination;
    FName WatchPointId;
    FGuid SourceEventId;
};
struct FPedroContextPointMemory
{
    double LastVisit=-1, LastFailure=-1, LastLoss=-1;
    int32 Losses=0;
};
struct FPedroContextMemory
{
    FPedroContextActivity Activity, LastActivity;
    TMap<FName,FPedroContextPointMemory> Points;
    FVector LastLossRoot=FVector::ZeroVector;
    TSet<FGuid> SeenEvents;
    double HoldStarted=-1, ActivityResume=0, CloseSince=-1, DisengageUntil=0, EnteredAt=0;
    double LastLoss=-1, LastPointVisit=-1, LastPointFailure=-1, AttackAt=-1;
    // A one-time proven melee-contact direction. Never refreshed from player transform.
    FVector ContactLocation=FVector::ZeroVector;
    double ContactAt=-1;
    int32 LossEpisodes=0;
    bool bWasFresh=false, bSatisfied=false, bAttackPending=false;
    FName CurrentPoint, FailedPoint, LossPoint;
};
struct FPedroContextInput
{
    bool bEnabled=false,bValid=false,bFresh=false,bRecent=false,bFollowActive=false,bWatchActive=false,bWithdrawActive=false,bAtPoint=false,bCanWatch=false;
    double Now=0, Distance=0, StartDistance=650;
    FVector Known=FVector::ZeroVector, Observer=FVector::ZeroVector;
    FName SelectedPoint;
    TOptional<FVector> SelectedGoal;
};
namespace PedroContextPolicy
{
    bool ValidPoint(const FVector& V);
    // A fixed contact direction is available only while the attack is pending.
    bool HasPendingContactWithin(const FPedroContextMemory& Memory, double Now, double MaxAge);
    int32 ActivityPriority(EPedroEvidenceKind Kind);
    double ActivityDuration(int32 Priority);
    bool AcceptActivity(FPedroContextMemory& Memory,const FPedroEvidenceRecord& Record,const FVector& Known,double Now,const FPedroContextSettings& S);
    void UpdateSenses(FPedroContextMemory& Memory,const FPedroContextInput& I,const FPedroContextSettings& S);
    FPedroContextDecision Select(const FPedroContextInput& I,const FPedroContextMemory& M,const FPedroContextDecision& Previous,const FPedroContextSettings& S);
    // Shared timing gate: candidate evaluation gets first refusal when the native retry opens.
    bool WatchEvaluationDue(const FPedroContextInput& I,const FPedroContextMemory& M,const FPedroContextSettings& S,
        double NextPoint,double RetryAfter,double LastPlan,double ReplanInterval,double PersonalSpace);
    double Score(const FPedroContextCandidate& C);
    int32 BestCandidate(const TArray<FPedroContextCandidate>& C,double Current,double Improvement);
    double Decay(double Now,double Last,double Duration);
    bool EscapeSegment(const FVector& A,const FVector& B,const FVector& Known,double Radius);
    bool CanBind(bool bObserver,bool bDirector,bool bSameWorld,bool bDirectOnly,bool bAttention,bool bNavigation);
}
