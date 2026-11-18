#include "Encounters/Pedro/PedroNavigationPolicy.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Encounters/Pedro/PedroObserverCharacter.h"
#include "Encounters/Pedro/PedroObserverAIController.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include <limits>
#define PEDRO_NAV_UNIT(Name) \
    IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPedroNavigation##Name##Test, "AI_Gavin_Project.Pedro.Navigation." #Name, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
    bool FPedroNavigation##Name##Test::RunTest(const FString&)
namespace PedroNavigationUnit
{
using namespace PedroNavigationPolicy;
using Mode=EPedroNavigationMode;using Reason=EPedroNavigationReason;
struct FNavUnitFixture
{
    APawn* Player=NewObject<APedroObserverCharacter>(GetTransientPackage());
    FPedroAwarenessState A;
    FPedroNavigationSettings S;
    FPedroNavIntent I;
    FNavUnitFixture(){A.AcceptSighting(Player,FVector(900,0,0),10);}
    FPedroNavigationDecision Get(double T=10,bool Enabled=true,bool Observer=true)const{return Select(Enabled,Observer,A,Player,FVector::ZeroVector,T,I,S);}
    void See(double D,double T=10){A.AcceptSighting(Player,FVector(D,0,0),T);}
    void Active(){I.bActive=true;I.Sighting=A.LastKnownPlayerLocation;I.SightingTime=A.LastSuccessfulSightingTime;I.LastPlan=10;}
};
PEDRO_NAV_UNIT(Disabled){FNavUnitFixture F;auto* C=NewObject<APedroObserverAIController>(GetTransientPackage());return TestTrue(TEXT("default disabled"),F.Get(10,false).Mode==Mode::Disabled&&!F.Get(10,false).bPlan&&!C->IsNavigationRunning()&&!C->GetPedroMoveId().IsValid());}
PEDRO_NAV_UNIT(InvalidObserver){FNavUnitFixture F;return TestTrue(TEXT("unpossessed"),F.Get(10,true,false).Mode==Mode::Disabled&&!F.Get(10,true,false).bPlan);}
PEDRO_NAV_UNIT(NoKnowledge){FNavUnitFixture F;F.A.Reset();return TestTrue(TEXT("empty awareness"),F.Get().Mode==Mode::Idle&&!F.Get().bPlan);}
PEDRO_NAV_UNIT(FreshObservedTarget){FNavUnitFixture F;return TestTrue(TEXT("valid fresh plan"),F.Get().Mode==Mode::FollowObserved&&F.Get().bPlan);}
PEDRO_NAV_UNIT(InvalidTarget){FNavUnitFixture F;auto* Other=NewObject<APedroObserverCharacter>(GetTransientPackage());return TestTrue(TEXT("identity mismatch"),Select(true,true,F.A,nullptr,FVector::ZeroVector,10,F.I,F.S).Mode==Mode::Idle&&Select(true,true,F.A,Other,FVector::ZeroVector,10,F.I,F.S).Mode==Mode::Idle);}
PEDRO_NAV_UNIT(InvalidLocation){FNavUnitFixture F;F.A.LastKnownPlayerLocation=FAISystem::InvalidLocation;bool B=!F.Get().bPlan;F.A.LastKnownPlayerLocation.X=std::numeric_limits<double>::quiet_NaN();return TestTrue(TEXT("invalid location"),B&&!F.Get().bPlan);}
PEDRO_NAV_UNIT(InvalidTime){FNavUnitFixture F;return TestTrue(TEXT("future and nonfinite time"),!F.Get(9).bPlan&&!F.Get(-1).bPlan&&!F.Get(std::numeric_limits<double>::infinity()).bPlan);}
PEDRO_NAV_UNIT(InvalidSettings){FNavUnitFixture F;F.S.StartDistance=F.S.StopDistance;bool B=!F.Get().bPlan;F.S=FPedroNavigationSettings{}; F.S.Speed=0;return TestTrue(TEXT("unsafe configuration"),B&&!F.Get().bPlan);}
PEDRO_NAV_UNIT(StartDistance){FNavUnitFixture F;F.See(649);bool B=F.Get().Mode==Mode::Idle;F.See(650);return TestTrue(TEXT("inclusive start"),B&&F.Get().bPlan);}
PEDRO_NAV_UNIT(StopDistance){FNavUnitFixture F;F.Active();F.See(501);bool B=F.Get().Mode==Mode::FollowObserved;F.See(500);return TestTrue(TEXT("inclusive stop"),B&&F.Get().Mode==Mode::Idle);}
PEDRO_NAV_UNIT(Hysteresis){FNavUnitFixture F;F.See(575);bool B=F.Get().Mode==Mode::Idle;F.Active();return TestTrue(TEXT("active request retained inside band"),B&&F.Get().Mode==Mode::FollowObserved&&!F.Get().bPlan);}
PEDRO_NAV_UNIT(PreferredDestination){FNavUnitFixture F;auto C=Candidates(FVector::ZeroVector,F.A.LastKnownPlayerLocation,F.S);return TestTrue(TEXT("offset from player"),C.Num()==5&&C[0].Equals(FVector(450,0,0),.01));}
PEDRO_NAV_UNIT(PersonalSpace){FNavUnitFixture F;F.See(200);return TestTrue(TEXT("no approach inside personal space"),F.Get().Reason==Reason::TooClose&&!AcceptProjection(FVector(450,0,0),FVector(520,0,0),FVector(750,0,0),F.S));}
PEDRO_NAV_UNIT(DegenerateGeometry){FNavUnitFixture F;return TestTrue(TEXT("reject coincidence and height"),Candidates(FVector::ZeroVector,FVector::ZeroVector,F.S).IsEmpty()&&Candidates(FVector::ZeroVector,FVector(900,0,500),F.S).IsEmpty());}
PEDRO_NAV_UNIT(CandidateGeneration){FNavUnitFixture F;auto C=Candidates(FVector::ZeroVector,F.A.LastKnownPlayerLocation,F.S);bool B=C.Num()==5;for(const auto& P:C)B&=FMath::IsNearlyEqual(FVector::Dist2D(P,F.A.LastKnownPlayerLocation),450.,.01);return TestTrue(TEXT("five ordered standoff points"),B&&C[1].Y<0&&C[2].Y>0&&C[3].X>C[1].X);}
PEDRO_NAV_UNIT(CandidateRanking){return TestTrue(TEXT("direct preferred; then cost and stable index"),BetterCandidate(0,900,1,200)&&!BetterCandidate(1,200,0,900)&&BetterCandidate(2,100,1,200)&&BetterCandidate(1,100,2,100)&&!BetterCandidate(2,100,1,100));}
PEDRO_NAV_UNIT(ProjectionLimits){FNavUnitFixture F;const FVector C(450,0,0);return TestTrue(TEXT("bounded projection"),AcceptProjection(C,FVector(500,0,-80),F.A.LastKnownPlayerLocation,F.S)&&!AcceptProjection(C,FVector(551,0,0),F.A.LastKnownPlayerLocation,F.S)&&!AcceptProjection(C,FVector(450,0,181),F.A.LastKnownPlayerLocation,F.S));}
PEDRO_NAV_UNIT(PartialAndExcessivePaths){FNavUnitFixture F;return TestTrue(TEXT("only complete grounded bounded paths"),AcceptPath(true,false,2200,F.S)&&!AcceptPath(true,false,2201,F.S)&&!AcceptPath(false,false,100,F.S)&&!AcceptPath(true,true,100,F.S));}
PEDRO_NAV_UNIT(RetainExistingRequest){FNavUnitFixture F;F.Active();F.See(920,10.4);return TestTrue(TEXT("small displacement does not replan"),F.Get(10.4).Mode==Mode::FollowObserved&&!F.Get(10.4).bPlan);}
PEDRO_NAV_UNIT(ReplanThresholds){FNavUnitFixture F;F.Active();F.See(1025,10.2);bool B=!F.Get(10.2).bPlan;F.See(1024,10.4);B&=!F.Get(10.4).bPlan;F.See(1025,10.5);return TestTrue(TEXT("time and displacement both required"),B&&F.Get(10.5).bPlan);}
PEDRO_NAV_UNIT(ContinueLastKnown){FNavUnitFixture F;F.Active();F.A.LoseSight();bool B=F.Get(11).Mode==Mode::ContinueLastKnown&&!F.Get(11).bPlan;F.I.bActive=false;return TestTrue(TEXT("never plan on loss"),B&&F.Get(11).Mode==Mode::Idle&&!F.Get(11).bPlan);}
PEDRO_NAV_UNIT(StaleAndExpiredKnowledge){FNavUnitFixture F;F.Active();return TestTrue(TEXT("stale hold then expire"),F.Get(10.36).Mode==Mode::ContinueLastKnown&&!F.Get(10.36).bPlan&&F.Get(12).Reason==Reason::Expired&&F.A.bHasLastKnownPlayerLocation);}
PEDRO_NAV_UNIT(FailureCooldown){FNavUnitFixture F;F.I.RetryAfter=11;F.See(900,10.9);bool B=F.Get(10.9).Reason==Reason::Cooldown;F.See(900,11);return TestTrue(TEXT("retry and stall boundary"),B&&F.Get(11).bPlan&&!Stalled(1.24,0,F.S)&&Stalled(1.25,19,F.S)&&!Stalled(1.25,20,F.S));}
PEDRO_NAV_UNIT(RequestOwnership){return TestTrue(TEXT("exact IDs only"),OwnsRequest(FAIRequestID(17),FAIRequestID(17))&&!OwnsRequest(FAIRequestID(17),FAIRequestID(18))&&!OwnsRequest(FAIRequestID::InvalidRequest,FAIRequestID::InvalidRequest)&&!OwnsRequest(FAIRequestID::AnyRequest,FAIRequestID(17)));}
}
#undef PEDRO_NAV_UNIT
#endif
