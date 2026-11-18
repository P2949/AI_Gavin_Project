#include "Encounters/Pedro/PedroNavigationPolicy.h"

bool FPedroNavigationSettings::IsValid() const
{
    for(double V:{StartDistance,StopDistance,PreferredDistance,PersonalSpace,Speed,Acceleration,Braking,AcceptanceRadius,
                  ReplanInterval,ReplanDisplacement,FreshAge,HoldAge,RetryInterval,MaxPathLength,StallWindow,StallDistance,MaxDuration,
                  ProjectionXY,ProjectionZ,MaxVerticalSeparation})
        if (!FMath::IsFinite(V)||V<=0) return false;
    return StartDistance>StopDistance && StopDistance>PreferredDistance &&
        PreferredDistance>PersonalSpace+AcceptanceRadius+ProjectionXY && HoldAge>FreshAge && MaxDuration>StallWindow;
}
namespace PedroNavigationPolicy
{
using M=EPedroNavigationMode; using R=EPedroNavigationReason;
static bool ValidPoint(const FVector& P) {return !P.ContainsNaN()&&FAISystem::IsValidLocation(P);}
FPedroNavigationDecision Select(bool bEnabled,bool bObserver,const FPedroAwarenessState& A,
    const APawn* Player,const FVector& O,double Now,const FPedroNavIntent& I,const FPedroNavigationSettings& S)
{
    if(!bEnabled)return {M::Disabled,R::Disabled,false};
    if(!bObserver)return {M::Disabled,R::NoObserver,false};
    if(!S.IsValid()||!ValidPoint(O)||!FMath::IsFinite(Now)||Now<0||!FMath::IsFinite(I.LastPlan)||!FMath::IsFinite(I.RetryAfter))return {M::Idle,R::Invalid,false};
    if(!IsValid(Player)||!A.bHasLastKnownPlayerLocation||A.ObservedPlayer.Get()!=Player||A.Status==EPedroAwarenessStatus::Unaware)return {M::Idle,R::NoKnowledge,false};
    if(!ValidPoint(A.LastKnownPlayerLocation)||!FMath::IsFinite(A.LastSuccessfulSightingTime)||A.LastSuccessfulSightingTime<0||A.LastSuccessfulSightingTime>Now)return {M::Idle,R::Invalid,false};
    const double Age=Now-A.LastSuccessfulSightingTime;
    if(Age>=S.HoldAge)return {M::Idle,R::Expired,false};
    const double D=FVector::Dist2D(O,A.LastKnownPlayerLocation);
    if(D<=S.PersonalSpace)return {M::Idle,R::TooClose,false};
    if(D<=S.StopDistance||(!I.bActive&&D<S.StartDistance))return {M::Idle,R::InBand,false};
    if(A.Status!=EPedroAwarenessStatus::Perceived||Age>S.FreshAge)
        return I.bActive?FPedroNavigationDecision{M::ContinueLastKnown,R::Remembered,false}:FPedroNavigationDecision{M::Idle,R::NoKnowledge,false};
    if(FMath::Abs(O.Z-A.LastKnownPlayerLocation.Z)>S.MaxVerticalSeparation)return {M::Blocked,R::NoRoute,false};
    if(Now<I.RetryAfter)return {M::Blocked,R::Cooldown,false};
    bool bPlan=I.LastPlan<0||Now-I.LastPlan>=S.ReplanInterval;
    if(I.bActive)bPlan=bPlan&&FVector::Dist(A.LastKnownPlayerLocation,I.Sighting)>=S.ReplanDisplacement;
    return {M::FollowObserved,R::Fresh,bPlan};
}
TArray<FVector> Candidates(const FVector& O,const FVector& P,const FPedroNavigationSettings& S)
{
    TArray<FVector> Out;
    if(!S.IsValid()||!ValidPoint(O)||!ValidPoint(P)||FMath::Abs(O.Z-P.Z)>S.MaxVerticalSeparation)return Out;
    const FVector Direction=(O-P).GetSafeNormal2D();
    if(Direction.IsNearlyZero())return Out;
    for(double Angle:{0.,30.,-30.,60.,-60.})Out.Add(P+Direction.RotateAngleAxis(Angle,FVector::UpVector)*S.PreferredDistance);
    return Out;
}
bool AcceptProjection(const FVector& C,const FVector& P,const FVector& K,const FPedroNavigationSettings& S)
{
    return S.IsValid()&&ValidPoint(C)&&ValidPoint(P)&&ValidPoint(K)&&FVector::Dist2D(C,P)<=S.ProjectionXY&&
        FMath::Abs(C.Z-P.Z)<=S.ProjectionZ&&FVector::Dist2D(P,K)>=S.PersonalSpace+S.AcceptanceRadius;
}
bool AcceptPath(bool bComplete,bool bSpecial,double Length,const FPedroNavigationSettings& S)
{return S.IsValid()&&bComplete&&!bSpecial&&FMath::IsFinite(Length)&&Length>0&&Length<=S.MaxPathLength;}
bool BetterCandidate(int32 I,double C,int32 B,double BC)
{return I>=0&&FMath::IsFinite(C)&&C>=0&&(B<0|| (I==0&&B!=0)||(I!=0&&B!=0&&(C<BC||(C==BC&&I<B))));}
bool OwnsRequest(FAIRequestID A,FAIRequestID B)
{return A.IsValid()&&B.IsValid()&&A.GetID()==B.GetID()&&A.GetID()!=FAIRequestID::AnyRequest.GetID();}
bool Stalled(double E,double D,const FPedroNavigationSettings& S)
{return FMath::IsFinite(E)&&FMath::IsFinite(D)&&E>=S.StallWindow&&D<S.StallDistance;}
}
