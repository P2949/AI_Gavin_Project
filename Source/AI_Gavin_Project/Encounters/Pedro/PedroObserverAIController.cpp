#include "Encounters/Pedro/PedroObserverAIController.h"

#include "CollisionQueryParams.h"
#include "Encounters/Pedro/PedroDirectObservation.h"
#include "Encounters/Pedro/PedroObserverCharacter.h"
#include "Encounters/Pedro/PedroEncounterDirector.h"
#include "Encounters/Pedro/PedroObserverWatchPoint.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/NavMeshPath.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"

APedroObserverAIController::APedroObserverAIController()
{
    bAttachToPawn = true;
    bStartAILogicOnPossess = false;
    ObserverPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PedroPerception"));
    SetPerceptionComponent(*ObserverPerception);
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("PedroSight"));
    SightConfig->SightRadius = 1500.0f;
    SightConfig->LoseSightRadius = 1800.0f;
    SightConfig->PeripheralVisionAngleDegrees = 75.0f;
    SightConfig->AutoSuccessRangeFromLastSeenLocation = FAISystem::InvalidRange;
    SightConfig->PointOfViewBackwardOffset = 0.0f;
    SightConfig->NearClippingRadius = 0.0f;
    SightConfig->SetMaxAge(5.0f);
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
    ObserverPerception->ConfigureSense(*SightConfig);
    ObserverPerception->SetDominantSense(SightConfig->GetSenseImplementation());
}

void APedroObserverAIController::GetActorEyesViewPoint(FVector& Location, FRotator& Rotation) const
{
    if (const auto* Observer = Cast<APedroObserverCharacter>(GetPawn()))
        Observer->GetActorEyesViewPoint(Location, Rotation);
    else
        Super::GetActorEyesViewPoint(Location, Rotation);
}

void APedroObserverAIController::GetPlayerViewPoint(FVector& Location, FRotator& Rotation) const
{
    GetActorEyesViewPoint(Location, Rotation);
}

void APedroObserverAIController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
    // Never call the base AI focus/facing logic. Only this optional policy
    // owns physical yaw; no actor target or live player transform is used.
    if (!IsAttentionRunning()) { Attention = {}; return; }
    auto* Observer = Cast<APedroObserverCharacter>(GetPawn());
    UWorld* World = GetWorld();
    const APlayerController* PC = World->GetFirstPlayerController();
    const APawn* Player = PC ? PC->GetPawn() : nullptr;
    if (!IsSupportedPlayer(Player) || Player->GetWorld() != World) Player = nullptr;
    Attention = PedroAttentionPolicy::Select(true,true,Awareness,Player,
        World->GetTimeSeconds(),IdleYaw.GetValue(),AttentionSettings);
    if(bContextEnabled)
        Attention=PedroAttentionPolicy::WithContext(Attention, Context.AttentionIntent==EPedroContextAttentionIntent::Idle,
            Context.AttentionIntent==EPedroContextAttentionIntent::Activity?Context.LookPoint:TOptional<FVector>{},IdleYaw.GetValue());
    if (!bUpdatePawn || Attention.Mode == EPedroAttentionMode::Disabled) return;
    FRotator Body = Observer->GetActorRotation();
    if (Body.ContainsNaN()) return;
    TOptional<double> Error;
    if (Attention.LookPoint.IsSet())
    {
        FVector Eye; FRotator Facing;
        Observer->GetActorEyesViewPoint(Eye,Facing);
        Error = PedroAttentionPolicy::LookError(Observer->GetActorLocation(),Eye,Facing.Vector(),
            Attention.LookPoint.GetValue(),AttentionSettings.MinimumHorizontalDistance);
    }
    else Error = PedroAttentionPolicy::ShortestError(Body.Yaw,Attention.IdleYaw);
    if (!Error.IsSet()) return;
    Attention.AngularError = Error.GetValue();
    Attention.RequestedYawStep = PedroAttentionPolicy::TurnStep(Attention.AngularError,DeltaTime,
        Attention.Mode == EPedroAttentionMode::Idle ? AttentionSettings.IdleDegreesPerSecond : AttentionSettings.TrackDegreesPerSecond,
        AttentionSettings);
    if (Attention.RequestedYawStep == 0) return;
    Body.Yaw = FRotator::NormalizeAxis(Body.Yaw + Attention.RequestedYawStep);
    Observer->SetActorRotation(Body,ETeleportType::None);
}

bool APedroObserverAIController::IsAttentionRunning() const
{
    return bAttentionEnabled && bRunning && GetWorld() && IdleYaw.IsSet() &&
        IsValid(Cast<APedroObserverCharacter>(GetPawn()));
}

void APedroObserverAIController::ResetAttention()
{
    IdleYaw.Reset();
    Attention = {};
}

void APedroObserverAIController::CaptureAttentionAnchor()
{
    ResetAttention();
    const auto* Observer = Cast<APedroObserverCharacter>(GetPawn());
    if (!bAttentionEnabled || !bRunning || !IsValid(Observer)) return;
    const double Yaw = Observer->GetActorRotation().Yaw;
    if (!FMath::IsFinite(Yaw)) return;
    IdleYaw = FRotator::NormalizeAxis(Yaw);
    Attention.Mode = EPedroAttentionMode::Idle;
    Attention.IdleYaw = IdleYaw.GetValue();
}

void APedroObserverAIController::SetAttentionEnabled(bool bEnabled)
{
    if (bAttentionEnabled == bEnabled) return;
    bAttentionEnabled = bEnabled;
    if(!bEnabled&&bContextEnabled){CancelPedroMove();ContextPath.Reset();}
    CaptureAttentionAnchor();
}

bool APedroObserverAIController::IsSupportedPlayer(const AActor* Candidate)
{
    const auto* Player = Cast<APawn>(Candidate);
    return IsValid(Player) && Player->IsPlayerControlled();
}

bool APedroObserverAIController::IsAwarenessRunning() const
{
    return bRunning && GetWorld() && GetWorld()->GetTimerManager().TimerExists(RefreshTimer) &&
        !GetWorld()->GetTimerManager().IsTimerPaused(RefreshTimer);
}

void APedroObserverAIController::OnPossess(APawn* InPawn)
{
    StopAwareness();
    Super::OnPossess(InPawn);
    if (!IsValid(Cast<APedroObserverCharacter>(GetPawn())) || !GetWorld()) return;
    ObserverPerception->ForgetAll();
    ObserverPerception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &APedroObserverAIController::HandleSightUpdate);
    bRunning = true;
    CaptureAttentionAnchor();
    ConfigureNavigation();
    ObserverPerception->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
    ObserverPerception->RequestStimuliListenerUpdate();
    GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this,
        &APedroObserverAIController::RefreshAwareness, 0.1f, true);
}

void APedroObserverAIController::StopAwareness()
{
    DisableContextualObservation();
    ResetNavigation();
    bRunning = false;
    ResetAttention();
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(RefreshTimer);
    Awareness.Reset();
    if (IsValid(ObserverPerception))
    {
        ObserverPerception->OnTargetPerceptionUpdated.RemoveDynamic(this, &APedroObserverAIController::HandleSightUpdate);
        ObserverPerception->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
        ObserverPerception->ForgetAll();
    }
}

void APedroObserverAIController::OnUnPossess()
{
    StopAwareness();
    Super::OnUnPossess();
}

void APedroObserverAIController::EndPlay(const EEndPlayReason::Type Reason)
{
    StopAwareness();
    Super::EndPlay(Reason);
}

bool APedroObserverAIController::HasNativeSight(APawn* Target) const
{
    FVector Location; float Age;
    return GetNativeSightSample(Target,Location,Age);
}

bool APedroObserverAIController::GetNativeSightSample(APawn* Target, FVector& OutLocation, float& OutAge) const
{
    if (!IsValid(Target) || !IsValid(ObserverPerception) ||
        !ObserverPerception->IsSenseEnabled(UAISense_Sight::StaticClass())) return false;
    FActorPerceptionBlueprintInfo Info;
    if (!ObserverPerception->GetActorsPerception(Target, Info)) return false;
    for (const FAIStimulus& Stimulus : Info.LastSensedStimuli)
    {
        if (Stimulus.Type == SightConfig->GetSenseID() && Stimulus.WasSuccessfullySensed() &&
            !Stimulus.IsExpired() && !Stimulus.StimulusLocation.ContainsNaN() &&
            FAISystem::IsValidLocation(Stimulus.StimulusLocation) &&
            FMath::IsFinite(Stimulus.GetAge()) && Stimulus.GetAge() <= SightConfig->GetMaxAge())
        {
            OutLocation = Stimulus.StimulusLocation;
            OutAge = Stimulus.GetAge();
            return true;
        }
    }
    return false;
}

bool APedroObserverAIController::TryCurrentVisibleLocation(APawn* Target, FVector& OutLocation) const
{
    // This is a bounded sensory query, not a movement goal or a memory read.
    // A cached success may be older than the target's current transform.
    // Read the candidate endpoint ONLY inside this query and publish it only
    // if the fresh physical ray AND current view geometry both succeed.
    const auto* Observer = Cast<APedroObserverCharacter>(GetPawn());
    UWorld* World = GetWorld();
    if (!IsValid(Observer) || !IsValid(Target) || !World || Target->GetWorld() != World) return false;
    FVector Eye;
    FRotator Facing;
    Observer->GetActorEyesViewPoint(Eye, Facing);
    const FVector Candidate = Target->GetActorLocation();
    const float Range = Awareness.Status == EPedroAwarenessStatus::Perceived
        ? SightConfig->LoseSightRadius : SightConfig->SightRadius;
    if (!FAISystem::IsValidLocation(Candidate) ||
        PedroDirectObservation::CheckGeometry(Eye, Facing.Vector(), Candidate, Range,
            SightConfig->PeripheralVisionAngleDegrees) != EPedroDirectObservationStatus::Visible) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PedroAwarenessRefresh), false);
    Params.AddIgnoredActor(Observer);
    Params.AddIgnoredActor(this);
    FHitResult Hit;
    if (World->LineTraceSingleByChannel(Hit, Eye, Candidate, ECC_Visibility, Params) &&
        Hit.GetActor() != Target) return false;
    OutLocation = Candidate;
    return true;
}

void APedroObserverAIController::HandleSightUpdate(AActor* Actor, FAIStimulus Stimulus)
{
    if (bRunning && IsSupportedPlayer(Actor) && Stimulus.Type == SightConfig->GetSenseID())
        RefreshAwareness();
}

void APedroObserverAIController::RefreshAwareness()
{
    UWorld* World = GetWorld();
    if (!bRunning) return;
    if (!World || !IsValid(Cast<APedroObserverCharacter>(GetPawn()))) { StopAwareness(); return; }
    // Single-player encounter. Identity only; no transform read here.
    const APlayerController* PC = World->GetFirstPlayerController();
    APawn* Player = PC ? PC->GetPawn() : nullptr;
    if (!IsSupportedPlayer(Player) || Player->GetWorld() != World)
    {
        Awareness.Reset();
        return;
    }
    if (Awareness.bHasLastKnownPlayerLocation && Awareness.ObservedPlayer.Get() != Player)
        Awareness.Reset();
    FVector VisibleLocation;
    if (HasNativeSight(Player) && TryCurrentVisibleLocation(Player, VisibleLocation))
    {
        if (!Awareness.AcceptSighting(Player, VisibleLocation, World->GetTimeSeconds()))
            Awareness.LoseSight();
    }
    else Awareness.LoseSight();
}

// All destinations originate in accepted awareness. No movement-to-actor or
// actor focus. Use a checked path directly rather than requesting a second,
// potentially different, path after validation.
void APedroObserverAIController::Tick(float DeltaTime)
{
    if(bContextEnabled)UpdateContext();
    Super::Tick(DeltaTime);
    if (bNavigationEnabled) UpdateNavigation();
}
bool APedroObserverAIController::IsNavigationRunning() const
{
    return bNavigationEnabled && bRunning && GetWorld() && bMovementConfigured && ConfiguredMovement.IsValid() &&
        IsValid(Cast<APedroObserverCharacter>(GetPawn()));
}
void APedroObserverAIController::SetNavigationEnabled(bool bEnabled)
{
    if(bNavigationEnabled==bEnabled)return;
    bNavigationEnabled=bEnabled;
    if(bEnabled)ConfigureNavigation();else ResetNavigation();
}
void APedroObserverAIController::ConfigureNavigation()
{
    if(!bNavigationEnabled||!bRunning||bMovementConfigured)return;
    auto* O=Cast<APedroObserverCharacter>(GetPawn());
    if(!IsValid(O)||!NavigationSettings.IsValid())return;
    auto* M=O->GetCharacterMovement();
    const auto R=O->GetActorRotation();
    if(!IsValid(M)||!M->IsComponentTickEnabled()||R.ContainsNaN()||FMath::Abs(R.Pitch)>0.5||FMath::Abs(R.Roll)>0.5||
        O->bUseControllerRotationYaw||M->bOrientRotationToMovement||M->bUseControllerDesiredRotation||!M->GetNavMovementProperties())return;
    ConfiguredMovement=M;
    SavedSpeed=M->MaxWalkSpeed;SavedAccel=M->MaxAcceleration;SavedBrake=M->BrakingDecelerationWalking;
    bSavedAccelerationPaths=M->GetNavMovementProperties()->bUseAccelerationForPaths;
    M->MaxWalkSpeed=NavigationSettings.Speed;M->MaxAcceleration=NavigationSettings.Acceleration;M->BrakingDecelerationWalking=NavigationSettings.Braking;
    M->GetNavMovementProperties()->bUseAccelerationForPaths=true;
    bMovementConfigured=true;
    Navigation={EPedroNavigationMode::Idle,EPedroNavigationReason::NoKnowledge,false};
}
void APedroObserverAIController::CancelPedroMove()
{
    const FAIRequestID Old=OwnedMove;
    MovePurpose=EPedroMovePurpose::None;MovePoint=NAME_None;
    OwnedMove=FAIRequestID::InvalidRequest;OwnedPath.Reset();MoveGoal=FVector::ZeroVector;NavIntent.bActive=false;RoutePlayer.Reset();
    auto* PF=GetPathFollowingComponent();
    // Clear ownership BEFORE AbortMove: synchronous completion is possible.
    if(IsValid(PF)&&PedroNavigationPolicy::OwnsRequest(Old,PF->GetCurrentRequestId()))
    {
        PF->AbortMove(*this,FPathFollowingResultFlags::ForcedScript,Old,EPathFollowingVelocityMode::Reset);
        if(ConfiguredMovement.IsValid()&&PF->GetStatus()==EPathFollowingStatus::Idle)ConfiguredMovement->StopMovementImmediately();
    }
}
void APedroObserverAIController::ResetNavigation()
{
    CancelPedroMove();
    if(ConfiguredMovement.IsValid()&&bMovementConfigured)
    {
        auto* M=ConfiguredMovement.Get();M->MaxWalkSpeed=SavedSpeed;M->MaxAcceleration=SavedAccel;M->BrakingDecelerationWalking=SavedBrake;
        if(auto* Props=M->GetNavMovementProperties())Props->bUseAccelerationForPaths=bSavedAccelerationPaths;
    }
    ConfiguredMovement.Reset();bMovementConfigured=false;Navigation={};NavIntent={};NavigationPlayer.Reset();MoveSubmissions=0;
}
bool APedroObserverAIController::HasValidPedroPath() const
{
    return OwnedPath.IsValid()&&OwnedPath->IsValid()&&OwnedPath->IsUpToDate()&&!OwnedPath->IsPartial()&&
        !OwnedPath->GetGoalActor();
}
double APedroObserverAIController::GetPedroPathLength() const {return OwnedPath.IsValid()?OwnedPath->GetLength():0;}
void APedroObserverAIController::OnMoveCompleted(FAIRequestID ID,const FPathFollowingResult& Result)
{
    if(PedroNavigationPolicy::OwnsRequest(OwnedMove,ID))
    {
        OwnedMove=FAIRequestID::InvalidRequest;OwnedPath.Reset();MoveGoal=FVector::ZeroVector;NavIntent.bActive=false;RoutePlayer.Reset();
        const bool bSuccess=Result.IsSuccess();
        const auto Purpose=MovePurpose;const FName Point=MovePoint;
        MovePurpose=EPedroMovePurpose::None;MovePoint=NAME_None;
        if(bContextEnabled&&GetWorld())ContextMoveEnded(Purpose,Point,bSuccess,GetWorld()->GetTimeSeconds());
        Navigation={bSuccess?EPedroNavigationMode::Idle:EPedroNavigationMode::Blocked,
            bSuccess?EPedroNavigationReason::Arrived:EPedroNavigationReason::PathInvalid,false};
        if(!bSuccess&&GetWorld())NavIntent.RetryAfter=GetWorld()->GetTimeSeconds()+NavigationSettings.RetryInterval;
    }
    Super::OnMoveCompleted(ID,Result);
}
void APedroObserverAIController::UpdateNavigation()
{
    using Mode=EPedroNavigationMode;using Reason=EPedroNavigationReason;
    if(!IsNavigationRunning()) {CancelPedroMove();return;}
    auto* O=Cast<APedroObserverCharacter>(GetPawn());auto* M=ConfiguredMovement.Get();auto* W=GetWorld();auto* PF=GetPathFollowingComponent();
    const double Now=W->GetTimeSeconds();const auto& S=NavigationSettings;
    const APlayerController* PC=W->GetFirstPlayerController();APawn* Player=PC?PC->GetPawn():nullptr;
    if(!IsSupportedPlayer(Player)||Player->GetWorld()!=W)Player=nullptr;
    if(NavigationPlayer.Get()!=Player||(!Player&&NavigationPlayer.IsStale()))
    {
        CancelPedroMove();NavIntent={};NavigationPlayer=Player;
    }
    if(NavIntent.bActive&&RoutePlayer.Get()!=Player)CancelPedroMove();
    if(!M->IsComponentTickEnabled()||!M->IsMovingOnGround()||!IsValid(PF)||O->bUseControllerRotationYaw||M->bOrientRotationToMovement||M->bUseControllerDesiredRotation)
    {CancelPedroMove();Navigation={Mode::Blocked,Reason::MovementUnavailable,false};return;}
    if(NavIntent.bActive&&!PedroNavigationPolicy::OwnsRequest(OwnedMove,PF->GetCurrentRequestId()))CancelPedroMove();
    if(!NavIntent.bActive&&PF->GetStatus()!=EPathFollowingStatus::Idle){Navigation={Mode::Blocked,Reason::ForeignRequest,false};return;}
    auto Fail=[&](Reason R){const auto Purpose=MovePurpose;const FName Point=MovePoint;CancelPedroMove();NavIntent.RetryAfter=Now+S.RetryInterval;Navigation={Mode::Blocked,R,false};
        if(bContextEnabled)ContextMoveEnded(Purpose,Point,false,Now);};
    if(NavIntent.bActive)
    {
        if(!HasValidPedroPath()){Fail(Reason::PathInvalid);return;}
        if(Now-MoveStarted>=S.MaxDuration){Fail(Reason::Timeout);return;}
        if(Now-StallStarted>=S.StallWindow)
        {
            if(PedroNavigationPolicy::Stalled(Now-StallStarted,FVector::Dist2D(O->GetActorLocation(),StallOrigin),S)){Fail(Reason::Stalled);return;}
            StallStarted=Now;StallOrigin=O->GetActorLocation();
        }
    }
    if(bContextEnabled&&UpdateContextNavigation(Now))return;
    Navigation=PedroNavigationPolicy::Select(true,true,Awareness,Player,O->GetActorLocation(),Now,NavIntent,S);
    if(Navigation.Mode!=Mode::FollowObserved&&Navigation.Mode!=Mode::ContinueLastKnown){CancelPedroMove();return;}
    if(!Navigation.bPlan)return;
    NavIntent.LastPlan=Now;
    const FVector Known=Awareness.LastKnownPlayerLocation;
    auto* Sys=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
    const auto& Agent=M->GetNavAgentPropertiesRef();
    const FVector Feet=M->GetFeetLocation();
    const auto* Data=Sys?Sys->GetNavDataForProps(Agent,Feet,FVector::ZeroVector):nullptr;
    if(!IsValid(Cast<ARecastNavMesh>(Data))){Fail(Reason::NoNavData);return;}
    FNavLocation Start;
    if(!Sys->ProjectPointToNavigation(Feet,Start,FVector(50,50,100),Data)||FVector::Dist2D(Feet,Start.Location)>50||FMath::Abs(Feet.Z-Start.Location.Z)>100){Fail(Reason::NoNavData);return;}
    const auto Candidates=PedroNavigationPolicy::Candidates(O->GetActorLocation(),Known,S);
    int32 Best=INDEX_NONE;double Cost=0;FVector Goal=FVector::ZeroVector;FNavPathSharedPtr Path;
    for(int32 I=0;I<Candidates.Num();++I)
    {
        FNavLocation Projected;
        if(!Sys->ProjectPointToNavigation(Candidates[I],Projected,FVector(S.ProjectionXY,S.ProjectionXY,S.ProjectionZ),Data)||
           !PedroNavigationPolicy::AcceptProjection(Candidates[I],Projected.Location,Known,S))continue;
        FPathFindingQuery Q(this,*Data,Feet,Projected.Location);Q.SetAllowPartialPaths(false);Q.SetNavAgentProperties(Agent);
        const FPathFindingResult R=Sys->FindPathSync(Agent,Q);
        if(!R.IsSuccessful()||!R.Path.IsValid()||!R.Path->IsValid())continue;
        bool bSpecial=R.Path->ContainsAnyCustomLink();
        bool bUnsafe=false;
        const auto& Points=R.Path->GetPathPoints();
        for(int32 J=0;J<Points.Num();++J)
        {
            const auto& Pt=Points[J];
            bSpecial|=FNavMeshNodeFlags(Pt.Flags).IsNavLink()||Pt.CustomNavLinkId.IsValid();
            if(Pt.Location.ContainsNaN()||!FAISystem::IsValidLocation(Pt.Location)){bUnsafe=true;break;}
            if(J>0)
            {
                const auto& Prev=Points[J-1].Location;
                if(!PedroNavigationPolicy::IsSafeSegment(Prev,Pt.Location,Known,S.PersonalSpace,false)){bUnsafe=true;break;}
            }
        }
        if(bUnsafe)continue;
        if(!PedroNavigationPolicy::AcceptPath(!R.Path->IsPartial(),bSpecial,R.Path->GetLength(),S))continue;
        const double C=R.Path->GetCost();
        if(PedroNavigationPolicy::BetterCandidate(I,C,Best,Cost)){Best=I;Cost=C;Goal=Projected.Location;Path=R.Path;}
        if(Best==0)break;
    }
    if(!Path.IsValid()){Fail(Reason::NoRoute);return;}
    if(!SubmitPedroPath(Goal,Path,Known,Now,EPedroMovePurpose::Follow))Fail(Reason::NoRoute);
}

bool APedroObserverAIController::SubmitPedroPath(const FVector& Goal,const FNavPathSharedPtr& Path,const FVector& Known,double Now,EPedroMovePurpose Purpose,FName Point)
{
    auto* PF=GetPathFollowingComponent();const auto& S=NavigationSettings;APawn* O=GetPawn();
    if(!IsValid(PF)||!IsValid(O)||!Path.IsValid()||!Path->IsValid()||!Path->IsUpToDate()||Path->IsPartial()||Path->GetGoalActor())return false;
    if(PF->GetStatus()!=EPathFollowingStatus::Idle&&!PedroNavigationPolicy::OwnsRequest(OwnedMove,PF->GetCurrentRequestId()))return false;
    CancelPedroMove();
    Path->EnableRecalculationOnInvalidation(false);
    FAIMoveRequest Request(Goal);Request.SetUsePathfinding(true);Request.SetAllowPartialPath(false);Request.SetProjectGoalLocation(false);
    Request.SetAcceptanceRadius(S.AcceptanceRadius);Request.SetReachTestIncludesAgentRadius(false);Request.SetReachTestIncludesGoalRadius(false);Request.SetCanStrafe(true);
    if(PF->HasReached(Request)){Navigation={EPedroNavigationMode::Idle,EPedroNavigationReason::Arrived,false};if(bContextEnabled)ContextMoveEnded(Purpose,Point,true,Now);return true;}
    const FAIRequestID ID=PF->RequestMove(Request,Path);
    if(!ID.IsValid()||!PedroNavigationPolicy::OwnsRequest(ID,PF->GetCurrentRequestId()))return false;
    OwnedMove=ID;OwnedPath=Path;MoveGoal=Goal;RoutePlayer=Awareness.ObservedPlayer;NavIntent.bActive=true;
    NavIntent.Sighting=Known;NavIntent.SightingTime=Awareness.LastSuccessfulSightingTime;
    MovePurpose=Purpose;MovePoint=Point;MoveStarted=StallStarted=Now;StallOrigin=O->GetActorLocation();++MoveSubmissions;
    return true;
}

// Context owns intent only. Native perception, the path owner and physical yaw remain above.
bool APedroObserverAIController::IsContextualObservationRunning() const
{
    const auto* D=ContextDirector.Get();
    return bContextEnabled&&PedroContextPolicy::CanBind(IsValid(GetPawn()),IsValid(D),IsValid(D)&&D->GetWorld()==GetWorld()&&D->DirectObservationActor.Get()==GetPawn(),
        IsValid(D)&&D->bUseDirectObservationAcquisition&&!D->bUsePrototypeAssumedAcquisition,IsAttentionRunning(),IsNavigationRunning())&&ContextSettings.IsValid();
}
bool APedroObserverAIController::EnableContextualObservation(APedroEncounterDirector* D,const TArray<APedroObserverWatchPoint*>& Points)
{
    if(!PedroContextPolicy::CanBind(IsValid(Cast<APedroObserverCharacter>(GetPawn())),IsValid(D),IsValid(D)&&D->GetWorld()==GetWorld()&&D->DirectObservationActor.Get()==GetPawn(),
        IsValid(D)&&D->bUseDirectObservationAcquisition&&!D->bUsePrototypeAssumedAcquisition,IsAttentionRunning(),IsNavigationRunning())||!ContextSettings.IsValid()||Points.Num()>32)return false;
    TSet<FName> Ids;TArray<TWeakObjectPtr<APedroObserverWatchPoint>> Valid;
    for(auto* P:Points){if(!IsValid(P)||P->GetWorld()!=GetWorld()||!P->IsUsable()||Ids.Contains(P->WatchPointId))return false;Ids.Add(P->WatchPointId);Valid.Add(P);}
    if(bContextEnabled&&ContextDirector.Get()==D&&ContextPoints==Valid&&IsContextualObservationRunning())return true;
    DisableContextualObservation();CancelPedroMove();
    bContextEnabled=true;ContextDirector=D;ContextPoints=MoveTemp(Valid);
    const APlayerController* PC=GetWorld()->GetFirstPlayerController();ContextPlayer=PC?PC->GetPawn():nullptr;
    ContextMemory.bSatisfied=D->HasCommittedContextResolution();++ContextGeneration;
    return true;
}
void APedroObserverAIController::ResetContextMemory()
{
    ContextMemory={};Context={};ContextPath.Reset();ContextGoal=ContextPlanSighting=FVector::ZeroVector;NextContextEvaluation=NextPointEvaluation=0;
    ContextSelectedScore=ContextCurrentScore=0;ContextSelectedPoint=NAME_None;ContextPathQueries=0;ContextCandidates.Reset();ContextRejections.Reset();ContextCurrentCandidate={};
}
void APedroObserverAIController::DisableContextualObservation()
{
    if(bContextEnabled)CancelPedroMove();
    bContextEnabled=false;ContextDirector.Reset();ContextPoints.Reset();ContextPlayer.Reset();ResetContextMemory();
}
void APedroObserverAIController::ReceiveContextEvidence(const APedroEncounterDirector* D,const FPedroEvidenceRecord& R)
{
    if(!IsContextualObservationRunning()||ContextDirector.Get()!=D||ContextMemory.bSatisfied||!ContextPlayer.IsValid()||Awareness.ObservedPlayer.Get()!=ContextPlayer.Get()||!Awareness.bHasLastKnownPlayerLocation)return;
    const auto* PC=GetWorld()->GetFirstPlayerController();
    if(!PC||PC->GetPawn()!=ContextPlayer.Get())return;
    const double Now=GetWorld()->GetTimeSeconds();
    if(Now-Awareness.LastSuccessfulSightingTime>=NavigationSettings.HoldAge)return;
    if(PedroContextPolicy::AcceptActivity(ContextMemory,R,Awareness.LastKnownPlayerLocation,Now,ContextSettings))NextContextEvaluation=0;
}
void APedroObserverAIController::ReceiveContextResolution(const APedroEncounterDirector* D)
{
    if(!bContextEnabled||ContextDirector.Get()!=D||!IsValid(D)||!D->HasCommittedContextResolution())return;
    ContextMemory.bSatisfied=true;ContextMemory.Activity={};ContextPath.Reset();CancelPedroMove();NextContextEvaluation=0;ContextMemory.EnteredAt=GetWorld()?GetWorld()->GetTimeSeconds():0;
    Context={};Context.Mode=EPedroContextMode::Satisfied;Context.Reason=EPedroContextReason::Resolved;Context.AttentionIntent=EPedroContextAttentionIntent::Idle;
}
void APedroObserverAIController::ContextMoveEnded(EPedroMovePurpose Purpose,FName Point,bool bSuccess,double Now)
{
    if(Purpose==EPedroMovePurpose::WatchPoint)
    {
        if(bSuccess){ContextMemory.CurrentPoint=Point;ContextMemory.LastPointVisit=Now;}
        else {ContextMemory.FailedPoint=Point;ContextMemory.LastPointFailure=Now;}
        if(!Point.IsNone()&&(ContextMemory.Points.Num()<32||ContextMemory.Points.Contains(Point)))
        {auto& P=ContextMemory.Points.FindOrAdd(Point);if(bSuccess)P.LastVisit=Now;else P.LastFailure=Now;}
        Context={};Context.Mode=bSuccess?EPedroContextMode::WatchFromPosition:EPedroContextMode::ObserveIdle;Context.WatchPointId=bSuccess?Point:NAME_None;
        ContextMemory.EnteredAt=Now;NextPointEvaluation=Now+ContextSettings.PointCooldown;
    }
    if(Purpose==EPedroMovePurpose::Withdraw){ContextMemory.DisengageUntil=Now+ContextSettings.DisengageDuration;ContextMemory.CloseSince=-1;ContextMemory.bAttackPending=false;Context={};Context.Mode=EPedroContextMode::Disengaged;Context.Reason=EPedroContextReason::Cooldown;ContextMemory.EnteredAt=Now;}
    NextContextEvaluation=0;
}
void APedroObserverAIController::UpdateContext()
{
    using CMode=EPedroContextMode;using CMove=EPedroContextMovementIntent;
    if(!IsContextualObservationRunning())
    {
        CancelPedroMove();ResetContextMemory();ContextDirector.Reset();ContextPoints.Reset();ContextPlayer.Reset();Context.Mode=CMode::ObserveIdle;Context.Reason=EPedroContextReason::Invalid;Context.AttentionIntent=EPedroContextAttentionIntent::Idle;return;
    }
    auto* W=GetWorld();const auto* PC=W->GetFirstPlayerController();APawn* Player=PC?PC->GetPawn():nullptr;
    if(!IsSupportedPlayer(Player)||Player->GetWorld()!=W)Player=nullptr;
    const double Now=W->GetTimeSeconds();const auto& S=ContextSettings;const auto& N=NavigationSettings;
    if(ContextPlayer.Get()!=Player||(!Player&&ContextPlayer.IsStale())){CancelPedroMove();ResetContextMemory();ContextPlayer=Player;ContextMemory.bSatisfied=ContextDirector->HasCommittedContextResolution();++ContextGeneration;}
    FPedroContextInput I;I.bEnabled=true;I.bValid=true;I.Now=Now;I.StartDistance=N.StartDistance;I.Known=Awareness.LastKnownPlayerLocation;
    const double Age=Now-Awareness.LastSuccessfulSightingTime;
    I.bRecent=IsValid(Player)&&Awareness.ObservedPlayer.Get()==Player&&Awareness.bHasLastKnownPlayerLocation&&Awareness.Status!=EPedroAwarenessStatus::Unaware&&PedroContextPolicy::ValidPoint(I.Known)&&Age>=0&&Age<N.HoldAge;
    I.bFresh=I.bRecent&&Age<=N.FreshAge&&Awareness.Status==EPedroAwarenessStatus::Perceived;
    I.Observer=GetPawn()->GetActorLocation();I.Distance=FVector::Dist2D(I.Observer,I.Known);
    I.bFollowActive=NavIntent.bActive&&MovePurpose==EPedroMovePurpose::Follow;
    I.bWatchActive=NavIntent.bActive&&MovePurpose==EPedroMovePurpose::WatchPoint;
    I.bWithdrawActive=NavIntent.bActive&&MovePurpose==EPedroMovePurpose::Withdraw;
    // A moving player invalidates a contextual fixed route. Never retarget a lost player.
    bool bInvalidPoint=false;
    for(const auto& P:ContextPoints)if(P.IsValid()&&P->WatchPointId==ContextMemory.CurrentPoint&&P->IsUsable()&&FVector::Dist2D(GetPawn()->GetActorLocation(),P->GetActorLocation())<=N.ProjectionXY+N.AcceptanceRadius+10){I.bAtPoint=true;break;}
    if(I.bWatchActive){bInvalidPoint=true;for(const auto& P:ContextPoints)if(P.IsValid()&&P->WatchPointId==MovePoint&&P->IsUsable()&&FVector::Dist2D(P->GetActorLocation(),MoveGoal)<=N.ProjectionXY+1&&FMath::Abs(P->GetActorLocation().Z-MoveGoal.Z)<=N.ProjectionZ+1){bInvalidPoint=false;break;}}
    bool bUnsafeUpdatedPath=false;
    if((I.bWatchActive||I.bWithdrawActive)&&I.bFresh&&OwnedPath.IsValid()&&FVector::Dist2D(I.Known,NavIntent.Sighting)>1)
    {
        const auto& Points=OwnedPath->GetPathPoints();for(int32 J=1;J<Points.Num();++J)
        {if(!PedroNavigationPolicy::IsSafeSegment(Points[J-1].Location,Points[J].Location,I.Known,N.PersonalSpace,I.bWithdrawActive)){bUnsafeUpdatedPath=true;break;}}
    }
    if((I.bWatchActive||I.bWithdrawActive)&&(!I.bFresh||bInvalidPoint||bUnsafeUpdatedPath||FVector::Dist2D(I.Known,NavIntent.Sighting)>=N.ReplanDisplacement||(I.bWatchActive&&I.Distance<N.PersonalSpace)))
    {
        const auto Purpose=MovePurpose;const FName Point=MovePoint;CancelPedroMove();ContextMoveEnded(Purpose,Point,false,Now);I.bWatchActive=I.bWithdrawActive=false;
    }
    if(!I.bAtPoint&&!I.bWatchActive)ContextMemory.CurrentPoint=NAME_None;
    const bool bUrgent=I.bFresh!=ContextMemory.bWasFresh||!I.bRecent||I.Distance<S.CloseDistance||ContextMemory.bSatisfied||ContextMemory.bAttackPending||(ContextMemory.Activity.EventId.IsValid()&&(Now>=ContextMemory.Activity.Expires||(I.bFresh&&FVector::Dist2D(I.Known,ContextMemory.Activity.PlayerAtObservation)>=S.DepartureDistance)));
    PedroContextPolicy::UpdateSenses(ContextMemory,I,S);
    const auto IsWatchMode=[](CMode Mode){return Mode==CMode::FollowPlayer||Mode==CMode::ObserveIdle||Mode==CMode::WatchFromPosition;};
    const bool bPointDue=PedroContextPolicy::WatchEvaluationDue(I,ContextMemory,S,NextPointEvaluation,
        NavIntent.RetryAfter,NavIntent.LastPlan,N.ReplanInterval,N.PersonalSpace);
    // UpdateContext runs before UpdateNavigation. Do not let the ordinary follow retry
    // win every opening simply because its per-frame update is faster than the policy timer.
    if(!bUrgent&&!(bPointDue&&IsWatchMode(Context.Mode))&&Now<NextContextEvaluation)return;
    NextContextEvaluation=Now+S.Interval;
    auto D=PedroContextPolicy::Select(I,ContextMemory,Context,S);
    if(bPointDue&&IsWatchMode(D.Mode))
    {
        NextPointEvaluation=Now+S.PointInterval;I.bCanWatch=TrySelectWatchPoint(I);
        if(I.bCanWatch)D=PedroContextPolicy::Select(I,ContextMemory,Context,S);
    }
    if(D.Mode!=Context.Mode)ContextMemory.EnteredAt=Now;
    Context=D;
    if(Context.MovementIntent==CMove::Stop||(Context.MovementIntent==CMove::Follow&&NavIntent.bActive&&MovePurpose!=EPedroMovePurpose::Follow)||
        (Context.MovementIntent==CMove::RetainFollow&&MovePurpose!=EPedroMovePurpose::Follow)||
        (Context.MovementIntent==CMove::WatchPoint&&MovePurpose!=EPedroMovePurpose::WatchPoint)||
        (Context.MovementIntent==CMove::Withdraw&&MovePurpose!=EPedroMovePurpose::Withdraw))CancelPedroMove();
}
bool APedroObserverAIController::TryContextPath(const FVector& Candidate,const FVector& Known,bool bWithdraw,FVector& Goal,FNavPathSharedPtr& Path) const
{
    auto* Sys=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());auto* M=ConfiguredMovement.Get();const auto& S=NavigationSettings;
    if(!Sys||!IsValid(M)||!PedroContextPolicy::ValidPoint(Candidate)||!PedroContextPolicy::ValidPoint(Known))return false;
    const auto& Agent=M->GetNavAgentPropertiesRef();const FVector Feet=M->GetFeetLocation();const auto* Data=Sys->GetNavDataForProps(Agent,Feet,FVector::ZeroVector);
    if(!IsValid(Cast<ARecastNavMesh>(Data)))return false;
    FNavLocation Start,End;
    if(!Sys->ProjectPointToNavigation(Feet,Start,FVector(50,50,100),Data)||FVector::Dist2D(Feet,Start.Location)>50||FMath::Abs(Feet.Z-Start.Location.Z)>100||
        !Sys->ProjectPointToNavigation(Candidate,End,FVector(S.ProjectionXY,S.ProjectionXY,S.ProjectionZ),Data)||!PedroNavigationPolicy::AcceptProjection(Candidate,End.Location,Known,S)||
        FMath::Abs(Feet.Z-End.Location.Z)>S.MaxVerticalSeparation)return false;
    FPathFindingQuery Q(this,*Data,Feet,End.Location);Q.SetAllowPartialPaths(false);Q.SetNavAgentProperties(Agent);
    const FPathFindingResult R=Sys->FindPathSync(Agent,Q);
    if(!R.IsSuccessful()||!R.Path.IsValid()||!R.Path->IsValid()||!PedroNavigationPolicy::AcceptPath(!R.Path->IsPartial(),R.Path->ContainsAnyCustomLink(),R.Path->GetLength(),S))return false;
    const auto& P=R.Path->GetPathPoints();if(P.Num()<2)return false;
    FVector Previous=Feet;
    for(const auto& Pt:P){if(!PedroContextPolicy::ValidPoint(Pt.Location)||FNavMeshNodeFlags(Pt.Flags).IsNavLink()||Pt.CustomNavLinkId.IsValid()||!PedroNavigationPolicy::IsSafeSegment(Previous,Pt.Location,Known,S.PersonalSpace,bWithdraw))return false;Previous=Pt.Location;}
    if(!Previous.Equals(End.Location,NavigationSettings.AcceptanceRadius)||!FMath::IsFinite(R.Path->GetCost()))return false;
    Goal=End.Location;Path=R.Path;return true;
}
bool APedroObserverAIController::TrySelectWatchPoint(FPedroContextInput& I)
{
    APawn* O=GetPawn();auto* M=ConfiguredMovement.Get();const auto& S=NavigationSettings;const auto& C=ContextSettings;
    if(!I.bFresh||!IsValid(O)||!IsValid(M)||I.Now<NavIntent.RetryAfter)return false;
    ContextPath.Reset();ContextPathQueries=0;ContextSelectedScore=0;ContextSelectedPoint=NAME_None;++ContextSelectionSerial;ContextCandidates.Reset();ContextRejections.Reset();
    FVector Eye;FRotator Facing;O->GetActorEyesViewPoint(Eye,Facing);
    const FVector Root=O->GetActorLocation();const double RootYaw=O->GetActorRotation().Yaw;const FVector RootOffset=Root-M->GetFeetLocation();
    const auto Visibility=[&](const FVector& At,double Yaw,double Radius)->double
    {
        const double Delta=Yaw-RootYaw;const FVector From=At+(Eye-Root).RotateAngleAxis(Delta,FVector::UpVector);const FVector Forward=Facing.Vector().RotateAngleAxis(Delta,FVector::UpVector);
        const double MaxRange=FMath::Min(Radius,double(SightConfig->SightRadius));
        if(PedroDirectObservation::CheckGeometry(From,Forward,I.Known,MaxRange,SightConfig->PeripheralVisionAngleDegrees)!=EPedroDirectObservationStatus::Visible)return -1;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(PedroWatchPoint),false);Params.AddIgnoredActor(O);Params.AddIgnoredActor(this);FHitResult Hit;
        if(GetWorld()->LineTraceSingleByChannel(Hit,From,I.Known,ECC_Visibility,Params)&&Hit.GetActor()!=ContextPlayer.Get())return -1;
        return FMath::Clamp(FVector::DotProduct(Forward.GetSafeNormal(),(I.Known-From).GetSafeNormal()),0.,1.);
    };
    const auto Fit=[&](const FVector& At){return FMath::Clamp(1-FMath::Abs(FVector::Dist2D(At,I.Known)-S.PreferredDistance)/S.PreferredDistance,0.,1.);};
    const auto Interest=[&](const FVector& At,double Radius){return ContextMemory.LastActivity.EventId.IsValid()&&FVector::Dist2D(At,ContextMemory.LastActivity.ObservedLocation)<=Radius?PedroContextPolicy::Decay(I.Now,ContextMemory.LastActivity.ObservedAt,C.MemoryDecay):0.;};
    const auto Penalty=[&](FName Id){
        if(Id.IsNone())return ContextMemory.LossPoint.IsNone()&&FVector::Dist2D(Root,ContextMemory.LastLossRoot)<100?.6*PedroContextPolicy::Decay(I.Now,ContextMemory.LastLoss,C.MemoryDecay):0.;
        const auto* P=ContextMemory.Points.Find(Id);if(!P)return 0.;
        return FMath::Clamp(FMath::Min(.8,.2*P->Losses)*PedroContextPolicy::Decay(I.Now,P->LastLoss,C.MemoryDecay)+.7*PedroContextPolicy::Decay(I.Now,P->LastFailure,C.MemoryDecay)+.3*PedroContextPolicy::Decay(I.Now,P->LastVisit,C.MemoryDecay),0.,1.);
    };
    FPedroContextCandidate Current;Current.bEligible=true;Current.Goal=Root;Current.Visibility=FMath::Max(0.,Visibility(Root,RootYaw,SightConfig->SightRadius));Current.DistanceFit=Fit(Root);Current.Interest=Interest(Root,1000);Current.Penalty=Penalty(ContextMemory.CurrentPoint);
    for(const auto& P:ContextPoints)if(P.IsValid()&&P->IsUsable()&&P->WatchPointId==ContextMemory.CurrentPoint){Current.Priority=P->BasePriority;break;}
    ContextCurrentScore=PedroContextPolicy::Score(Current);ContextCurrentCandidate=Current;
    TArray<APedroObserverWatchPoint*> Points;TMap<FName,int32> Ids;
    for(const auto& Weak:ContextPoints)if(Weak.IsValid()&&Weak->IsUsable())++Ids.FindOrAdd(Weak->WatchPointId);
    for(const auto& Weak:ContextPoints){auto* P=Weak.Get();if(!IsValid(P)||!P->IsUsable()||P->GetWorld()!=GetWorld()||Ids.FindRef(P->WatchPointId)!=1||P->WatchPointId==ContextMemory.CurrentPoint)continue;
        const FVector At=P->GetActorLocation()+RootOffset;
        if(FVector::Dist2D(At,Root)<100||FVector::Dist2D(At,I.Known)<S.PersonalSpace+S.AcceptanceRadius||FVector::Dist2D(At,I.Known)>P->ObservationRadius||FMath::Abs(At.Z-Root.Z)>S.MaxVerticalSeparation)continue;Points.Add(P);}
    Points.Sort([](const APedroObserverWatchPoint& A,const APedroObserverWatchPoint& B){if(A.BasePriority!=B.BasePriority)return A.BasePriority>B.BasePriority;return A.WatchPointId.LexicalLess(B.WatchPointId);});
    TArray<FPedroContextCandidate> Candidates;TArray<FNavPathSharedPtr> Paths;
    for(int32 J=0;J<FMath::Min(8,Points.Num())&&ContextPathQueries<5;++J)
    {
        auto* P=Points[J];const FVector At=P->GetActorLocation()+RootOffset;
        if(Visibility(At,P->GetActorRotation().Yaw,P->ObservationRadius)<0){ContextRejections.Add(P->WatchPointId.ToString()+TEXT(":view"));continue;}
        FVector Goal;FNavPathSharedPtr Path;++ContextPathQueries;
        if(!TryContextPath(P->GetActorLocation(),I.Known,false,Goal,Path)){ContextRejections.Add(P->WatchPointId.ToString()+TEXT(":path"));continue;}
        const FVector ProjectedRoot=Goal+RootOffset;const double V=Visibility(ProjectedRoot,P->GetActorRotation().Yaw,P->ObservationRadius);if(V<0){ContextRejections.Add(P->WatchPointId.ToString()+TEXT(":projected-view"));continue;}
        FPedroContextCandidate A;A.Id=P->WatchPointId;A.Goal=Goal;A.bEligible=true;A.Visibility=V;A.DistanceFit=Fit(ProjectedRoot);A.Interest=Interest(ProjectedRoot,P->ObservationRadius);A.Priority=P->BasePriority;A.Cost=FMath::Clamp(Path->GetLength()/S.MaxPathLength,0.,1.);A.Penalty=Penalty(A.Id);
        Candidates.Add(A);Paths.Add(Path);
    }
    ContextCandidates=Candidates;
    const int32 Best=PedroContextPolicy::BestCandidate(Candidates,ContextCurrentScore,C.Improvement);if(Best==INDEX_NONE)return false;
    ContextSelectedScore=PedroContextPolicy::Score(Candidates[Best]);ContextSelectedPoint=Candidates[Best].Id;I.SelectedPoint=Candidates[Best].Id;I.SelectedGoal=Candidates[Best].Goal;ContextGoal=Candidates[Best].Goal;ContextPath=Paths[Best];ContextPlanSighting=I.Known;return true;
}
bool APedroObserverAIController::UpdateContextNavigation(double Now)
{
    using Move=EPedroContextMovementIntent;using Mode=EPedroNavigationMode;using Reason=EPedroNavigationReason;
    if(Context.MovementIntent==Move::Follow||Context.MovementIntent==Move::RetainFollow)return false;
    if(Context.MovementIntent==Move::Stop){CancelPedroMove();Navigation={Mode::Idle,Reason::InBand,false};return true;}
    const auto Purpose=Context.MovementIntent==Move::WatchPoint?EPedroMovePurpose::WatchPoint:EPedroMovePurpose::Withdraw;
    if(NavIntent.bActive&&MovePurpose==Purpose){Navigation={Mode::FollowObserved,Reason::Fresh,false};return true;}
    if(Awareness.Status!=EPedroAwarenessStatus::Perceived||!Awareness.bHasLastKnownPlayerLocation||Awareness.ObservedPlayer.Get()!=ContextPlayer.Get()||Now-Awareness.LastSuccessfulSightingTime>NavigationSettings.FreshAge||Now<NavIntent.RetryAfter||Now-Awareness.LastSuccessfulSightingTime<0)return true;
    const FVector Known=Awareness.LastKnownPlayerLocation;FVector Goal;FNavPathSharedPtr Path;
    if(Purpose==EPedroMovePurpose::WatchPoint){Goal=ContextGoal;if(ContextPlanSighting.Equals(Known,.01))Path=ContextPath;ContextPath.Reset();}
    else
    {
        const FVector Root=GetPawn()->GetActorLocation();const FVector Away=(Root-Known).GetSafeNormal2D();ContextPathQueries=0;
        if(FVector::Dist2D(Root,Known)>=ContextSettings.RetreatDistance-NavigationSettings.AcceptanceRadius)
        {ContextMoveEnded(Purpose,NAME_None,true,Now);Navigation={Mode::Idle,Reason::InBand,false};return true;}
        if(!Away.IsNearlyZero())for(double Angle:{0.,30.,-30.,60.,-60.}){++ContextPathQueries;const FVector Candidate=Known+Away.RotateAngleAxis(Angle,FVector::UpVector)*ContextSettings.RetreatDistance;if(TryContextPath(Candidate,Known,true,Goal,Path))break;}
        ContextMemory.bAttackPending=false;
    }
    NavIntent.LastPlan=Now;
    if(!Path.IsValid()||!SubmitPedroPath(Goal,Path,Known,Now,Purpose,Context.WatchPointId))
    {CancelPedroMove();NavIntent.RetryAfter=Now+NavigationSettings.RetryInterval;Navigation={Mode::Blocked,Reason::NoRoute,false};ContextMoveEnded(Purpose,Context.WatchPointId,false,Now);return true;}
    if(NavIntent.bActive)Navigation={Mode::FollowObserved,Reason::Fresh,false};return true;
}
