// Included inside PedroAwarenessRuntime, after the proven navigation fixture.
// All geometry/actors are transient. Never teleport an enabled observer inside a scenario.
using CMode=EPedroContextMode;
using CPurpose=EPedroMovePurpose;
struct FContextFixture
{
    TArray<TWeakObjectPtr<APedroObserverWatchPoint>> Points;
    TWeakObjectPtr<APedroEncounterDirector> OtherDirector;
    TWeakObjectPtr<APedroObserverAIController> LoggedController;
    uint32 LastSelection=0, Submissions=0, Generation=0;
    int32 Losses=0;
    double SeedYaw=0, Started=0, NextSpam=0, Separation=0, Score=0, EndTime=0;
    FVector FixedActivity=FVector::ZeroVector, Before=FVector::ZeroVector;
    FGuid Event;
    FName Point;
    bool bRecovery=false;
};
FContextFixture Ctx;
const TCHAR* ContextModeName(CMode M)
{
    switch(M){case CMode::Disabled:return TEXT("Disabled");case CMode::ObserveIdle:return TEXT("ObserveIdle");case CMode::FollowPlayer:return TEXT("FollowPlayer");case CMode::ObserveActivity:return TEXT("ObserveActivity");case CMode::TravelToWatchPoint:return TEXT("TravelToWatchPoint");case CMode::WatchFromPosition:return TEXT("WatchFromPosition");case CMode::HoldLastKnown:return TEXT("HoldLastKnown");case CMode::Withdraw:return TEXT("Withdraw");case CMode::Disengaged:return TEXT("Disengaged");case CMode::Satisfied:return TEXT("Satisfied");}return TEXT("Invalid");
}
const TCHAR* PurposeName(CPurpose P)
{switch(P){case CPurpose::None:return TEXT("None");case CPurpose::Follow:return TEXT("Follow");case CPurpose::WatchPoint:return TEXT("WatchPoint");case CPurpose::Withdraw:return TEXT("Withdraw");}return TEXT("Invalid");}
const TCHAR* ContextSuite(){return Run.bContextProbe?TEXT("context-probe"):Run.bIntegration?TEXT("context-integration"):TEXT("context");}
CMode ContextMode(){return Run.Controller.IsValid()?Run.Controller->GetContextDecision().Mode:CMode::Disabled;}
CPurpose ContextPurpose(){return Run.Controller.IsValid()?Run.Controller->GetPedroMovePurpose():CPurpose::None;}
const FPedroContextMemory& ContextMemory(){static const FPedroContextMemory Empty;return Run.Controller.IsValid()?Run.Controller->GetContextMemory():Empty;}
bool ContextRunning(){return Run.Controller.IsValid()&&Run.Controller->IsContextualObservationRunning();}
bool ContextFollowing(){return ContextRunning()&&ContextMode()==CMode::FollowPlayer&&NavFollowing()&&ContextPurpose()==CPurpose::Follow;}
bool ContextWatching(){return ContextRunning()&&ContextMode()==CMode::WatchFromPosition&&NavStopped()&&NavSeen()&&!ContextMemory().CurrentPoint.IsNone();}
bool ContextTravel(){return ContextRunning()&&ContextMode()==CMode::TravelToWatchPoint&&NavActive()&&ContextPurpose()==CPurpose::WatchPoint;}
bool ContextActivity(){return ContextRunning()&&ContextMode()==CMode::ObserveActivity&&NavStopped()&&ContextMemory().Activity.EventId.IsValid()&&Run.Controller->GetAttentionDecision().Mode==EPedroAttentionMode::ObserveActivity;}
bool ContextActivityAligned(){return ContextActivity()&&FMath::Abs(Run.Controller->GetAttentionDecision().AngularError)<=3;}
void DestroyContextPoints(){for(const auto& P:Ctx.Points)if(P.IsValid())P->Destroy();Ctx.Points.Reset();}
void CleanContextOther(){DestroyController(Run.SecondController);if(Run.SecondObserver.IsValid())Run.SecondObserver->Destroy();Run.SecondObserver.Reset();if(Ctx.OtherDirector.IsValid())Ctx.OtherDirector->Destroy();Ctx.OtherDirector.Reset();}
APedroObserverWatchPoint* ContextPointAt(const TCHAR* Id,const FVector& WorldFeet,double Yaw,float Priority=1,float Radius=1200)
{
    auto* P=SpawnAt<APedroObserverWatchPoint>(WorldFeet,FRotator(0,Yaw,0));if(!IsValid(P))return nullptr;
    P->WatchPointId=FName(Id);P->BasePriority=Priority;P->ObservationRadius=Radius;Ctx.Points.Add(P);return P;
}
APedroObserverWatchPoint* ContextPoint(const TCHAR* Id,double X,double Y,double Yaw,float Priority=1,float Radius=1200)
{return ContextPointAt(Id,ArenaOrigin+FVector(X,Y,-NavGroundOffset),Yaw,Priority,Radius);}
APedroObserverWatchPoint* FindContextPoint(const TCHAR* Id){for(const auto& P:Ctx.Points)if(P.IsValid()&&P->WatchPointId==FName(Id))return P.Get();return nullptr;}
bool BindContext()
{
    TArray<APedroObserverWatchPoint*> P;for(const auto& W:Ctx.Points)if(W.IsValid())P.Add(W.Get());
    return Run.Controller.IsValid()&&Run.Director.IsValid()&&Run.Controller->EnableContextualObservation(Run.Director.Get(),P)&&ContextRunning();
}
bool ResetContextRig(const FVector& O,const FVector& Player)
{
    CleanContextOther();DestroyContextPoints();
    if(NavTest.PhysicalBlocker.IsValid())NavTest.PhysicalBlocker->Destroy();NavTest.PhysicalBlocker.Reset();
    return NavReset(O,Player,true,true)&&BuildEvidenceRig();
}
bool ContextReset(const FVector& O,const FVector& Player){return ResetContextRig(O,Player)&&BindContext();}
bool ContextFollowSetup(){return ContextReset(FVector::ZeroVector,FVector(1200,0,0))&&Place(Run.A.Get(),ArenaOrigin+FVector(1050,300,64));}
bool ContextWatchSetup(bool bAlternates=false)
{
    if(!ResetContextRig(FVector::ZeroVector,FVector(1000,0,0))||!ContextPoint(TEXT("Primary"),550,0,0))return false;
    if(bAlternates&&(!ContextPoint(TEXT("Similar"),550,140,0,.35f)||!ContextPoint(TEXT("Alternative"),720,300,45,.8f)))return false;
    return BindContext();
}
bool RememberActivity()
{
    if(!Run.Controller.IsValid()||Run.ExpectedAcquired.IsEmpty())return false;
    const auto& A=ContextMemory().Activity;
    if(!A.EventId.IsValid()||A.EventId!=Run.ExpectedAcquired.Last())return false;
    Ctx.FixedActivity=A.ObservedLocation;Ctx.Event=A.EventId;return true;
}
bool FixedActivity(){return Run.Controller.IsValid()&&ContextMemory().LastActivity.EventId==Ctx.Event&&ContextMemory().LastActivity.ObservedLocation.Equals(Ctx.FixedActivity,.01);}
bool ContextRejects(const TCHAR* Text){return Run.Controller.IsValid()&&Run.Controller->GetContextRejections().Contains(FString(Text));}
bool ContextCloseSetup(bool bBlocked=false)
{
    if(!ContextReset(FVector::ZeroVector,FVector(180,0,0)))return false;
    Ctx.Separation=FVector::Dist2D(NavRoot(),Run.Player->GetActorLocation());
    if(bBlocked){NavTest.PhysicalBlocker=NavBox(ArenaOrigin+FVector(-105,0,-NavGroundOffset+35),FVector(50,600,70),false);return NavTest.PhysicalBlocker.IsValid();}
    return true;
}
bool ContextWithdrawing(){return ContextMode()==CMode::Withdraw&&NavActive()&&ContextPurpose()==CPurpose::Withdraw&&NavTest.Travel>20;}
bool ContextWithdrawn(){return ContextMode()==CMode::Disengaged&&NavStopped()&&FVector::Dist2D(NavRoot(),Run.Player->GetActorLocation())>=395&&Ctx.Separation<220;}
void AddContextProbeSteps()
{
    Add(TEXT(""),TEXT("ProbeNavCapability"),PrepareNavigationArena,[]{return NavCapability()&&NavSeen()&&NavStopped();},.25);
    Add(TEXT("P01"),TEXT("ProbeFollow"),ContextFollowSetup,[]{return ContextFollowing()&&NavTest.Travel>65;},.05);
    Add(TEXT("P02"),TEXT("ProbeActivity"),[]{Ctx.SeedYaw=BodyYaw();return Inspect(Run.A.Get(),true)&&RememberActivity();},[]{return ContextActivityAligned()&&EvidenceMatches(1,1)&&FixedActivity()&&FMath::Abs(FMath::FindDeltaAngleDegrees(Ctx.SeedYaw,BodyYaw()))>5;},.1);
    Add(TEXT("P03"),TEXT("ProbeWatchPoint"),[]{return ContextWatchSetup();},[]{return ContextWatching()&&NavTest.Travel>300&&ContextMemory().CurrentPoint==FName(TEXT("Primary"));},.25);
    Add(TEXT("P04"),TEXT("ProbeWithdrawal"),[]{return ContextCloseSetup();},[]{return ContextWithdrawn()&&NavTest.Travel>200&&EvidenceMatches(0,0);},.2);
}
void MakeContextSteps()
{
    if(Run.bContextProbe){AddContextProbeSteps();return;}
    Add(TEXT(""),TEXT("ContextNavCapability"),PrepareNavigationArena,[]{return NavCapability()&&NavSeen()&&NavStopped();},.25);
    Add(TEXT("X01"),TEXT("ContextDisabled"),[]{return NavReset(FVector::ZeroVector,FVector(1000,0,0),true,true);},[]{return NavFollowing()&&NavTest.Travel>60&&!ContextRunning()&&ContextMode()==CMode::Disabled;},.1);
    Add(TEXT("X02"),TEXT("ContextInitiallyHidden"),[]{return ContextReset(FVector::ZeroVector,FVector(-900,0,0));},[]{return ContextRunning()&&NoSighting()&&NavStopped()&&NavSubmissions()==0&&ContextMode()==CMode::ObserveIdle;},.6);
    Add(TEXT("X03"),TEXT("ContextFollows"),ContextFollowSetup,[]{return ContextFollowing()&&NavTest.Travel>80;},.05);
    Add(TEXT("X04"),TEXT("ContextInBand"),[]{return ContextReset(FVector::ZeroVector,FVector(450,0,0));},[]{return NavSeen()&&NavStopped()&&ContextMode()==CMode::ObserveIdle&&NavSubmissions()==0;},.4);
    Add(TEXT(""),TEXT("ActivityBeforeFollow"),ContextFollowSetup,[]{return ContextFollowing()&&NavTest.Travel>65;},.05);
    Add(TEXT("X05"),TEXT("ActivityInterrupt"),[]{Ctx.SeedYaw=BodyYaw();Ctx.Submissions=NavSubmissions();return Inspect(Run.A.Get(),true)&&RememberActivity();},[]{return ContextActivity()&&EvidenceMatches(1,1)&&NavSubmissions()==Ctx.Submissions;},.05);
    Add(TEXT("X06"),TEXT("ActivityTurns"),Noop,[]{return ContextActivityAligned()&&FMath::Abs(FMath::FindDeltaAngleDegrees(Ctx.SeedYaw,BodyYaw()))>5&&FixedActivity();},.05);
    Add(TEXT("X07"),TEXT("HiddenActivityRejected"),[]{return WallForProp(Run.B.Get(),30)&&Inspect(Run.B.Get(),false);},[]{return EvidenceMatches(1,1)&&ContextMemory().SeenEvents.Num()==1&&FixedActivity();},.1);
    Add(TEXT("X08"),TEXT("ActivityFixedLocation"),[]{return WallOff()&&Place(Run.A.Get(),ArenaOrigin+FVector(1100,-300,64));},[]{return ContextActivity()&&FixedActivity()&&!Run.A->GetActorLocation().Equals(Ctx.FixedActivity,.01)&&Run.Controller->GetContextDecision().LookPoint.IsSet()&&Run.Controller->GetContextDecision().LookPoint.GetValue().Equals(Ctx.FixedActivity,.01);},.05);
    Add(TEXT(""),TEXT("SpamReady"),[]{return ContextReset(FVector::ZeroVector,FVector(500,0,0));},[]{return NavSeen()&&NavStopped()&&TraceProp(Run.A.Get())==ETrace::Visible;},.2);
    Add(TEXT("X09"),TEXT("ActivityCap"),[]{Ctx.Started=Run.World->GetTimeSeconds();Ctx.NextSpam=Ctx.Started+.15;Ctx.bRecovery=false;return Inspect(Run.A.Get(),true);},[]{const double Now=Run.World->GetTimeSeconds();if(Now>=Ctx.NextSpam&&Now-Ctx.Started<3.25){Ctx.NextSpam=Now+.15;if(!Inspect(Run.A.Get(),true))return false;}if(Now-Ctx.Started>1&&ContextMode()!=CMode::ObserveActivity)Ctx.bRecovery=true;return Now-Ctx.Started>=3.3&&Ctx.bRecovery&&EvidenceCount()>=10&&SampledCount()==1&&ContextMemory().SeenEvents.Num()==EvidenceCount();},.15);
    Add(TEXT(""),TEXT("DepartureReady"),[]{return ContextReset(FVector::ZeroVector,FVector(450,0,0));},[]{return NavSeen()&&NavStopped()&&TraceProp(Run.A.Get())==ETrace::Visible;},.2);
    Add(TEXT(""),TEXT("DepartureActivity"),[]{return Inspect(Run.A.Get(),true);},ContextActivity,.05);
    Add(TEXT("X10"),TEXT("ObservedDeparture"),[]{return MovePlayer(FVector(1050,0,0));},[]{return ContextFollowing()&&NavTest.Travel>40&&EvidenceMatches(1,1);},.05);
    Add(TEXT("X11"),TEXT("WatchPointSelected"),[]{return ContextWatchSetup();},[]{return ContextTravel()&&Run.Controller->GetPedroMoveWatchPoint()==FName(TEXT("Primary"))&&Run.Controller->GetContextSelectedScore()>=Run.Controller->GetContextCurrentScore()+10;},.05);
    Add(TEXT("X12"),TEXT("WatchPointTravel"),Noop,[]{return ContextTravel()&&NavTest.Travel>100;},.05);
    Add(TEXT("X13"),TEXT("WatchPointArrived"),Noop,[]{return ContextWatching()&&ContextMemory().CurrentPoint==FName(TEXT("Primary"))&&FVector::Dist2D(NavRoot(),FindContextPoint(TEXT("Primary"))->GetActorLocation())<55;},.4);
    Add(TEXT("X14"),TEXT("DisconnectedPointRejected"),[]{return ResetContextRig(FVector(1100,0,0),FVector(2350,0,0))&&ContextPoint(TEXT("Island"),1930,0,0)&&BindContext();},[]{return NavSeen()&&ContextRejects(TEXT("Island:path"))&&Run.Controller->GetContextPathQueries()>0&&Run.Controller->GetContextSelectionSerial()>0&&ContextPurpose()!=CPurpose::WatchPoint&&NavStopped()&&NavSubmissions()==0&&NavTest.Travel<2;},.1);
    Add(TEXT("X15"),TEXT("ViewRejected"),[]{return ResetContextRig(FVector::ZeroVector,FVector(900,0,0))&&ContextPoint(TEXT("WrongView"),450,0,180)&&BindContext();},[]{return NavSeen()&&ContextRejects(TEXT("WrongView:view"))&&ContextPurpose()!=CPurpose::WatchPoint;},.15);
    Add(TEXT(""),TEXT("RankingArrival"),[]{return ContextWatchSetup(true);},[]{return ContextWatching()&&ContextMemory().CurrentPoint==FName(TEXT("Primary"));},.25);
    Add(TEXT("X16"),TEXT("StableRanking"),[]{auto* P=FindContextPoint(TEXT("Similar"));if(!P)return false;P->BasePriority=.4f;Ctx.Submissions=NavSubmissions();return true;},[]{return ContextWatching()&&ContextMemory().CurrentPoint==FName(TEXT("Primary"))&&NavSubmissions()==Ctx.Submissions;},2.2,[]{return ContextPurpose()!=CPurpose::WatchPoint&&NavSubmissions()==Ctx.Submissions;});
    Add(TEXT("X17"),TEXT("ImprovedPosition"),[]{auto* P=FindContextPoint(TEXT("Alternative"));if(!P)return false;P->BasePriority=1;return MovePlayer(FVector(1050,600,0));},[]{return ContextTravel()&&Run.Controller->GetPedroMoveWatchPoint()==FName(TEXT("Alternative"))&&Run.Controller->GetContextSelectedScore()>=Run.Controller->GetContextCurrentScore()+10&&NavSubmissions()>Ctx.Submissions;},.05);
    Add(TEXT(""),TEXT("AdaptationArrival"),Noop,[]{return ContextWatching()&&ContextMemory().CurrentPoint==FName(TEXT("Alternative"));},2.1);
    Add(TEXT(""),TEXT("AdaptationLoseSight"),[]{Ctx.Losses=ContextMemory().LossEpisodes;Ctx.Point=ContextMemory().CurrentPoint;Ctx.Generation=Run.Controller->GetContextSelectionSerial();Ctx.Score=Run.Controller->GetContextCurrentCandidate().Penalty;if(!FreezeMemory())return false;const FVector At=(NavRoot()+Run.Player->GetActorLocation())*.5;return WallAt(At,FVector(80,500,500))&&PlayerRayHitsWall();},[]{return Lost()&&ContextMemory().LossEpisodes==Ctx.Losses+1&&NavStopped()&&ContextMemory().LossPoint==Ctx.Point;},.5,MemoryUnchanged);
    Add(TEXT("X18"),TEXT("AdaptedPreference"),WallOff,[]{const auto* P=ContextMemory().Points.Find(Ctx.Point);return NavSeen()&&P&&P->Losses==1&&ContextMemory().LossEpisodes==Ctx.Losses+1&&Run.Controller->GetContextSelectionSerial()>Ctx.Generation&&Run.Controller->GetContextCurrentCandidate().Penalty>Ctx.Score;},.3);
    Add(TEXT(""),TEXT("HiddenAdaptationReady"),[]{return ContextReset(FVector::ZeroVector,FVector(500,0,0));},[]{return NavSeen()&&NavStopped()&&TraceProp(Run.A.Get())==ETrace::Visible;},.2);
    Add(TEXT(""),TEXT("KnownActivityForAdaptation"),[]{return Inspect(Run.A.Get(),true)&&RememberActivity();},ContextActivity,.05);
    Add(TEXT("X19"),TEXT("HiddenAdaptationRejected"),[]{Ctx.Losses=ContextMemory().LossEpisodes;return WallForProp(Run.B.Get(),30)&&Inspect(Run.B.Get(),false);},[]{return NavSeen()&&FixedActivity()&&ContextMemory().LossEpisodes==Ctx.Losses&&ContextMemory().SeenEvents.Num()==1&&EvidenceMatches(1,1);},.2);
    Add(TEXT(""),TEXT("DecayLoseSight"),[]{return WallOff()&&FreezeMemory()&&BroadWall()&&PlayerRayHitsWall();},[]{return Lost()&&NavStopped();},.4,MemoryUnchanged);
    Add(TEXT("X20"),TEXT("AdaptationDecays"),WallOff,[]{return NavSeen()&&NavStopped()&&ContextMemory().LastLoss>=0&&Run.World->GetTimeSeconds()-ContextMemory().LastLoss>=15.5&&Run.Controller->GetContextCurrentCandidate().Penalty==0&&EvidenceMatches(1,1);},.2);
    Add(TEXT(""),TEXT("LossFollowReady"),[]{return ContextReset(FVector::ZeroVector,FVector(1300,0,0));},[]{return ContextFollowing()&&NavTest.Travel>60;},.05);
    Add(TEXT("X21"),TEXT("RetainFollowOnLoss"),[]{return NavRemember()&&NavCover()&&PlayerRayHitsWall();},[]{return Lost()&&NavActive()&&ContextMode()==CMode::HoldLastKnown&&ContextPurpose()==CPurpose::Follow&&NavFrozenRoute();},.15,NavFrozenRoute);
    Add(TEXT(""),TEXT("LossWatchReady"),[]{return ContextWatchSetup();},[]{return ContextTravel()&&NavTest.Travel>70;},.05);
    Add(TEXT("X22"),TEXT("CancelWatchOnLoss"),[]{Ctx.Submissions=NavSubmissions();return FreezeMemory()&&NavCover()&&PlayerRayHitsWall();},[]{return Lost()&&NavStopped()&&ContextMode()==CMode::HoldLastKnown&&NavSubmissions()==Ctx.Submissions;},.2,MemoryUnchanged);
    Add(TEXT("X23"),TEXT("ReacquireContext"),WallOff,[]{return NavSeen()&&NavActive()&&NavSubmissions()>Ctx.Submissions&&Run.Controller->GetNavigationIntent().SightingTime>Run.Frozen.LastSuccessfulSightingTime;},.1);
    Add(TEXT(""),TEXT("WithdrawalMoving"),[]{return ContextCloseSetup();},ContextWithdrawing,.1);
    Add(TEXT("X24"),TEXT("WithdrawSafely"),Noop,[]{return ContextWithdrawn()&&NavTest.Travel>100&&EvidenceMatches(0,0);},.2);
    Add(TEXT("X25"),TEXT("ObstructedWithdrawal"),[]{return ContextCloseSetup(true);},[]{return ContextMode()==CMode::Disengaged&&NavStopped()&&FVector::Dist2D(NavRoot(),Run.Player->GetActorLocation())<395&&NavSubmissions()>0&&EvidenceMatches(0,0);},.2);
    Add(TEXT("X26"),TEXT("DisengagementRecovery"),[]{if(NavTest.PhysicalBlocker.IsValid())NavTest.PhysicalBlocker->Destroy();NavTest.PhysicalBlocker.Reset();Ctx.EndTime=ContextMemory().DisengageUntil;return MovePlayer(FVector(1000,0,0));},[]{return ContextFollowing()&&Run.World->GetTimeSeconds()>=Ctx.EndTime&&NavTest.Travel>30;},.1);
    Add(TEXT(""),TEXT("DisableContext"),[]{Run.Controller->DisableContextualObservation();return !ContextRunning()&&!Run.Controller->GetPedroMoveId().IsValid();},[]{return ContextMode()==CMode::Disabled&&NavFollowing()&&ContextMemory().SeenEvents.IsEmpty();},.1);
    Add(TEXT("X27"),TEXT("ReenableContext"),BindContext,[]{return ContextFollowing()&&ContextMemory().SeenEvents.IsEmpty();},.1);
    Add(TEXT(""),TEXT("LifecycleReady"),[]{return ContextReset(FVector::ZeroVector,FVector(500,0,0));},[]{return NavSeen()&&NavStopped()&&TraceProp(Run.A.Get())==ETrace::Visible;},.2);
    Add(TEXT(""),TEXT("LifecycleEvidence"),[]{return Inspect(Run.A.Get(),true);},ContextActivity,.05);
    Add(TEXT(""),TEXT("ReplaceContextPlayer"),[]{Ctx.Generation=Run.Controller->GetContextGeneration();auto* Old=Run.Player.Get();if(!IsValid(Old)||!Old->Destroy())return false;auto* P=SpawnObserver(ArenaOrigin+FVector(-700,0,0),FRotator::ZeroRotator);if(!IsValid(P))return false;Run.PC->Possess(P);Run.Player=P;P->GetCapsuleComponent()->SetCanEverAffectNavigation(false);return Freeze(P);},[]{return EmptyAwareness()&&NavStopped()&&ContextMemory().SeenEvents.IsEmpty()&&Run.Controller->GetContextGeneration()>Ctx.Generation&&EvidenceMatches(1,1);},.3);
    Add(TEXT(""),TEXT("UnpossessContext"),[]{Run.Controller->UnPossess();return true;},[]{return !ContextRunning()&&ContextMode()==CMode::Disabled&&NavStopped()&&ContextMemory().SeenEvents.IsEmpty();},.25);
    Add(TEXT(""),TEXT("RepossessContext"),[]{DestroyController(Run.Controller);Run.Controller=AttachMovable(Run.Observer.Get());if(!Run.Controller.IsValid())return false;Run.Controller->SetAttentionEnabled(true);return NavEnable()&&BindContext();},[]{return ContextRunning()&&ContextMemory().SeenEvents.IsEmpty()&&NavStopped();},.2);
    Add(TEXT("X28"),TEXT("DirectorDestroyed"),[]{return Run.Director.IsValid()&&Run.Director->Destroy();},[]{return !ContextRunning()&&NavStopped()&&ContextMemory().SeenEvents.IsEmpty();},.3);
    Add(TEXT(""),TEXT("TwoObserversReady"),[]{if(!ContextReset(FVector::ZeroVector,FVector(500,0,0)))return false;Run.SecondObserver=SpawnObserver(ArenaOrigin+FVector(0,-400,0),FRotator(0,180,0));Run.SecondController=AttachMovable(Run.SecondObserver.Get());if(!Run.SecondController.IsValid())return false;Run.SecondController->SetAttentionEnabled(true);Run.SecondController->SetNavigationEnabled(true);FTransform At(FRotator::ZeroRotator,ArenaOrigin+FVector(0,0,-500));auto* D=Run.World->SpawnActorDeferred<APedroEncounterDirector>(APedroEncounterDirector::StaticClass(),At,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);if(!IsValid(D))return false;D->SetFlags(RF_Transient);Ctx.OtherDirector=D;D->DirectObservationActor=Run.SecondObserver.Get();D->bUseDirectObservationAcquisition=true;D->FinishSpawning(At);return Run.SecondController->EnableContextualObservation(D,{});},[]{return NavSeen()&&NavStopped()&&Run.SecondController.IsValid()&&Run.SecondController->IsContextualObservationRunning()&&Run.SecondController->GetAwarenessState().Status==EStatus::Unaware;},.3);
    Add(TEXT("X29"),TEXT("IndependentContext"),[]{return Inspect(Run.A.Get(),true);},[]{return ContextActivity()&&EvidenceMatches(1,1)&&Ctx.OtherDirector.IsValid()&&Ctx.OtherDirector->GetObservationState().GetEvidenceCount()==0&&Run.SecondController.IsValid()&&Run.SecondController->GetContextMemory().SeenEvents.IsEmpty();},.2);
    Add(TEXT(""),TEXT("DemoContextFollow"),[]{return ResetContextRig(FVector::ZeroVector,FVector(1200,0,0))&&ContextPoint(TEXT("DemoPoint"),650,750,12,1,800)&&Place(Run.A.Get(),ArenaOrigin+FVector(1050,200,64))&&Place(Run.B.Get(),ArenaOrigin+FVector(1150,350,64))&&BindContext();},[]{return ContextFollowing()&&NavTest.Travel>80;},.05);
    Add(TEXT(""),TEXT("DemoContextActivity"),[]{return Inspect(Run.A.Get(),true)&&RememberActivity();},[]{return ContextActivity()&&EvidenceMatches(1,1);},.15);
    Add(TEXT(""),TEXT("DemoContextHidden"),[]{return FreezeMemory()&&NavCover()&&PlayerRayHitsWall()&&Inspect(Run.B.Get(),false);},[]{return Lost()&&NavStopped()&&EvidenceMatches(1,1)&&FixedActivity();},.25,MemoryUnchanged);
    Add(TEXT(""),TEXT("DemoContextReacquire"),[]{return WallOff()&&MovePlayer(FVector(1100,850,0))&&Place(Run.B.Get(),ArenaOrigin+FVector(1150,900,64));},[]{return ContextTravel()&&Run.Controller->GetPedroMoveWatchPoint()==FName(TEXT("DemoPoint"))&&EvidenceMatches(1,1);},.1);
    Add(TEXT(""),TEXT("DemoContextRelocate"),Noop,[]{return ContextWatching()&&NavTest.Travel>150&&EvidenceMatches(1,1)&&TraceProp(Run.B.Get())==ETrace::Visible;},.25);
    Add(TEXT(""),TEXT("DemoContextNewActivity"),[]{return Inspect(Run.B.Get(),true);},[]{return ContextActivity()&&EvidenceMatches(2,2);},.15);
    Add(TEXT(""),TEXT("DemoContextApproach"),[]{const FVector At=NavRoot()+FRotator(0,BodyYaw(),0).Vector()*180;Ctx.Separation=FVector::Dist2D(NavRoot(),At);return Place(Run.Player.Get(),FVector(At.X,At.Y,Run.Player->GetActorLocation().Z));},ContextWithdrawing,.1);
    Add(TEXT("X30"),TEXT("DemoContextWithdrawal"),Noop,[]{return ContextWithdrawn()&&NavTest.Travel>100&&EvidenceMatches(2,2)&&Run.Emitted.Num()==3;},.3);
}

bool ConfigureSavedContextPoint(bool bCreate)
{
    if(!NavTest.bSavedPair)return false;
    // Pick a goal in the previously validated 480--520 cm landing envelope.
    const auto* Capsule=Run.Observer->GetCapsuleComponent();
    const FVector Direction=(NavTest.SavedSecondLanding-NavTest.SavedSecondGoal).GetSafeNormal2D();
    if(Direction.IsNearlyZero())return false;
    FNavLocation Projected;const FVector Wanted=NavTest.SavedSecondGoal+Direction*30;
    if(!NavProject(Wanted,Projected,FVector(40,40,100))||FVector::Dist2D(Wanted,Projected.Location)>15)return false;
    const FVector At=Projected.Location+FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()+2);
    const double Yaw=SavedFaceYaw(At,NavTest.SavedSecondPlayer);
    if(!ClearSavedCapsule(At,Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight())||
       !SavedSightAt(At,Yaw,NavTest.SavedSecondPlayer,nullptr,1500,70))return false;
    for(auto* Prop:{Run.A.Get(),Run.B.Get(),Run.C.Get()})if(!SavedSightAt(At,Yaw,Prop->GetActorLocation(),Prop,Run.Director->DirectObservationMaxDistance,70))return false;
    auto* Point=bCreate?ContextPointAt(TEXT("SavedSecond"),Projected.Location,Yaw):FindContextPoint(TEXT("SavedSecond"));
    if(!IsValid(Point)||!Place(Point,Projected.Location,FRotator(0,Yaw,0)))return false;
    UE_LOG(LogTemp,Display,TEXT("PEDRO_CONTEXT_FIXTURE savedPoint phase=%s goal=%s yaw=%.3f player=%s"),bCreate?TEXT("planned"):TEXT("actual"),*VectorString(Projected.Location),Yaw,*VectorString(NavTest.SavedSecondPlayer));
    return true;
}
bool SavedContextSettled(){return ContextRunning()&&NavArrived()&&Aligned()&&ContextMode()!=CMode::ObserveActivity&&SavedAllVisible();}
void MakeContextIntegrationSteps()
{
    Add(TEXT("L01"),TEXT("SavedContextConfiguration"),PrepareSavedNavigation,SavedNavigationReady,.25);
    Add(TEXT("L02"),TEXT("SavedContextFollow"),[]{if(!SavedRoutePosition(true)||!ConfigureSavedContextPoint(true))return false;Run.Controller=AttachMovable(Run.Observer.Get());if(!Run.Controller.IsValid())return false;Run.Controller->SetAttentionEnabled(true);if(!NavEnable()||!BindContext())return false;FindContextPoint(TEXT("SavedSecond"))->bEnabled=false;return true;},[]{return SavedContextSettled()&&NavTest.Travel>150&&NavTest.YawTravel>5&&EvidenceMatches(0,0);},.3);
    Add(TEXT("L03"),TEXT("SavedContextStatue"),[]{return Inspect(Run.A.Get(),true)&&RememberActivity();},[]{return ContextActivity()&&EvidenceMatches(1,1)&&AllOutcomesHidden();},.2);
    Add(TEXT(""),TEXT("SavedStatueAttentionSettled"),Noop,SavedContextSettled,.25);
    Add(TEXT("L04"),TEXT("SavedContextRadioHidden"),[]{return WallForProp(Run.B.Get(),30)&&Inspect(Run.B.Get(),false);},[]{return EvidenceMatches(1,1)&&FixedActivity()&&AllOutcomesHidden()&&NavStopped()&&ContextMemory().SeenEvents.Num()==1;},.25);
    Add(TEXT(""),TEXT("SavedContextRestore"),[]{NavTest.SavedRestorePlayer=Run.Player->GetActorLocation();NavTest.SavedRestoreSightingTime=State().LastSuccessfulSightingTime;NavTest.SavedRestoreSubmissions=NavSubmissions();return WallOff();},[]{return SavedContextSettled()&&State().LastSuccessfulSightingTime>NavTest.SavedRestoreSightingTime&&Run.World->GetTimeSeconds()-State().LastSuccessfulSightingTime<=.35;},.25,[]{return Run.Player.IsValid()&&Run.Player->GetActorLocation().Equals(NavTest.SavedRestorePlayer,.01)&&NavStopped()&&NavSubmissions()==NavTest.SavedRestoreSubmissions&&NavTest.Travel<2&&EvidenceMatches(1,1)&&AllOutcomesHidden();});
    Add(TEXT(""),TEXT("SavedContextSelectWatchPoint"),[]{if(!SavedRoutePosition(false)||!ConfigureSavedContextPoint(false))return false;auto* P=FindContextPoint(TEXT("SavedSecond"));if(!IsValid(P))return false;P->bEnabled=true;return true;},[]{return ContextTravel()&&Run.Controller->GetPedroMoveWatchPoint()==FName(TEXT("SavedSecond"))&&Run.Controller->GetContextSelectedScore()>=Run.Controller->GetContextCurrentScore()+10&&EvidenceMatches(1,1);},.05);
    Add(TEXT(""),TEXT("SavedContextRelocation"),Noop,[]{return ContextWatching()&&Aligned()&&SavedAllVisible()&&FVector::Dist2D(NavRoot(),NavTest.SavedRelocationRoot)>150&&NavSubmissions()>NavTest.SavedRelocationSubmissions&&EvidenceMatches(1,1);},.3);
    Add(TEXT("L05"),TEXT("SavedContextRadioNew"),[]{return Inspect(Run.B.Get(),true);},[]{return ContextActivity()&&EvidenceMatches(2,2)&&AllOutcomesHidden();},.2);
    Add(TEXT(""),TEXT("SavedRadioAttentionSettled"),Noop,[]{return ContextWatching()&&Aligned()&&SavedAllVisible()&&EvidenceMatches(2,2);},.25);
    Add(TEXT("L06"),TEXT("SavedContextSatisfied"),[]{return EvidenceMatches(2,2)&&AllOutcomesHidden()&&Inspect(Run.C.Get(),true)&&EvidenceMatches(3,3)&&AllOutcomesHidden();},[]{return FinalPresentation()&&ContextRunning()&&ContextMode()==CMode::Satisfied&&NavStopped()&&ContextMemory().bSatisfied&&Run.Emitted.Num()==4;},.5);
}

FVector ContextPlayerStart=FVector::ZeroVector;
double ContextStartHealth=-1;
TWeakObjectPtr<APawn> ContextStepPlayer;
double ContextHealth()
{
    const auto* H=Run.Player.IsValid()?Run.Player->FindComponentByClass<UHealthComponent>():nullptr;
    return IsValid(H)?H->GetCurrentHealth():-1;
}
void CaptureContextPose()
{
    ContextStepPlayer=Run.Player;ContextPlayerStart=Run.Player.IsValid()?Run.Player->GetActorLocation():FVector::ZeroVector;ContextStartHealth=ContextHealth();
}
bool GuardContext()
{
    if(ContextStepPlayer!=Run.Player||!Run.Player.IsValid()||!Run.Player->GetActorLocation().Equals(ContextPlayerStart,.1)||ContextHealth()+.01<ContextStartHealth)return false;
    if(!Run.Controller.IsValid())return true;
    const auto& M=ContextMemory();const auto& D=Run.Controller->GetContextDecision();
    if(M.SeenEvents.Num()>EvidenceCount()&&Run.Director.IsValid())return false;
    if(M.Activity.EventId.IsValid()&&!M.SeenEvents.Contains(M.Activity.EventId))return false;
    if(D.AttentionIntent==EPedroContextAttentionIntent::Activity&&(!D.LookPoint.IsSet()||!D.SourceEventId.IsValid()))return false;
    if(NavActive()&&ContextPurpose()==CPurpose::None)return false;
    if(ContextRunning()&&D.Mode==CMode::Satisfied&&NavActive())return false;
    return true;
}
void LogContextCandidates()
{
    auto* C=Run.Controller.Get();if(!IsValid(C))return;
    if(Ctx.LoggedController.Get()!=C){Ctx.LoggedController=C;Ctx.LastSelection=0;}
    if(!C->GetContextSelectionSerial()||C->GetContextSelectionSerial()==Ctx.LastSelection)return;
    Ctx.LastSelection=C->GetContextSelectionSerial();
    const auto Log=[&](const FPedroContextCandidate& A,bool bCurrent)
    {
        UE_LOG(LogTemp,Display,TEXT("PEDRO_CONTEXT_CANDIDATE controller=%s serial=%u id=%s current=%s goal=%s v=%.6f f=%.6f i=%.6f p=%.6f c=%.6f r=%.6f score=%.6f"),
            *GetNameSafe(C),Ctx.LastSelection,*A.Id.ToString(),Yes(bCurrent),*VectorString(A.Goal),A.Visibility,A.DistanceFit,A.Interest,A.Priority,A.Cost,A.Penalty,PedroContextPolicy::Score(A));
    };
    Log(C->GetContextCurrentCandidate(),true);
    for(const auto& A:C->GetContextCandidates())Log(A,false);
    for(const auto& Rejection:C->GetContextRejections())
    {UE_LOG(LogTemp,Display,TEXT("PEDRO_CONTEXT_FIXTURE reject=%s controller=%s serial=%u"),*Rejection,*GetNameSafe(C),Ctx.LastSelection);}
    UE_LOG(LogTemp,Display,TEXT("PEDRO_CONTEXT_SELECTION controller=%s serial=%u attempts=%u selectedScore=%.6f currentScore=%.6f selected=%s"),*GetNameSafe(C),Ctx.LastSelection,C->GetContextPathQueries(),C->GetContextSelectedScore(),C->GetContextCurrentScore(),*C->GetContextSelectedPoint().ToString());
}
void LogContextSample(const TCHAR* Kind,const FStep& Step,double Dt=0)
{
    const auto* C=Run.Controller.Get();const auto& M=ContextMemory();const auto& A=State();
    const FPedroContextDecision Empty;const auto& D=IsValid(C)?C->GetContextDecision():Empty;
    const FPedroNavIntent* Intent=IsValid(C)?&C->GetNavigationIntent():nullptr;
    const auto* Move=Run.Observer.IsValid()?Run.Observer->GetCharacterMovement():nullptr;
    FVector Eye=FVector::ZeroVector;FRotator Facing=FRotator::ZeroRotator;if(Run.Observer.IsValid())Run.Observer->GetActorEyesViewPoint(Eye,Facing);
    const double Now=Run.World->GetTimeSeconds();const FVector Player=Run.Player.IsValid()?Run.Player->GetActorLocation():FVector::ZeroVector;
    UE_LOG(LogTemp,Display,TEXT("PEDRO_CONTEXT_TEST status=%s case=%s step=%s now=%.6f dt=%.6f root=%s start=%s velocity=%s yaw=%.6f eye=%s forward=%s travel=%.3f yawTravel=%.3f running=%s mode=%s reason=%d entered=%.6f movement=%d attentionIntent=%d attention=%d look=%s lookSet=%s event=%s activity=%s activityAt=%.6f expiry=%.6f lastEvent=%s lastActivity=%s lastActivityAt=%.6f seen=%d point=%s currentPoint=%s selectedScore=%.6f currentScore=%.6f attempts=%u selection=%u losses=%d lastLoss=%.6f holdStart=%.6f resume=%.6f closeSince=%.6f disengageUntil=%.6f generation=%u purpose=%s active=%s id=%u goal=%s valid=%s length=%.3f routePoint=%s submissions=%u planTime=%.6f sighting=%s sightTime=%.6f awareness=%s has=%s known=%s time=%.6f player=%s target=%s actual=%s controller=%s director=%s grounded=%s health=%.3f evidence=%d sampled=%d emitted=%s acquired=%s final=%s satisfied=%s progress=%d otherSeen=%d"),
        Kind,Step.Case.IsEmpty()?TEXT("-"):*Step.Case,*Step.Name,Now,Dt,*VectorString(NavRoot()),*VectorString(NavTest.Start),*VectorString(NavVelocity()),BodyYaw(),*VectorString(Eye),*VectorString(Facing.Vector()),NavTest.Travel,NavTest.YawTravel,Yes(ContextRunning()),ContextModeName(D.Mode),int32(D.Reason),M.EnteredAt,int32(D.MovementIntent),int32(D.AttentionIntent),C?int32(C->GetAttentionDecision().Mode):0,
        *VectorString(D.LookPoint.IsSet()?D.LookPoint.GetValue():FVector::ZeroVector),Yes(D.LookPoint.IsSet()),*M.Activity.EventId.ToString(EGuidFormats::Digits),*VectorString(M.Activity.ObservedLocation),M.Activity.ObservedAt,M.Activity.Expires,*M.LastActivity.EventId.ToString(EGuidFormats::Digits),*VectorString(M.LastActivity.ObservedLocation),M.LastActivity.ObservedAt,M.SeenEvents.Num(),*D.WatchPointId.ToString(),*M.CurrentPoint.ToString(),C?C->GetContextSelectedScore():0,C?C->GetContextCurrentScore():0,C?C->GetContextPathQueries():0,C?C->GetContextSelectionSerial():0,M.LossEpisodes,M.LastLoss,M.HoldStarted,M.ActivityResume,M.CloseSince,M.DisengageUntil,C?C->GetContextGeneration():0,
        PurposeName(ContextPurpose()),Yes(NavActive()),C&&C->GetPedroMoveId().IsValid()?C->GetPedroMoveId().GetID():0,*VectorString(C?C->GetPedroMoveGoal():FVector::ZeroVector),Yes(C&&C->HasValidPedroPath()),C?C->GetPedroPathLength():0,*((C?C->GetPedroMoveWatchPoint():NAME_None).ToString()),NavSubmissions(),Intent?Intent->LastPlan:-1,*VectorString(Intent?Intent->Sighting:FVector::ZeroVector),Intent?Intent->SightingTime:0,
        StatusName(A.Status),Yes(A.bHasLastKnownPlayerLocation),*VectorString(A.LastKnownPlayerLocation),A.LastSuccessfulSightingTime,*GetNameSafe(Run.Player.Get()),*GetNameSafe(A.ObservedPlayer.Get()),*VectorString(Player),*GetNameSafe(C),*GetNameSafe(Run.Director.Get()),Yes(Move&&Move->IsMovingOnGround()),ContextHealth(),EvidenceCount(),SampledCount(),*Ids(Run.Emitted),*EvidenceIds(),Yes(Finalized()),Yes(M.bSatisfied),Progress()?Progress()->GetResolvedEncounterCount():-1,Run.SecondController.IsValid()?Run.SecondController->GetContextMemory().SeenEvents.Num():-1);
    LogContextCandidates();
}
