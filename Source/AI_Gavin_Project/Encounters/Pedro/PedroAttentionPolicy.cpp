#include "Encounters/Pedro/PedroAttentionPolicy.h"

bool FPedroAttentionSettings::IsValid() const
{
    return FMath::IsFinite(TrackDegreesPerSecond) && TrackDegreesPerSecond > 0 &&
        FMath::IsFinite(IdleDegreesPerSecond) && IdleDegreesPerSecond > 0 &&
        FMath::IsFinite(PersistenceSeconds) && PersistenceSeconds > 0 &&
        FMath::IsFinite(FreshSeconds) && FreshSeconds >= 0 && FreshSeconds < PersistenceSeconds &&
        FMath::IsFinite(DeadZoneDegrees) && DeadZoneDegrees >= 0 && DeadZoneDegrees < 180 &&
        FMath::IsFinite(MinimumHorizontalDistance) && MinimumHorizontalDistance > 0 &&
        FMath::IsFinite(MaxDeltaSeconds) && MaxDeltaSeconds > 0 && MaxDeltaSeconds <= 1;
}

FPedroAttentionDecision PedroAttentionPolicy::Select(bool bEnabled, bool bObserverValid,
    const FPedroAwarenessState& Knowledge, const APawn* CurrentPlayer,
    double Now, double IdleYaw, const FPedroAttentionSettings& Settings)
{
    FPedroAttentionDecision D;
    if (!bEnabled || !bObserverValid || !Settings.IsValid() ||
        !FMath::IsFinite(Now) || Now < 0 || !FMath::IsFinite(IdleYaw)) return D;
    D.Mode = EPedroAttentionMode::Idle;
    D.IdleYaw = FRotator::NormalizeAxis(IdleYaw);
    if (!IsValid(CurrentPlayer) || Knowledge.ObservedPlayer.Get() != CurrentPlayer ||
        !Knowledge.bHasLastKnownPlayerLocation || Knowledge.Status == EPedroAwarenessStatus::Unaware ||
        !FAISystem::IsValidLocation(Knowledge.LastKnownPlayerLocation) || Knowledge.LastKnownPlayerLocation.ContainsNaN() ||
        !FMath::IsFinite(Knowledge.LastSuccessfulSightingTime) || Knowledge.LastSuccessfulSightingTime < 0 ||
        Knowledge.LastSuccessfulSightingTime > Now) return D;
    const double Age = Now - Knowledge.LastSuccessfulSightingTime;
    if (Age >= Settings.PersistenceSeconds) return D;
    D.Mode = Knowledge.Status == EPedroAwarenessStatus::Perceived && Age <= Settings.FreshSeconds
        ? EPedroAttentionMode::TrackObserved : EPedroAttentionMode::HoldLastKnown;
    D.LookPoint = Knowledge.LastKnownPlayerLocation;
    return D;
}

double PedroAttentionPolicy::ShortestError(double From, double To)
{
    if (!FMath::IsFinite(From) || !FMath::IsFinite(To)) return 0;
    // Normalize separately to avoid overflowing a subtraction. Exact 180
    // always takes positive yaw, including the -180 representation.
    return FRotator::NormalizeAxis(FRotator::NormalizeAxis(To) - FRotator::NormalizeAxis(From));
}

TOptional<double> PedroAttentionPolicy::LookError(const FVector& Root, const FVector& Eye,
    const FVector& Forward, const FVector& Point, double MinimumDistance)
{
    if (Root.ContainsNaN() || Eye.ContainsNaN() || Forward.ContainsNaN() || Point.ContainsNaN() ||
        !FAISystem::IsValidLocation(Point) || !FMath::IsFinite(MinimumDistance) || MinimumDistance <= 0) return {};
    const FVector2D F(Forward.X, Forward.Y);
    const FVector2D Q(Eye.X-Root.X, Eye.Y-Root.Y);
    const FVector2D R(Point.X-Root.X, Point.Y-Root.Y);
    const double F2=F.SizeSquared(),Q2=Q.SizeSquared(),R2=R.SizeSquared();
    if (!FMath::IsFinite(F2) || !FMath::IsFinite(Q2) || !FMath::IsFinite(R2) ||
        F2 < 1.e-8 || (R-Q).Size() < MinimumDistance || R.Size() <= Q.Size() + 1.e-3) return {};
    // R=RotateYaw(delta)*(Q+t*Fhat) for t>=0. On the outside of the
    // eye's rotation circle there is one forward solution. Accounting
    // for lateral offset avoids overshooting when the eye orbits the root.
    const FVector2D Unit = F.GetSafeNormal();
    const double Side = Q.X*(-Unit.Y) + Q.Y*Unit.X;
    const double Bearing = FMath::RadiansToDegrees(FMath::Atan2(R.Y,R.X));
    const double Correction = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Side/R.Size(),-1.0,1.0)));
    const double Heading = FMath::RadiansToDegrees(FMath::Atan2(Unit.Y,Unit.X));
    return ShortestError(Heading, Bearing-Correction);
}

double PedroAttentionPolicy::TurnStep(double Error, double DeltaSeconds, double Speed,
    const FPedroAttentionSettings& Settings)
{
    if (!Settings.IsValid() || !FMath::IsFinite(Error) || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0 ||
        !FMath::IsFinite(Speed) || Speed <= 0 || FMath::Abs(Error) <= Settings.DeadZoneDegrees) return 0;
    const double Limit = Speed * FMath::Min(DeltaSeconds,Settings.MaxDeltaSeconds);
    return FMath::Clamp(Error,-Limit,Limit);
}

FPedroAttentionDecision PedroAttentionPolicy::WithContext(const FPedroAttentionDecision& Base,bool bIdle,const TOptional<FVector>& Activity,double IdleYaw)
{
    if(Base.Mode==EPedroAttentionMode::Disabled)return Base;
    if(bIdle){FPedroAttentionDecision D;D.Mode=EPedroAttentionMode::Idle;D.IdleYaw=IdleYaw;return D;}
    if(Activity.IsSet()&&FAISystem::IsValidLocation(Activity.GetValue())&&!Activity.GetValue().ContainsNaN())
    {FPedroAttentionDecision D=Base;D.Mode=EPedroAttentionMode::ObserveActivity;D.LookPoint=Activity;return D;}
    return Base;
}
