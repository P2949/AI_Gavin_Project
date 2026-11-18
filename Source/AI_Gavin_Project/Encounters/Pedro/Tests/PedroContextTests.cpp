#include "Encounters/Pedro/PedroContextPolicy.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Encounters/Pedro/PedroAttentionPolicy.h"
#include "Encounters/Pedro/PedroNavigationPolicy.h"
#include "Encounters/Pedro/PedroObserverAIController.h"
#include "Encounters/Pedro/PedroObserverWatchPoint.h"
#include "Encounters/Pedro/PedroEncounterDirector.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include <limits>
#define PEDRO_CTX_UNIT(Name) \
    IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPedroContext##Name##Test,"AI_Gavin_Project.Pedro.Context." #Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
    bool FPedroContext##Name##Test::RunTest(const FString&)
namespace PedroContextUnit
{
using namespace PedroContextPolicy;
using CM=EPedroContextMode;using CR=EPedroContextReason;using MV=EPedroContextMovementIntent;
struct FContextUnit
{
    FPedroContextInput I;FPedroContextMemory M;FPedroContextSettings S;FPedroContextDecision Prev;
    FContextUnit(){I.bEnabled=I.bValid=I.bFresh=I.bRecent=true;I.Now=10;I.Distance=900;I.Known=FVector(900,0,0);}
    FPedroContextDecision Get()const{return Select(I,M,Prev,S);}
    FPedroEvidenceRecord Event(EPedroEvidenceKind K=EPedroEvidenceKind::InspectedOptionalObject)const
    {FPedroEvidenceRecord R;R.Event.EventId=FGuid::NewGuid();R.Event.Kind=K;R.Event.SubjectId=FName(TEXT("Test"));R.Event.EventLocation=FVector(800,250,0);R.Event.WorldTimeSeconds=I.Now;R.AcquiredAtWorldTimeSeconds=I.Now;R.KnowledgeSource=EPedroKnowledgeSource::DirectObservation;return R;}
    bool Add(EPedroEvidenceKind K=EPedroEvidenceKind::InspectedOptionalObject){return AcceptActivity(M,Event(K),I.Known,I.Now,S);}
    void Advance(double Time){I.Now+=Time;UpdateSenses(M,I,S);}
    FPedroContextCandidate Candidate(const TCHAR* Name=TEXT("A"))const{FPedroContextCandidate C;C.Id=FName(Name);C.Goal=FVector(450,0,0);C.Visibility=.8;C.DistanceFit=.8;C.Interest=.6;C.Priority=.5;C.Cost=.3;C.Penalty=.2;C.bEligible=true;return C;}
};
PEDRO_CTX_UNIT(Disabled){FContextUnit F;F.I.bEnabled=false;return TestTrue(TEXT("disabled no contextual intent"),F.Get().Mode==CM::Disabled);}
PEDRO_CTX_UNIT(InvalidSettings){FContextUnit F;F.S.Interval=0;return TestTrue(TEXT("invalid settings fail closed"),F.Get().Reason==CR::Invalid&&F.Get().MovementIntent==MV::Stop);}
PEDRO_CTX_UNIT(MissingBinding){FContextUnit F;F.I.bValid=false;return TestTrue(TEXT("invalid binding cannot move"),F.Get().Reason==CR::Invalid);}
PEDRO_CTX_UNIT(NoKnowledge){FContextUnit F;F.I.bFresh=F.I.bRecent=false;return TestTrue(TEXT("no pursuit without legitimate knowledge"),F.Get().Mode==CM::ObserveIdle&&F.Get().MovementIntent==MV::Stop);}
PEDRO_CTX_UNIT(FollowFresh){FContextUnit F;return TestTrue(TEXT("distant sight"),F.Get().Mode==CM::FollowPlayer);}
PEDRO_CTX_UNIT(ComfortableDistance){FContextUnit F;F.I.Distance=500;return TestTrue(TEXT("stationary at comfortable distance"),F.Get().MovementIntent==MV::Stop);}
PEDRO_CTX_UNIT(AcceptActivity){FContextUnit F;return TestTrue(TEXT("accepted event triggers activity"),F.Add()&&F.Get().Mode==CM::ObserveActivity&&F.Get().SourceEventId==F.M.Activity.EventId);}
PEDRO_CTX_UNIT(ActivityPriority){FContextUnit F;const bool A=F.Add(EPedroEvidenceKind::DestroyedOwnedProperty);F.Advance(.1);const bool B=F.Add(EPedroEvidenceKind::RepeatedInteraction);return TestTrue(TEXT("higher priority retains focus"),A&&B&&F.M.Activity.Priority==3&&F.M.LastActivity.Priority==1);}
PEDRO_CTX_UNIT(FixedActivity){FContextUnit F;auto R=F.Event();bool A=AcceptActivity(F.M,R,F.I.Known,F.I.Now,F.S);R.Event.EventLocation=FVector(0,0,999);return TestTrue(TEXT("copied record"),A&&F.Get().LookPoint.GetValue().Equals(FVector(800,250,0)));}
PEDRO_CTX_UNIT(DuplicateActivity){FContextUnit F;auto R=F.Event();return TestTrue(TEXT("exact ID once"),AcceptActivity(F.M,R,F.I.Known,F.I.Now,F.S)&&!AcceptActivity(F.M,R,F.I.Known,F.I.Now,F.S)&&F.M.SeenEvents.Num()==1);}
PEDRO_CTX_UNIT(UnsupportedProvenance){FContextUnit F;auto R=F.Event();for(auto P:{EPedroKnowledgeSource::PrototypeAssumed,EPedroKnowledgeSource::Reported,EPedroKnowledgeSource::Inferred,EPedroKnowledgeSource::None}){R.KnowledgeSource=P;if(AcceptActivity(F.M,R,F.I.Known,F.I.Now,F.S))return false;}return TestTrue(TEXT("direct only"),F.M.SeenEvents.IsEmpty());}
PEDRO_CTX_UNIT(ExpiredActivity){FContextUnit F;auto R=F.Event();F.I.Now+=1;return TestTrue(TEXT("stale record rejected"),!AcceptActivity(F.M,R,F.I.Known,F.I.Now,F.S));}
PEDRO_CTX_UNIT(ActivityCap){FContextUnit F;F.Add();for(int I=0;I<8;++I){F.Advance(.4);F.Add();}return TestTrue(TEXT("hard cap and refractory"),F.Get().Mode!=CM::ObserveActivity&&F.M.ActivityResume>=14);}
PEDRO_CTX_UNIT(Persistence)
{
    FContextUnit F;F.I.bCanWatch=true;F.I.SelectedPoint=FName(TEXT("A"));F.I.SelectedGoal=FVector(450,0,0);F.M.EnteredAt=9.8;
    const bool A=F.Get().Mode==CM::FollowPlayer;F.Advance(.4);
    const bool B=TestTrue(TEXT("point selection persistence"),A&&F.Get().Mode==CM::TravelToWatchPoint);
    // Retry is due before the next ordinary context tick. The controller must evaluate
    // eligible points before UpdateNavigation can consume this retry opportunity.
    F.M.EnteredAt=9;F.I.Now=10.99;
    const auto Due=[&](double Next=10.){return WatchEvaluationDue(F.I,F.M,F.S,Next,11.,10.,.35,250.);};
    const bool Before=!Due();F.I.Now=11.;const bool At=Due();
    const bool Cooldown=!Due(11.5);F.I.bWatchActive=true;const bool Active=!Due();F.I.bWatchActive=false;
    F.I.bFresh=false;const bool Hidden=!Due();F.I.bFresh=true;F.I.bAtPoint=true;F.M.LastPointVisit=10.;const bool Dwell=!Due();
    return TestTrue(TEXT("retry boundary grants bounded watch evaluation before normal tick"),Before&&At&&Cooldown&&Active&&Hidden&&Dwell)&&B;
}
PEDRO_CTX_UNIT(PriorityInterrupt){FContextUnit F;F.Add();F.M.CloseSince=9;const bool A=F.Get().Mode==CM::Withdraw;F.M.CloseSince=-1;const bool B=F.Add(EPedroEvidenceKind::AttackedObserver);return TestTrue(TEXT("proximity or simulated accepted attack overrides activity"),A&&B&&F.Get().Mode==CM::Withdraw);}
PEDRO_CTX_UNIT(ObservedDeparture){FContextUnit F;F.Add();F.I.Known.X+=301;F.Advance(.1);return TestTrue(TEXT("visible departure cancels hold"),F.Get().Mode==CM::FollowPlayer);}
PEDRO_CTX_UNIT(RecentLoss){FContextUnit F;F.I.bFresh=false;F.I.bFollowActive=true;return TestTrue(TEXT("retain only existing follow"),F.Get().Mode==CM::HoldLastKnown&&F.Get().MovementIntent==MV::RetainFollow);}
PEDRO_CTX_UNIT(ExpiredLoss){FContextUnit F;F.I.bFresh=F.I.bRecent=false;F.I.bFollowActive=true;return TestTrue(TEXT("never move on expired memory"),F.Get().MovementIntent==MV::Stop);}
PEDRO_CTX_UNIT(WatchPointMetadata){auto* P=NewObject<APedroObserverWatchPoint>(GetTransientPackage());const bool A=!P->IsUsable();P->WatchPointId=FName(TEXT("Point"));const bool B=P->IsUsable();P->BasePriority=2;return TestTrue(TEXT("authored metadata required"),A&&B&&!P->IsUsable()&&!P->PrimaryActorTick.bCanEverTick);}
PEDRO_CTX_UNIT(CandidateEligibility){FContextUnit F;auto C=F.Candidate();C.bEligible=false;return TestTrue(TEXT("hard filter dominates score"),BestCandidate({C},0,10)==INDEX_NONE);}
PEDRO_CTX_UNIT(InvalidPath){FPedroNavigationSettings S;return TestTrue(TEXT("partial, special, excessive paths rejected"),!PedroNavigationPolicy::AcceptPath(false,false,200,S)&&!PedroNavigationPolicy::AcceptPath(true,true,200,S)&&!PedroNavigationPolicy::AcceptPath(true,false,2201,S));}
PEDRO_CTX_UNIT(Score){FContextUnit F;return TestTrue(TEXT("specified formula"),FMath::IsNearlyEqual(PedroContextPolicy::Score(F.Candidate()),55.5,.001));}
PEDRO_CTX_UNIT(Tie){FContextUnit F;return TestTrue(TEXT("stable lexical tie"),BestCandidate({F.Candidate(TEXT("B")),F.Candidate(TEXT("A"))},0,10)==1);}
PEDRO_CTX_UNIT(Improvement){FContextUnit F;return TestTrue(TEXT("inclusive threshold"),BestCandidate({F.Candidate()},45.5,10)==0&&BestCandidate({F.Candidate()},45.51,10)==INDEX_NONE);}
PEDRO_CTX_UNIT(Dwell){FContextUnit F;F.I.bAtPoint=true;F.I.Distance=450;F.M.CurrentPoint=FName(TEXT("A"));F.I.bCanWatch=false;return TestTrue(TEXT("stationary at selected point"),F.Get().Mode==CM::WatchFromPosition&&F.Get().WatchPointId==F.M.CurrentPoint);}
PEDRO_CTX_UNIT(LossEpisode){FContextUnit F;F.Advance(.1);F.I.bFresh=false;F.Advance(.1);F.Advance(.1);return TestTrue(TEXT("edge not frames"),F.M.LossEpisodes==1);}
PEDRO_CTX_UNIT(Decay){FContextUnit F;return TestTrue(TEXT("bounded decay"),Decay(10,10,F.S.MemoryDecay)==1&&Decay(25,10,F.S.MemoryDecay)==0&&Decay(9,10,F.S.MemoryDecay)==0&&FMath::IsNearlyEqual(Decay(17.5,10,15),.5));}
PEDRO_CTX_UNIT(Proximity){FContextUnit F;F.I.Distance=219;F.Advance(.1);const bool A=F.Get().Mode==CM::ObserveIdle;F.Advance(.6);return TestTrue(TEXT("sustained close sight"),A&&F.Get().Mode==CM::Withdraw);}
PEDRO_CTX_UNIT(EscapeSafety){const FVector K=FVector::ZeroVector;return TestTrue(TEXT("escape monotonically without reentry"),EscapeSegment(FVector(100,0,0),FVector(450,0,0),K,250)&&!EscapeSegment(FVector(100,0,0),FVector(50,200,0),K,250)&&!EscapeSegment(FVector(450,0,0),FVector(-450,0,0),K,250));}
PEDRO_CTX_UNIT(NoRetreatRoute){FPedroNavigationSettings S;return TestTrue(TEXT("retreat never bypasses ordinary path eligibility"),!PedroNavigationPolicy::AcceptPath(false,false,500,S)&&!PedroNavigationPolicy::AcceptProjection(FVector(450,0,0),FVector(100,0,0),FVector::ZeroVector,S));}
PEDRO_CTX_UNIT(Disengage){FContextUnit F;F.M.DisengageUntil=13;const bool A=F.Get().Mode==CM::Disengaged;F.Advance(3);return TestTrue(TEXT("bounded disengagement"),A&&F.Get().Mode==CM::FollowPlayer);}
PEDRO_CTX_UNIT(Satisfied){FContextUnit F;F.Add();F.M.bSatisfied=true;F.M.CloseSince=1;return TestTrue(TEXT("successful completion wins"),F.Get().Mode==CM::Satisfied&&F.Get().MovementIntent==MV::Stop);}
// Native object/contract tests. Positive possession and actual Director delivery are world tests.
PEDRO_CTX_UNIT(OptIn){auto* C=NewObject<APedroObserverAIController>(GetTransientPackage());return TestTrue(TEXT("unpossessed controller remains off"),!C->EnableContextualObservation(nullptr,{})&&!C->IsContextualObservationRunning()&&C->GetContextDecision().Mode==CM::Disabled);}
PEDRO_CTX_UNIT(WrongBinding){auto* C=NewObject<APedroObserverAIController>(GetTransientPackage());auto* D=NewObject<APedroEncounterDirector>(GetTransientPackage());return TestTrue(TEXT("wrong observer and world"),!C->EnableContextualObservation(D,{})&&!CanBind(true,true,false,true,true,true)&&!CanBind(true,true,true,false,true,true));}
PEDRO_CTX_UNIT(AcquiredBoundary){FContextUnit F;FPedroObservationState State;auto R=F.Event();const bool A=State.RecordEvidence(R)&&AcceptActivity(F.M,State.EvidenceHistory.Last(),F.I.Known,F.I.Now,F.S);return TestTrue(TEXT("accepted direct record only"),A&&State.GetEvidenceCount()==1&&F.M.Activity.EventId==R.Event.EventId);}
PEDRO_CTX_UNIT(AssumedBoundary){FContextUnit F;FPedroObservationState State;auto R=F.Event();R.KnowledgeSource=EPedroKnowledgeSource::PrototypeAssumed;return TestTrue(TEXT("accepted assumed evidence is not contextual"),State.RecordEvidence(R)&&!AcceptActivity(F.M,State.EvidenceHistory.Last(),F.I.Known,F.I.Now,F.S));}
PEDRO_CTX_UNIT(NoReplay){FContextUnit F;FPedroObservationState State;auto R=F.Event();State.RecordEvidence(R);auto* C=NewObject<APedroObserverAIController>(GetTransientPackage());return TestTrue(TEXT("new context has no history"),C->GetContextMemory().SeenEvents.IsEmpty()&&State.GetEvidenceCount()==1);}
PEDRO_CTX_UNIT(RequestOwnership){auto* C=NewObject<APedroObserverAIController>(GetTransientPackage());return TestTrue(TEXT("stale IDs not owned"),C->GetPedroMovePurpose()==EPedroMovePurpose::None&&!PedroNavigationPolicy::OwnsRequest(FAIRequestID(17),FAIRequestID(18)));}
PEDRO_CTX_UNIT(IndependentReset){FContextUnit A,B;A.Add();auto* C=NewObject<APedroObserverAIController>(GetTransientPackage());C->DisableContextualObservation();return TestTrue(TEXT("memory not shared"),B.M.SeenEvents.IsEmpty()&&A.M.SeenEvents.Num()==1&&C->GetContextMemory().SeenEvents.IsEmpty());}
PEDRO_CTX_UNIT(CommittedResolution){auto* D=NewObject<APedroEncounterDirector>(GetTransientPackage());FContextUnit F;F.M.bSatisfied=D->HasCommittedContextResolution();const bool A=F.Get().Mode==CM::FollowPlayer;F.M.bSatisfied=true;return TestTrue(TEXT("not satisfied until committed"),A&&F.Get().Mode==CM::Satisfied);}
}
#undef PEDRO_CTX_UNIT
#endif
