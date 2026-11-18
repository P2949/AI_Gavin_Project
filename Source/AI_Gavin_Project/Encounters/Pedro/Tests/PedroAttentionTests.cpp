#include "Encounters/Pedro/PedroAttentionPolicy.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Encounters/Pedro/PedroObserverAIController.h"
#include "Encounters/Pedro/PedroObserverCharacter.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include <limits>

#define PEDRO_ATTENTION_UNIT(Name) \
    IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPedroAttention##Name##Test, \
        "AI_Gavin_Project.Pedro.Attention." #Name, \
        EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
    bool FPedroAttention##Name##Test::RunTest(const FString& /*Parameters*/)

namespace PedroAttentionUnit
{
using E = EPedroAttentionMode;
using namespace PedroAttentionPolicy;
struct FFixture
{
    APawn* Player = NewObject<APedroObserverCharacter>(GetTransientPackage());
    FPedroAwarenessState S;
    FPedroAttentionSettings Settings;
    FPedroAttentionDecision Decide(double Time=10.0, bool bEnabled=true, bool bPossessed=true) const
    { return Select(bEnabled,bPossessed,S,Player,Time,25,Settings); }
    bool Sight(const FVector& At=FVector(450,100,20), double Time=10.0)
    { return S.AcceptSighting(Player,At,Time); }
};
bool Near(double A, double B) { return FMath::IsNearlyEqual(A,B,1.e-6); }
bool IsMode(const FPedroAttentionDecision& D,E Mode,bool bPoint)
{ return D.Mode==Mode && D.LookPoint.IsSet()==bPoint; }

PEDRO_ATTENTION_UNIT(Disabled)
{
    FFixture F; F.Sight();
    auto* C=NewObject<APedroObserverAIController>(GetTransientPackage());
    return TestTrue(TEXT("disabled inputs and controller default"),IsMode(F.Decide(10,false),E::Disabled,false) &&
        IsMode(F.Decide(10,true,false),E::Disabled,false) && !C->IsAttentionRunning() &&
        C->GetAttentionDecision().Mode==E::Disabled);
}
PEDRO_ATTENTION_UNIT(IdleWithoutKnowledge)
{
    FFixture F; const auto D=F.Decide();
    return TestTrue(TEXT("idle at authored heading"),IsMode(D,E::Idle,false) && D.IdleYaw==25);
}
PEDRO_ATTENTION_UNIT(TrackFreshSighting)
{
    FFixture F; F.Sight();const auto D=F.Decide(10.1);
    return TestTrue(TEXT("only legitimate point"),IsMode(D,E::TrackObserved,true) && D.LookPoint.GetValue()==F.S.LastKnownPlayerLocation);
}
PEDRO_ATTENTION_UNIT(UpdateObservedTarget)
{
    FFixture F;F.Sight();const auto Before=F.Decide(); F.Sight(FVector(500,-100,10),10.2);const auto After=F.Decide(10.2);
    return TestTrue(TEXT("new legitimate location selected"),Before.LookPoint.IsSet() && After.LookPoint.IsSet() &&
        Before.LookPoint.GetValue()!=After.LookPoint.GetValue() && After.LookPoint.GetValue()==F.S.LastKnownPlayerLocation);
}
PEDRO_ATTENTION_UNIT(HoldLastKnown)
{
    FFixture F;F.Sight();F.S.LoseSight();const auto D=F.Decide(11);
    return TestTrue(TEXT("hold uses unchanged memory"),IsMode(D,E::HoldLastKnown,true) &&
        D.LookPoint.GetValue()==F.S.LastKnownPlayerLocation && F.S.LastSuccessfulSightingTime==10);
}
PEDRO_ATTENTION_UNIT(HoldExpiry)
{
    FFixture F;F.Sight();F.S.LoseSight();
    return TestTrue(TEXT("half-open persistence interval"),IsMode(F.Decide(11.999),E::HoldLastKnown,true) &&
        IsMode(F.Decide(12),E::Idle,false) && IsMode(F.Decide(40),E::Idle,false) && F.S.bHasLastKnownPlayerLocation);
}
PEDRO_ATTENTION_UNIT(StalePerceived)
{
    FFixture F;F.Sight();
    return TestTrue(TEXT("stale success expires too"),IsMode(F.Decide(10.34),E::TrackObserved,true) &&
        IsMode(F.Decide(10.36),E::HoldLastKnown,true) && IsMode(F.Decide(12),E::Idle,false));
}
PEDRO_ATTENTION_UNIT(Reacquire)
{
    FFixture F;F.Sight();F.S.LoseSight(); F.Sight(FVector(100,20,30),13);
    return TestTrue(TEXT("tracking resumes from new observation"),IsMode(F.Decide(13),E::TrackObserved,true));
}
PEDRO_ATTENTION_UNIT(InvalidTarget)
{
    FFixture F;F.Sight();auto* Other=NewObject<APedroObserverCharacter>(GetTransientPackage());
    return TestTrue(TEXT("missing/replaced target ignored"),IsMode(Select(true,true,F.S,nullptr,10,0,F.Settings),E::Idle,false) &&
        IsMode(Select(true,true,F.S,Other,10,0,F.Settings),E::Idle,false));
}
PEDRO_ATTENTION_UNIT(InvalidLocation)
{
    FFixture F;F.Sight(); F.S.LastKnownPlayerLocation=FAISystem::InvalidLocation;
    const bool bInvalid=IsMode(F.Decide(),E::Idle,false);
    // Avoid constructing an invalid FVector via UE diagnostic constructors.
    F.S.LastKnownPlayerLocation.X=std::numeric_limits<double>::quiet_NaN();
    return TestTrue(TEXT("invalid/sentinel point ignored"),bInvalid && IsMode(F.Decide(),E::Idle,false));
}
PEDRO_ATTENTION_UNIT(InvalidTime)
{
    FFixture F;F.Sight();const double Inf=std::numeric_limits<double>::infinity();
    const bool bNow=IsMode(F.Decide(Inf),E::Disabled,false) && IsMode(F.Decide(-1),E::Disabled,false) && IsMode(F.Decide(9),E::Idle,false);
    F.S.LastSuccessfulSightingTime=Inf;
    return TestTrue(TEXT("invalid/backward clocks"),bNow && IsMode(F.Decide(),E::Idle,false));
}
PEDRO_ATTENTION_UNIT(InvalidSettings)
{
    FFixture F;F.Sight(); F.Settings.TrackDegreesPerSecond=-1;
    const bool bSpeed=IsMode(F.Decide(),E::Disabled,false) && TurnStep(40,0.1,90,F.Settings)==0;
    F.Settings=FPedroAttentionSettings{};F.Settings.FreshSeconds=F.Settings.PersistenceSeconds;
    return TestTrue(TEXT("invalid tuning rejected"),bSpeed && IsMode(F.Decide(),E::Disabled,false));
}
PEDRO_ATTENTION_UNIT(TurnLeft)
{
    const auto Error=LookError(FVector::ZeroVector,FVector(0,0,64),FVector::ForwardVector,FVector(500,-500,0),10);
    return TestTrue(TEXT("negative yaw"),Error.IsSet() && Near(Error.GetValue(),-45) && Near(TurnStep(Error.GetValue(),0.1,90,{}),-9));
}
PEDRO_ATTENTION_UNIT(TurnRight)
{
    const auto Error=LookError(FVector::ZeroVector,FVector(0,0,64),FVector::ForwardVector,FVector(500,500,0),10);
    return TestTrue(TEXT("positive yaw"),Error.IsSet() && Near(Error.GetValue(),45) && Near(TurnStep(Error.GetValue(),0.1,90,{}),9));
}
PEDRO_ATTENTION_UNIT(ShortestPath)
{
    return TestTrue(TEXT("wrap both directions"),Near(ShortestError(170,-170),20) && Near(ShortestError(-170,170),-20) && Near(ShortestError(350,10),20));
}
PEDRO_ATTENTION_UNIT(OppositeHeading)
{
    return TestTrue(TEXT("deterministic positive 180"),Near(ShortestError(0,180),180) && Near(ShortestError(0,-180),180));
}
PEDRO_ATTENTION_UNIT(DeadZone)
{
    return TestTrue(TEXT("inclusive dead zone"),TurnStep(2,0.1,90,{})==0 && TurnStep(-2,0.1,90,{})==0 && Near(TurnStep(2.1,0.1,90,{}),2.1));
}
PEDRO_ATTENTION_UNIT(SpeedAndOvershoot)
{
    FPedroAttentionSettings Settings; Settings.DeadZoneDegrees=0;
    const auto Simulate=[&](int32 FPS,int32 Frames,double Target){double Yaw=0;for(int32 I=0;I<Frames;++I) Yaw+=TurnStep(Target-Yaw,1.0/FPS,90,Settings);return Yaw;};
    double Mixed=0;for(double Dt:{0.017,0.031,0.009,0.082,0.055,0.1,0.1,0.1,0.1,0.1}) Mixed+=TurnStep(60-Mixed,Dt,90,Settings);
    return TestTrue(TEXT("30/60/120 and variable game-time intervals"),Near(Simulate(30,30,60),60) && Near(Simulate(60,60,60),60) &&
        Near(Simulate(120,120,60),60) && Near(Simulate(30,15,90),45) && Near(Simulate(60,30,90),45) && Near(Simulate(120,60,90),45) && Near(Mixed,60) && Near(TurnStep(4,0.1,90,Settings),4) && Near(TurnStep(-80,0.1,45,Settings),-4.5));
}
PEDRO_ATTENTION_UNIT(InvalidDeltaTime)
{
    const double Inf=std::numeric_limits<double>::infinity();
    return TestTrue(TEXT("reject invalid dt; clamp hitch"),TurnStep(90,0,90,{})==0 && TurnStep(90,-1,90,{})==0 &&
        TurnStep(90,Inf,90,{})==0 && Near(TurnStep(90,1,90,{}),9));
}
PEDRO_ATTENTION_UNIT(DegenerateGeometry)
{
    const FVector Root(10,20,30),Eye=Root+FVector(20,80,64),Point=Root+FVector(500,300,0);
    const FVector Forward=FRotator(0,10,0).Vector();
    const auto Error=LookError(Root,Eye,Forward,Point,10);
    if (!TestTrue(TEXT("offset ray reachable"),Error.IsSet())) return false;
    const FVector NewEye=Root+(Eye-Root).RotateAngleAxis(Error.GetValue(),FVector::UpVector);
    const FVector NewForward=Forward.RotateAngleAxis(Error.GetValue(),FVector::UpVector);
    const auto Residual=LookError(Root,NewEye,NewForward,Point,10);
    return TestTrue(TEXT("exact offset convergence; safe degeneracies"),Residual.IsSet() && FMath::Abs(Residual.GetValue())<1.e-5 &&
        !LookError(Root,Eye,FVector::UpVector,Point,10).IsSet() && !LookError(Root,Eye,Forward,Eye,10).IsSet() &&
        !LookError(Root,Eye,Forward,Root,10).IsSet());
}
}
#undef PEDRO_ATTENTION_UNIT
#endif
