#include "Encounters/Pedro/PedroObserverAIController.h"

#include "CollisionQueryParams.h"
#include "Encounters/Pedro/PedroDirectObservation.h"
#include "Encounters/Pedro/PedroObserverCharacter.h"
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
    auto Fail=[&](Reason R){CancelPedroMove();NavIntent.RetryAfter=Now+S.RetryInterval;Navigation={Mode::Blocked,R,false};};
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
                const FVector A(Prev.X,Prev.Y,0),B(Pt.Location.X,Pt.Location.Y,0),P(Known.X,Known.Y,0);
                if(FMath::PointDistToSegment(P,A,B)<S.PersonalSpace){bUnsafe=true;break;}
            }
        }
        if(bUnsafe)continue;
        if(!PedroNavigationPolicy::AcceptPath(!R.Path->IsPartial(),bSpecial,R.Path->GetLength(),S))continue;
        const double C=R.Path->GetCost();
        if(PedroNavigationPolicy::BetterCandidate(I,C,Best,Cost)){Best=I;Cost=C;Goal=Projected.Location;Path=R.Path;}
        if(Best==0)break;
    }
    if(!Path.IsValid()){Fail(Reason::NoRoute);return;}
    CancelPedroMove();
    Path->EnableRecalculationOnInvalidation(false);
    FAIMoveRequest Request(Goal);Request.SetUsePathfinding(true);Request.SetAllowPartialPath(false);Request.SetProjectGoalLocation(false);
    Request.SetAcceptanceRadius(S.AcceptanceRadius);Request.SetReachTestIncludesAgentRadius(false);Request.SetReachTestIncludesGoalRadius(false);Request.SetCanStrafe(true);
    if(PF->HasReached(Request)){Navigation={Mode::Idle,Reason::Arrived,false};return;}
    const FAIRequestID ID=PF->RequestMove(Request,Path);
    if(!ID.IsValid()||!PedroNavigationPolicy::OwnsRequest(ID,PF->GetCurrentRequestId())){Fail(Reason::NoRoute);return;}
    OwnedMove=ID;OwnedPath=Path;MoveGoal=Goal;RoutePlayer=Awareness.ObservedPlayer;NavIntent.bActive=true;
    NavIntent.Sighting=Known;NavIntent.SightingTime=Awareness.LastSuccessfulSightingTime;
    MoveStarted=StallStarted=Now;StallOrigin=O->GetActorLocation();++MoveSubmissions;
}
