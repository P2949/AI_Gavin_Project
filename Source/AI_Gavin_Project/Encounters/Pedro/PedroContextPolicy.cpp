#include "Encounters/Pedro/PedroContextPolicy.h"

bool FPedroContextSettings::IsValid() const
{
    for(double V:{Interval,Persistence,ActivityCap,ActivityRecovery,DepartureDistance,Dwell,PointCooldown,PointInterval,Improvement,MemoryDecay,CloseDistance,CloseDuration,RetreatDistance,DisengageDuration})
        if(!FMath::IsFinite(V)||V<=0)return false;
    return ActivityCap>=2.5&&RetreatDistance>CloseDistance&&Persistence<=Dwell;
}
namespace PedroContextPolicy
{
using Mode=EPedroContextMode;using Reason=EPedroContextReason;using Move=EPedroContextMovementIntent;using Look=EPedroContextAttentionIntent;
bool ValidPoint(const FVector& V){return !V.ContainsNaN()&&FMath::Abs(V.X)<1.e12&&FMath::Abs(V.Y)<1.e12&&FMath::Abs(V.Z)<1.e12;}
bool HasPendingContactWithin(const FPedroContextMemory& Memory, double Now, double MaxAge)
{
    const double Age = Now - Memory.ContactAt;
    return Memory.bAttackPending && Memory.ContactAt >= 0 && Age >= 0 &&
        Age <= MaxAge && ValidPoint(Memory.ContactLocation);
}
int32 ActivityPriority(EPedroEvidenceKind K)
{
    switch(K){case EPedroEvidenceKind::None:return 0;case EPedroEvidenceKind::RepeatedInteraction:return 1;
    case EPedroEvidenceKind::DestroyedOwnedProperty:case EPedroEvidenceKind::DestroyedUnownedProperty:case EPedroEvidenceKind::AttackedNPC:case EPedroEvidenceKind::AttackedObserver:return 3;
    default:return static_cast<uint8>(K)<=static_cast<uint8>(EPedroEvidenceKind::AttackedObserver)?2:0;}
}
double ActivityDuration(int32 P){return P==1?1.2:P==2?2.:P==3?2.5:0.;}
bool AcceptActivity(FPedroContextMemory& M,const FPedroEvidenceRecord& R,const FVector& Known,double Now,const FPedroContextSettings& S)
{
    const auto& E=R.Event;const int32 P=ActivityPriority(E.Kind);
    if(!S.IsValid()||!FMath::IsFinite(Now)||Now<0||!FMath::IsFinite(E.WorldTimeSeconds)||!FMath::IsFinite(R.AcquiredAtWorldTimeSeconds)||R.AcquiredAtWorldTimeSeconds+0.001<E.WorldTimeSeconds||E.WorldTimeSeconds<0||Now+0.001<R.AcquiredAtWorldTimeSeconds||Now-E.WorldTimeSeconds>.35||!E.EventId.IsValid()||!P||!ValidPoint(E.EventLocation)||!ValidPoint(Known)||(E.SubjectId.IsNone()&&E.OpportunityId.IsNone())||R.KnowledgeSource!=EPedroKnowledgeSource::DirectObservation||M.SeenEvents.Contains(E.EventId)||M.SeenEvents.Num()>=4096)return false;
    M.SeenEvents.Add(E.EventId);
    M.LastActivity={E.EventId,E.SubjectId,E.OpportunityId,E.Kind,E.EventLocation,Known,Now,Now+ActivityDuration(P),P};
    if(M.Activity.EventId.IsValid()&&Now>=M.Activity.Expires){M.ActivityResume=FMath::Max(M.ActivityResume,M.Activity.Expires+S.ActivityRecovery);M.Activity={};M.HoldStarted=-1;}
    if(E.Kind==EPedroEvidenceKind::AttackedObserver){M.bAttackPending=true;M.AttackAt=Now;}
    if(Now<M.ActivityResume)return true;
    if(M.HoldStarted<0||Now>=M.Activity.Expires){M.HoldStarted=Now;M.Activity={};}
    if(M.Activity.Priority>P&&Now<M.Activity.Expires)return true;
    M.Activity={E.EventId,E.SubjectId,E.OpportunityId,E.Kind,E.EventLocation,Known,Now,FMath::Min(Now+ActivityDuration(P),M.HoldStarted+S.ActivityCap),P};
    // A hard refractory interval prevents repeated actions from extending a hold indefinitely.
    if(M.Activity.Expires>=M.HoldStarted+S.ActivityCap)M.ActivityResume=M.Activity.Expires+S.ActivityRecovery;
    return true;
}
double Decay(double Now,double Last,double Duration){return FMath::IsFinite(Now)&&FMath::IsFinite(Last)&&FMath::IsFinite(Duration)&&Duration>0&&Last>=0&&Now>=Last?FMath::Clamp(1-(Now-Last)/Duration,0.,1.):0.;}
void UpdateSenses(FPedroContextMemory& M,const FPedroContextInput& I,const FPedroContextSettings& S)
{
    if(!I.bValid||!S.IsValid())return;
    if(M.bWasFresh&&!I.bFresh)
    {
        M.LossEpisodes=FMath::Min(M.LossEpisodes+1,32);M.LastLoss=I.Now;M.LossPoint=M.CurrentPoint;M.LastLossRoot=I.Observer;
        if(!M.CurrentPoint.IsNone()&&(M.Points.Num()<32||M.Points.Contains(M.CurrentPoint)))
        {auto& P=M.Points.FindOrAdd(M.CurrentPoint);P.LastLoss=I.Now;P.Losses=FMath::Min(P.Losses+1,4);}
    }
    if(M.bAttackPending&&(M.AttackAt<0||I.Now-M.AttackAt>2.5))M.bAttackPending=false;
    M.bWasFresh=I.bFresh;
    if(I.bFresh&&I.Distance<S.CloseDistance){if(M.CloseSince<0)M.CloseSince=I.Now;}else M.CloseSince=-1;
    if(M.Activity.EventId.IsValid()&&(I.Now>=M.Activity.Expires||(I.bFresh&&FVector::Dist2D(I.Known,M.Activity.PlayerAtObservation)>=S.DepartureDistance))){M.ActivityResume=FMath::Max(M.ActivityResume,I.Now+S.ActivityRecovery);M.Activity={};M.HoldStarted=-1;}
}
FPedroContextDecision Select(const FPedroContextInput& I,const FPedroContextMemory& M,const FPedroContextDecision& Previous,const FPedroContextSettings& S)
{
    auto D=[](Mode A,Reason B,Move C=Move::Stop,Look L=Look::Default){FPedroContextDecision R;R.Mode=A;R.Reason=B;R.MovementIntent=C;R.AttentionIntent=L;return R;};
    if(!I.bEnabled)return {};
    if(!I.bValid||!S.IsValid()||!FMath::IsFinite(I.Now)||I.Now<0||!FMath::IsFinite(I.Distance)||I.Distance<0)return D(Mode::ObserveIdle,Reason::Invalid,Move::Stop,Look::Idle);
    if(M.bSatisfied)return D(Mode::Satisfied,Reason::Resolved,Move::Stop,Look::Idle);
    if(I.bFresh&&(I.bWithdrawActive||M.bAttackPending||(M.CloseSince>=0&&I.Now-M.CloseSince>=S.CloseDuration&&I.Now>=M.DisengageUntil)))return D(Mode::Withdraw,Reason::Close,Move::Withdraw);
    if(I.Now<M.DisengageUntil)return D(Mode::Disengaged,Reason::Cooldown,Move::Stop,Look::Default);
    if(!I.bFresh)
    {
        if(I.bRecent&&M.Activity.EventId.IsValid()&&I.Now<M.Activity.Expires){auto R=D(Mode::ObserveActivity,Reason::Activity,Move::Stop,Look::Activity);R.LookPoint=M.Activity.ObservedLocation;R.SourceEventId=M.Activity.EventId;return R;}
        return I.bRecent?D(Mode::HoldLastKnown,Reason::Lost,I.bFollowActive?Move::RetainFollow:Move::Stop):D(Mode::ObserveIdle,Reason::Lost,Move::Stop,Look::Idle);
    }
    // Close but not yet sustained: stop rather than start another contextual route.
    if(I.Distance<S.CloseDistance)return D(Mode::ObserveIdle,Reason::Close);
    if(M.Activity.EventId.IsValid()&&I.Now<M.Activity.Expires&&FVector::Dist2D(I.Known,M.Activity.PlayerAtObservation)<S.DepartureDistance){auto R=D(Mode::ObserveActivity,Reason::Activity,Move::Stop,Look::Activity);R.LookPoint=M.Activity.ObservedLocation;R.SourceEventId=M.Activity.EventId;return R;}
    if(I.bWatchActive&&Previous.Mode==Mode::TravelToWatchPoint)return Previous;
    if(I.bAtPoint&&(!I.bCanWatch||I.SelectedPoint.IsNone())&&(I.Distance<I.StartDistance||I.Now-M.LastPointVisit<S.Dwell)){auto R=D(Mode::WatchFromPosition,Reason::Point);R.WatchPointId=M.CurrentPoint;return R;}
    if(I.bCanWatch&&!I.SelectedPoint.IsNone()&&I.SelectedGoal.IsSet()&&I.Now-M.EnteredAt>=S.Persistence){auto R=D(Mode::TravelToWatchPoint,Reason::Point,Move::WatchPoint);R.WatchPointId=I.SelectedPoint;R.Destination=I.SelectedGoal;return R;}
    if(I.Distance>=I.StartDistance||I.bFollowActive)return D(Mode::FollowPlayer,Reason::Distant,Move::Follow);
    return D(Mode::ObserveIdle,Reason::InBand);
}
bool WatchEvaluationDue(const FPedroContextInput& I,const FPedroContextMemory& M,const FPedroContextSettings& S,
    double NextPoint,double RetryAfter,double LastPlan,double ReplanInterval,double PersonalSpace)
{
    return I.bFresh&&!I.bWatchActive&&!I.bWithdrawActive&&I.Now>=NextPoint&&I.Now>=RetryAfter&&
        (LastPlan<0||I.Now-LastPlan>=ReplanInterval)&&I.Now-M.EnteredAt>=S.Persistence&&
        (!I.bAtPoint||I.Now-M.LastPointVisit>=S.Dwell)&&I.Distance>=PersonalSpace;
}
double Score(const FPedroContextCandidate& C)
{
    if(!C.bEligible||!ValidPoint(C.Goal))return -1.e9;
    for(double V:{C.Visibility,C.DistanceFit,C.Interest,C.Priority,C.Cost,C.Penalty})if(!FMath::IsFinite(V)||V<0||V>1)return -1.e9;
    return 40*C.Visibility+20*C.DistanceFit+15*C.Interest+10*C.Priority-15*C.Cost-10*C.Penalty;
}
int32 BestCandidate(const TArray<FPedroContextCandidate>& C,double Current,double Improvement)
{
    if(!FMath::IsFinite(Current)||!FMath::IsFinite(Improvement)||Improvement<=0)return INDEX_NONE;
    int32 Best=INDEX_NONE;double Value=-1.e9;
    for(int32 I=0;I<C.Num();++I){const double V=Score(C[I]);if(C[I].Id.IsNone()||V<=-1.e8||V<Current+Improvement)continue;
        if(Best==INDEX_NONE||V>Value||(V==Value&&C[I].Id.LexicalLess(C[Best].Id))){Best=I;Value=V;}}
    return Best;
}
bool EscapeSegment(const FVector& A,const FVector& B,const FVector& K,double R)
{
    if(!ValidPoint(A)||!ValidPoint(B)||!ValidPoint(K)||!FMath::IsFinite(R)||R<=0)return false;
    const FVector P(A.X-K.X,A.Y-K.Y,0),D(B.X-A.X,B.Y-A.Y,0);
    if(P.SizeSquared()<R*R)return FVector::DotProduct(P,D)>=-1.e-6; // derivative of squared separation is nondecreasing on a segment
    return FMath::PointDistToSegment(FVector::ZeroVector,P,P+D)>=R-1.e-6;
}
bool CanBind(bool O,bool D,bool W,bool Direct,bool A,bool N){return O&&D&&W&&Direct&&A&&N;}
}
