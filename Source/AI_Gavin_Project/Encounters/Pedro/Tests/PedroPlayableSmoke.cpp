// Native game-process smoke: real saved map, AI possession, real Recast route
// and movement. Tests may place the PLAYER as scenario setup; they never move
// Pedro directly. Manual real-input tests remain mandatory for interactions.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerStart.h"
#include "Components/CapsuleComponent.h"
#include "Encounters/Pedro/PedroDirectObservation.h"
#include "Encounters/Pedro/Tests/PedroSmokeVisibility.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavMesh/RecastNavMesh.h"
#include "Combat/Attack/MeleeAttackComponent.h"
#include "Components/SceneComponent.h"
#include "Encounters/Pedro/PedroPlayableIntegrationActor.h"
#include "Encounters/Pedro/PedroEncounterDirector.h"
#include "Encounters/Pedro/PedroObserverCharacter.h"
#include "Encounters/Pedro/PedroObserverAIController.h"
#include "Encounters/Pedro/PedroObserverWatchPoint.h"
#include "Camera/CameraComponent.h"
#include "Player/MainPlayerCharacter.h"
#include "Player/Interaction/PlayerInteractionComponent.h"
#include "Encounters/Pedro/PedroPrototypeInteractableActor.h"
#include "Encounters/Pedro/PedroObservationSourceComponent.h"

namespace PedroPlayableSmoke
{
struct FState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<APedroObserverCharacter> Observer;
    TWeakObjectPtr<APedroObserverAIController> Controller;
    TWeakObjectPtr<APedroEncounterDirector> Director;
    TWeakObjectPtr<APawn> Player;
    FVector Start=FVector::ZeroVector;
    double Started=-1, WaitingForMove=-1, AttackSince=-1;
    int32 AttackStage=0;
    bool bBegan=false,bPlacedPlayer=false,bFollowProved=false;
};
FState S;

// Game-process setup only. The saved observer never moves/turns for the test:
// candidates originate in real placed actors or measured Recast positions.
// This is deliberately more robust than probing five points at a fixed radius.
struct FSetupSearchStats
{
    int32 Tried=0, Projected=0, Distant=0, CompletePaths=0;
    int32 GeometryVisible=0, ClearRays=0, Unoccupied=0;
    int32 RejectedSnap=0, RejectedPath=0, RejectedGeometry=0;
    int32 BlockedByGeometry=0, BlockedByOccupancy=0;
    FVector Eye=FVector::ZeroVector, Facing=FVector::ZeroVector;
};

bool FindPlayerSetupLocation(UWorld* W, APedroObserverCharacter* Observer,
    APedroObserverAIController* Controller, APawn* Player,
    UNavigationSystemV1* Nav, const ANavigationData* Data,
    const FVector& StartNav, const FNavAgentProperties& AgentProperties,
    FVector& OutPlayerLocation, FSetupSearchStats& Stats)
{
    if (!W || !IsValid(Observer) || !IsValid(Controller) || !IsValid(Player) ||
        !Nav || !IsValid(Data)) return false;
    const ACharacter* Character=Cast<ACharacter>(Player);
    const UCharacterMovementComponent* PlayerMovement=Character ? Character->GetCharacterMovement() : nullptr;
    const UCapsuleComponent* Capsule=Character ? Character->GetCapsuleComponent() : nullptr;
    if (!IsValid(PlayerMovement) || !IsValid(Capsule) ||
        Capsule->GetScaledCapsuleRadius() <= 0.0f ||
        Capsule->GetScaledCapsuleHalfHeight() <= Capsule->GetScaledCapsuleRadius()) return false;

    FVector Eye; FRotator EyeRotation;
    Observer->GetActorEyesViewPoint(Eye,EyeRotation);
    const FVector Facing=EyeRotation.Vector();
    const FVector Forward=Facing.GetSafeNormal2D();
    Stats.Eye=Eye; Stats.Facing=Facing;
    if (Eye.ContainsNaN() || Facing.ContainsNaN() || Forward.IsNearlyZero()) return false;
    const FVector ActorToFeet=Player->GetActorLocation()-PlayerMovement->GetFeetLocation();
    if (ActorToFeet.ContainsNaN()) return false;

    TArray<FVector> Requests;
    // Prefer actual authored locations and the real current player spawn.
    Requests.Add(PlayerMovement->GetFeetLocation());
    for (TActorIterator<APlayerStart> It(W); It; ++It)
        Requests.Add(It->GetActorLocation());
    for (TActorIterator<APedroObserverWatchPoint> It(W); It; ++It)
        Requests.Add(It->GetActorLocation());

    // Bounded local sampling. Offsets are relative to a proved Recast position,
    // never invented absolute level coordinates, and every candidate MUST pass
    // a complete synchronous path, actual sight cone and blocking ray.
    static constexpr float Radii[]={1100.f,950.f,1250.f,800.f,700.f,1350.f};
    static constexpr float Angles[]={0.f,15.f,-15.f,30.f,-30.f,45.f,-45.f,
                                      60.f,-60.f,70.f,-70.f};
    for (float Radius:Radii)
        for (float Angle:Angles)
            Requests.Add(StartNav+Forward.RotateAngleAxis(Angle,FVector::UpVector)*Radius);
    if (Requests.Num()>96) return false; // never silently grow setup queries

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PedroPlayableSetupLOS),false);
    QueryParams.AddIgnoredActor(Observer);
    QueryParams.AddIgnoredActor(Player);
    FCollisionQueryParams OccupancyParams(SCENE_QUERY_STAT(PedroPlayableSetupOccupancy),false);
    OccupancyParams.AddIgnoredActor(Player);
    const FCollisionShape Shape=FCollisionShape::MakeCapsule(
        Capsule->GetScaledCapsuleRadius(),
        FMath::Max(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-3.f));
    for (const FVector& Wish:Requests)
    {
        ++Stats.Tried;
        FNavLocation NavPoint;
        if (!Nav->ProjectPointToNavigation(Wish,NavPoint,FVector(100,100,180),Data)) continue;
        ++Stats.Projected;
        if (FVector::Dist2D(NavPoint.Location,Wish)>100.f ||
            FMath::Abs(NavPoint.Location.Z-Wish.Z)>180.f)
        { ++Stats.RejectedSnap; continue; }
        const float Separation=FVector::Dist2D(StartNav,NavPoint.Location);
        if (Separation<750.f || Separation>1450.f) continue;
        ++Stats.Distant;
        FPathFindingQuery Query(Controller,*Data,StartNav,NavPoint.Location);
        Query.SetAllowPartialPaths(false);
        Query.SetNavAgentProperties(AgentProperties);
        const FPathFindingResult Path=Nav->FindPathSync(AgentProperties,Query);
        if (!Path.IsSuccessful() || !Path.Path.IsValid() || Path.Path->IsPartial() ||
            Path.Path->GetLength()>2200.f || Path.Path->ContainsAnyCustomLink())
        { ++Stats.RejectedPath; continue; }
        ++Stats.CompletePaths;
        const FVector Candidate=NavPoint.Location+ActorToFeet;
        if (PedroDirectObservation::CheckGeometry(Eye,Facing,Candidate,1500.f,75.f)
            != EPedroDirectObservationStatus::Visible)
        { ++Stats.RejectedGeometry; continue; }
        ++Stats.GeometryVisible;
        FHitResult Block;
        if (W->LineTraceSingleByChannel(Block,Eye,Candidate,ECC_Visibility,QueryParams))
        { ++Stats.BlockedByGeometry; continue; }
        ++Stats.ClearRays;
        if (W->OverlapBlockingTestByChannel(Candidate,Player->GetActorQuat(),
            ECC_Pawn,Shape,OccupancyParams))
        { ++Stats.BlockedByOccupancy; continue; }
        ++Stats.Unoccupied;
        OutPlayerLocation=Candidate;
        UE_LOG(LogTemp,Display,
            TEXT("PEDRO_PLAYABLE_SETUP selected=yes attempted=%d distance=%.1f path=%.1f target=%s"),
            Stats.Tried,Separation,Path.Path->GetLength(),*Candidate.ToCompactString());
        return true;
    }
    UE_LOG(LogTemp,Error,
        TEXT("PEDRO_PLAYABLE_SETUP selected=no attempted=%d projected=%d snap_rejected=%d "
             "distant=%d complete_paths=%d path_rejected=%d geometry_visible=%d "
             "geometry_rejected=%d clear_rays=%d occluded=%d unoccupied=%d occupied=%d "
             "eye=%s forward=%s start=%s"),
        Stats.Tried,Stats.Projected,Stats.RejectedSnap,Stats.Distant,Stats.CompletePaths,
        Stats.RejectedPath,Stats.GeometryVisible,Stats.RejectedGeometry,Stats.ClearRays,
        Stats.BlockedByGeometry,Stats.Unoccupied,Stats.BlockedByOccupancy,
        *Eye.ToCompactString(),*Facing.ToCompactString(),*StartNav.ToCompactString());
    return false;
}

bool Finish(const TCHAR* Error=nullptr)
{
    if(Error)
    {
        UE_LOG(LogTemp, Error, TEXT("PEDRO_PLAYABLE_TEST status=FAIL reason=%s"),Error);
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("PEDRO_PLAYABLE_TEST status=SUMMARY passed=6 expected=6 result=PASS"));
    }
    FPlatformMisc::RequestExit(false);
    return false;
}

bool TestTick(float)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("PedroPlayableSmoke")))return false;
    if(!GEngine)return true;
    if(!S.bBegan)
    {
        FString ExpectedMap;
        if(!FParse::Value(FCommandLine::Get(), TEXT("PedroPlayableExpectedMap="), ExpectedMap) ||
           (ExpectedMap != TEXT("/Game/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype") &&
            ExpectedMap != TEXT("/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest")))
            return Finish(TEXT("MissingOrUnapprovedExpectedMapFlag"));
        UWorld* World=nullptr;
        for(const auto& Context:GEngine->GetWorldContexts())
        {
            auto* Candidate=Context.World();
            if(Candidate && Candidate->WorldType==EWorldType::Game && Candidate->HasBegunPlay())
            {World=Candidate;break;}
        }
        if(!World||!World->GetFirstPlayerController()||!World->GetFirstPlayerController()->GetPawn())return true;
        const FString ActualMap = World->GetOutermost()->GetName();
        if(ActualMap != ExpectedMap)
        {
            UE_LOG(LogTemp, Error, TEXT("PEDRO_PLAYABLE_MAP expected=%s actual=%s"),
                *ExpectedMap, *ActualMap);
            return Finish(TEXT("UnexpectedLoadedMap"));
        }
        S.World=World;
        S.Player=World->GetFirstPlayerController()->GetPawn();
        S.bBegan=true;S.Started=World->GetTimeSeconds();
        UE_LOG(LogTemp,Display,TEXT("PEDRO_PLAYABLE_TEST status=BEGIN map=%s expected=6"),
            *World->GetOutermost()->GetName());
    }
    UWorld* W=S.World.Get();
    if(!W||!S.Player.IsValid() || !S.Player->IsPlayerControlled())return Finish(TEXT("PlayerLost"));
    const double Now=W->GetTimeSeconds();
    if(Now-S.Started>30.0)return Finish(TEXT("GlobalDeadlineExceeded"));

    if(!S.Observer.IsValid())
    {
        int32 CountIntegration=0,CountDirector=0,CountObserver=0;
        APedroPlayableIntegrationActor* Integration=nullptr;
        APedroEncounterDirector* Director=nullptr;
        APedroObserverCharacter* Observer=nullptr;
        for(TActorIterator<APedroPlayableIntegrationActor> It(W);It;++It){++CountIntegration;Integration=*It;}
        for(TActorIterator<APedroEncounterDirector> It(W);It;++It){++CountDirector;Director=*It;}
        for(TActorIterator<APedroObserverCharacter> It(W);It;++It){++CountObserver;Observer=*It;}
        const bool bCounts = CountIntegration==1 && CountDirector==1 && CountObserver==1;
        const bool bValidActors = IsValid(Integration) && IsValid(Director) && IsValid(Observer);
        const bool bBindings = bValidActors && Integration->Observer==Observer &&
            Integration->Director==Director && Director->DirectObservationActor.Get()==Observer;
        const bool bPoints = bValidActors && Integration->WatchPoints.Num()>=1;
        const bool bPolicy = bValidActors && Director->bUseDirectObservationAcquisition &&
            !Director->bUsePrototypeAssumedAcquisition &&
            Director->CompletionPolicy==EPedroCompletionPolicy::EvidenceDriven;
        if(!bCounts || !bValidActors || !bBindings || !bPoints || !bPolicy)
        {
            UE_LOG(LogTemp, Error,
                TEXT("PEDRO_PLAYABLE_CONFIGURATION directors=%d observers=%d integrations=%d refs=%s watchpoints=%d policy=%s"),
                CountDirector, CountObserver, CountIntegration, bBindings?TEXT("valid"):TEXT("invalid"),
                IsValid(Integration)?Integration->WatchPoints.Num():0,
                bPolicy?TEXT("valid"):TEXT("invalid"));
            return Finish(TEXT("SavedActorConfiguration"));
        }
        if(!Integration->IsPlayableActive())
        {
            if(Now-S.Started>3.0)return Finish(TEXT("AutoActivationDeadline"));
            return true;
        }
        auto* Controller=Cast<APedroObserverAIController>(Observer->GetController());
        if(!Controller||Controller->GetPawn()!=Observer||
           !Controller->IsAwarenessRunning()||!Controller->IsAttentionRunning()||
           !Controller->IsNavigationRunning()||!Controller->IsContextualObservationRunning())
            return Finish(TEXT("ControllerNotActive"));
        S.Observer=Observer;S.Controller=Controller;S.Director=Director;
        S.Start=Observer->GetActorLocation();
        UE_LOG(LogTemp,Display,TEXT("PEDRO_PLAYABLE_TEST status=PASS case=Configuration"));
        UE_LOG(LogTemp,Display,TEXT("PEDRO_PLAYABLE_TEST status=PASS case=Activation"));
    }

    if(!S.bPlacedPlayer)
    {
        APedroObserverCharacter* O=S.Observer.Get();auto* M=O?O->GetCharacterMovement():nullptr;
        auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
        auto* Data=Nav&&M?Nav->GetNavDataForProps(M->GetNavAgentPropertiesRef(),M->GetFeetLocation(),FVector::ZeroVector):nullptr;
        if(!IsValid(Cast<ARecastNavMesh>(Data)))return Finish(TEXT("MissingSavedRecast"));
        FNavLocation StartNav;
        if(!Nav->ProjectPointToNavigation(M->GetFeetLocation(),StartNav,FVector(70,70,150),Data))
            return Finish(TEXT("ObserverNotOnSavedNavmesh"));
        UE_LOG(LogTemp,Display,TEXT("PEDRO_PLAYABLE_TEST status=PASS case=SavedNavmesh"));
        // Player placement is scenario setup, never a substitute for Pedro
        // walking by real path-following. Re-evaluate the exact runtime sight
        // geometry, Recast path and physical occupancy before placing once.
        FVector Candidate;
        FSetupSearchStats Search;
        if (!FindPlayerSetupLocation(W,O,S.Controller.Get(),S.Player.Get(),Nav,Data,
                                     StartNav.Location,M->GetNavAgentPropertiesRef(),Candidate,Search))
            return Finish(TEXT("NoVisibleReachablePlayerTestPosition"));
        if (!S.Player->SetActorLocation(Candidate,false,nullptr,ETeleportType::TeleportPhysics) ||
            FVector::Dist(S.Player->GetActorLocation(),Candidate)>2.f)
            return Finish(TEXT("ValidPlayerSetupPlacementFailed"));
        // Verify position with the very same observer geometry used by the
        // production direct-awareness query. No fake stimulus or AI yaw.
        FVector Eye;FRotator Facing;O->GetActorEyesViewPoint(Eye,Facing);
        if (PedroDirectObservation::CheckGeometry(Eye,Facing.Vector(),
                S.Player->GetActorLocation(),1500.f,75.f)!=EPedroDirectObservationStatus::Visible)
            return Finish(TEXT("PlayerSetupGeometryDrift"));
        S.bPlacedPlayer=true;S.WaitingForMove=Now;
    }
    if(S.bPlacedPlayer && !S.bFollowProved)
    {
        const double Travel=FVector::Dist2D(S.Observer->GetActorLocation(),S.Start);
        if(Travel>120.0)
        {
            if(S.Director->GetObservationState().GetEvidenceCount()!=0 ||
               S.Director->HasFinalClassification())return Finish(TEXT("MovementFabricatedEvidence"));
            UE_LOG(LogTemp,Display,
                TEXT("PEDRO_PLAYABLE_TEST status=PASS case=AutonomousFollow displacement=%.3f submissions=%u"),
                Travel,S.Controller->GetPedroMoveSubmissions());
            S.bFollowProved=true;
        }
        else if(Now-S.WaitingForMove>13.0)
            return Finish(TEXT("AutonomousMovementDeadline"));
        else return true;
    }
    // Exercise REAL player melee mechanics: TryStartAttack performs the
    // existing windup and sphere sweep. Never synthesize a hit delegate.
    APawn* P=S.Player.Get();
    UMeleeAttackComponent* Melee=P?P->FindComponentByClass<UMeleeAttackComponent>():nullptr;
    APlayerController* PC=W->GetFirstPlayerController();
    if(!IsValid(Melee)||!IsValid(PC)||PC->GetPawn()!=P)
        return Finish(TEXT("MissingRealPlayerMelee"));
    if(S.AttackStage==0)
    {
        if(!Melee->TryStartAttack())return Finish(TEXT("MissAttackNotStarted"));
        S.AttackStage=1;S.AttackSince=Now;return true;
    }
    if(S.AttackStage==1)
    {
        if(Now-S.AttackSince>3.0)return Finish(TEXT("MissAttackTimeout"));
        if(Now-S.AttackSince<.55 || !Melee->CanAttack())return true;
        if(S.Director->GetObservationState().ObserverAttackCount!=0)
            return Finish(TEXT("RemoteMeleeMissFalselyRecorded"));
        UE_LOG(LogTemp,Display,TEXT("PEDRO_PLAYABLE_TEST status=PASS case=RealMeleeMiss"));
        S.AttackStage=2;
    }
    if(S.AttackStage==2)
    {
        // One-time test setup places the player immediately behind Pedro,
        // without moving the observer or altering native perception state.
        const FVector Back=S.Observer->GetActorForwardVector().GetSafeNormal2D();
        if(Back.IsNearlyZero())return Finish(TEXT("ObserverFacingInvalid"));
        const FVector Position=S.Observer->GetActorLocation()-Back*135.0;
        if(!P->SetActorLocation(Position,false))return Finish(TEXT("ContactPlayerPlacementFailed"));
        const FVector AimAt=S.Observer->GetActorLocation()+FVector(0,0,60);
        const USceneComponent* Origin=Melee->GetAttackOriginComponent();
        if(!IsValid(Origin))return Finish(TEXT("MissingRealMeleeOrigin"));
        PC->SetControlRotation((AimAt-Origin->GetComponentLocation()).Rotation());
        S.AttackStage=3;S.AttackSince=Now;return true;
    }
    if(S.AttackStage==3)
    {
        if(Now-S.AttackSince<.07)return true;
        if(!Melee->TryStartAttack())return Finish(TEXT("ContactAttackNotStarted"));
        S.AttackStage=4;S.AttackSince=Now;return true;
    }
    if(S.AttackStage==4)
    {
        if(Now-S.AttackSince>3.0)return Finish(TEXT("RealContactDeadline"));
        const auto& State=S.Director->GetObservationState();
        if(State.ObserverAttackCount==0)return true;
        if(State.ObserverAttackCount!=1 || State.EvidenceHistory.Num()!=1 ||
            State.EvidenceHistory.Last().KnowledgeSource!=EPedroKnowledgeSource::DirectContact ||
            State.Opportunities.GetSampledCoreCount()!=0 ||
            S.Director->HasFinalClassification() || S.Observer->CanBeDamaged())
            return Finish(TEXT("WrongContactProvenanceOrSideEffect"));
        UE_LOG(LogTemp,Display,
            TEXT("PEDRO_PLAYABLE_TEST status=PASS case=RealMeleeContact attackCount=%d provenance=DirectContact"),
            State.ObserverAttackCount);
        return Finish();
    }
    return true;
}
[[maybe_unused]] const auto PlayableTicker =
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TestTick));
}
// Separate opt-in *component-level* game-process validation of genuine Pedro
// interaction dispatch. This never manufactures world events, evidence or progress.
// Only the player can be placed as bounded test scenario setup; never Pedro.
namespace PedroInteractionSmoke
{
struct FTarget
{
    TWeakObjectPtr<APedroPrototypeInteractableActor> Actor;
    TWeakObjectPtr<UPedroObservationSourceComponent> Source;
    bool bCore = false;
};
struct FSearchCounts
{
    int32 Props=0, Projected=0, Available=0, Occluded=0;
    int32 WrongVisibility=0, Occupied=0, BlockedTrace=0;
};
struct FState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AMainPlayerCharacter> Player;
    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<UCameraComponent> Camera;
    TWeakObjectPtr<UPlayerInteractionComponent> Interaction;
    TWeakObjectPtr<APedroObserverCharacter> Observer;
    TWeakObjectPtr<APedroEncounterDirector> Director;
    TWeakObjectPtr<APedroPrototypeInteractableActor> CurrentProp;
    TArray<FTarget> Targets;
    int32 Step=0, WaitFrames=0, EvidenceBefore=0, InspectionBefore=0;
    double Started=0, NoReplaySince=0, RepeatStarted=0;
    bool bProbeOnly=false;
};
FState S;

bool Done(const TCHAR* Reason=nullptr)
{
    if (Reason)
    {
        UE_LOG(LogTemp, Error, TEXT("PEDRO_INTERACTION_TEST status=FAIL reason=%s step=%d"), Reason, S.Step);
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("PEDRO_INTERACTION_TEST status=SUMMARY passed=6 expected=6 result=PASS"));
    }
    FPlatformMisc::RequestExit(false);
    return false;
}

bool ObserverCanSee(FTarget& Target)
{
    return PedroSmokeVisibility::CanSee(S.World.Get(), S.Observer.Get(),
        S.Director.Get(), Target.Actor.Get());
}

bool FindRealInteractionPosition(FTarget& Target, bool bMustBeSeen,
    FVector& OutPosition, FRotator& OutAim, FSearchCounts& Counts)
{
    UWorld* W=S.World.Get(); AMainPlayerCharacter* P=S.Player.Get();
    UCameraComponent* Cam=S.Camera.Get(); APedroPrototypeInteractableActor* A=Target.Actor.Get();
    if (!IsValid(W) || !IsValid(P) || !IsValid(Cam) || !IsValid(A)) return false;
    const UCharacterMovementComponent* Move=P->GetCharacterMovement();
    const UCapsuleComponent* Capsule=P->GetCapsuleComponent();
    UNavigationSystemV1* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
    if (!Move || !Capsule || !Nav) return false;
    const ANavigationData* Data=Nav->GetNavDataForProps(Move->GetNavAgentPropertiesRef(),
        Move->GetFeetLocation(),FVector::ZeroVector);
    if (!IsValid(Cast<ARecastNavMesh>(Data))) return false;
    const FVector FeetOffset=P->GetActorLocation()-Move->GetFeetLocation();
    const FVector CameraOffset=Cam->GetComponentLocation()-P->GetActorLocation();
    const FCollisionShape CapsuleShape=FCollisionShape::MakeCapsule(
        Capsule->GetScaledCapsuleRadius(),
        FMath::Max(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-3.f));
    FCollisionQueryParams PlayerTrace(SCENE_QUERY_STAT(PedroFlowInteractionTrace),false);
    PlayerTrace.AddIgnoredActor(P);
    FCollisionQueryParams Occupancy(SCENE_QUERY_STAT(PedroFlowOccupancy),false);
    Occupancy.AddIgnoredActor(P);
    FVector BoundsOrigin, BoundsExtent;
    A->GetActorBounds(false,BoundsOrigin,BoundsExtent);
    static constexpr float Radii[]={120.f,160.f,200.f};
    for (float Radius:Radii)
    {
        for (int32 AngleIndex=0;AngleIndex<16;++AngleIndex)
        {
            ++Counts.Props;
            const double Angle=2.0*PI*static_cast<double>(AngleIndex)/16.0;
            const FVector Offset(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0);
            FNavLocation Projected;
            if (!Nav->ProjectPointToNavigation(A->GetActorLocation()+Offset,Projected,
                FVector(70,70,160),Data)) continue;
            ++Counts.Projected;
            const FVector Candidate=Projected.Location+FeetOffset;
            if (FVector::Dist2D(Candidate,A->GetActorLocation())>215.f) continue;
            if (W->OverlapBlockingTestByChannel(Candidate,P->GetActorQuat(),ECC_Pawn,
                CapsuleShape,Occupancy)) {++Counts.Occupied;continue;}
            const FVector Eye=Candidate+CameraOffset;
            if (FVector::Dist(Eye,BoundsOrigin)>=245.f) continue;
            FHitResult Hit;
            if (!W->LineTraceSingleByChannel(Hit,Eye,BoundsOrigin,ECC_Visibility,PlayerTrace)
                || Hit.GetActor()!=A) {++Counts.BlockedTrace;continue;}
            ++Counts.Available;
            const bool bVisible=ObserverCanSee(Target);
            if (bVisible!=bMustBeSeen) {++Counts.WrongVisibility;continue;}
            OutPosition=Candidate;
            OutAim=(BoundsOrigin-Eye).Rotation();
            return true;
        }
    }
    return false;
}

bool PositionForRealDispatch(bool bMustBeSeen)
{
    FSearchCounts Totals;
    for (FTarget& Target:S.Targets)
    {
        if (!Target.bCore || !Target.Actor.IsValid() || !Target.Source.IsValid()) continue;
        if (S.CurrentProp.IsValid() && S.CurrentProp==Target.Actor) continue;
        FVector Position; FRotator Aim;
        FSearchCounts Counts;
        const bool bFound=FindRealInteractionPosition(Target,bMustBeSeen,Position,Aim,Counts);
        Totals.Props+=Counts.Props; Totals.Projected+=Counts.Projected;
        Totals.Available+=Counts.Available; Totals.Occupied+=Counts.Occupied;
        Totals.BlockedTrace+=Counts.BlockedTrace; Totals.WrongVisibility+=Counts.WrongVisibility;
        if (!bFound) continue;
        if (!S.Player->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics) ||
            FVector::Dist(S.Player->GetActorLocation(),Position)>2.f) continue;
        S.Controller->SetControlRotation(Aim);
        S.CurrentProp=Target.Actor;
        S.InspectionBefore=Target.Actor->GetSuccessfulInspectionCount();
        S.EvidenceBefore=S.Director->GetObservationState().GetEvidenceCount();
        S.WaitFrames=0;
        UE_LOG(LogTemp,Display,
            TEXT("PEDRO_INTERACTION_SETUP target=%s opportunity=%s visible=%s tested=%d projected=%d"),
            *Target.Actor->GetName(),*Target.Source->OpportunityId.ToString(),
            bMustBeSeen?TEXT("yes"):TEXT("no"),Totals.Props,Totals.Projected);
        return true;
    }
    UE_LOG(LogTemp,Error,
        TEXT("PEDRO_INTERACTION_SETUP selected=no requested_visible=%s tested=%d projected=%d usable=%d occupied=%d blocked=%d visibility_mismatch=%d"),
        bMustBeSeen?TEXT("yes"):TEXT("no"),Totals.Props,Totals.Projected,
        Totals.Available,Totals.Occupied,Totals.BlockedTrace,Totals.WrongVisibility);
    return false;
}

enum class EDispatchReadiness
{
    Ready, InvalidSetup, PlayerTraceMismatch, ObserverVisibilityMismatch
};

EDispatchReadiness CheckPreparedDispatch(bool bExpectedVisible, FHitResult& Hit)
{
    APedroPrototypeInteractableActor* Prop=S.CurrentProp.Get();
    UCameraComponent* Cam=S.Camera.Get();
    if (!IsValid(Prop)||!IsValid(Cam)||!S.Interaction.IsValid()||!S.World.IsValid()||
        !S.Player.IsValid()||!S.Observer.IsValid()||!S.Director.IsValid())
        return EDispatchReadiness::InvalidSetup;
    FTarget* Tracked=S.Targets.FindByPredicate(
        [Prop](const FTarget& T){return T.Actor.Get()==Prop;});
    if (!Tracked || !Tracked->Source.IsValid())return EDispatchReadiness::InvalidSetup;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PedroFlowDispatchProof),false);
    Params.AddIgnoredActor(S.Player.Get());
    if (!S.World->LineTraceSingleByChannel(Hit,Cam->GetComponentLocation(),
        Cam->GetComponentLocation()+Cam->GetForwardVector()*250.f,ECC_Visibility,Params)
        || Hit.GetActor()!=Prop)
        return EDispatchReadiness::PlayerTraceMismatch;
    if (ObserverCanSee(*Tracked)!=bExpectedVisible)
        return EDispatchReadiness::ObserverVisibilityMismatch;
    return EDispatchReadiness::Ready;
}

void LogDispatchBlocker(EDispatchReadiness Readiness, const FHitResult& Hit)
{
    const TCHAR* Reason=Readiness==EDispatchReadiness::PlayerTraceMismatch?TEXT("PlayerTraceMismatch"):
        Readiness==EDispatchReadiness::ObserverVisibilityMismatch?TEXT("ObserverVisibilityMismatch"):
        TEXT("InvalidSetup");
    UE_LOG(LogTemp,Display,TEXT("PEDRO_INTERACTION_DIAG step=%d reason=%s target=%s hit=%s"),
        S.Step,Reason,*GetNameSafe(S.CurrentProp.Get()),*GetNameSafe(Hit.GetActor()));
}

bool DispatchPrepared(bool bExpectedVisible)
{
    FHitResult Hit;
    const auto Readiness=CheckPreparedDispatch(bExpectedVisible,Hit);
    if (Readiness!=EDispatchReadiness::Ready)
    {
        LogDispatchBlocker(Readiness,Hit);
        return false;
    }
    if (!S.Interaction->TryInteract())
    {
        UE_LOG(LogTemp,Display,TEXT("PEDRO_INTERACTION_DIAG step=%d reason=TryInteractRejectedVerifiedTrace"),S.Step);
        return false;
    }
    return true;
}

bool Tick(float)
{
    const bool bRun=FParse::Param(FCommandLine::Get(),TEXT("PedroInteractionSmoke"));
    if(!bRun) return false;
    if(!GEngine) return true;
    if(S.Step==0)
    {
        FString Expected;
        if(!FParse::Value(FCommandLine::Get(),TEXT("PedroInteractionExpectedMap="),Expected) ||
            (Expected!=TEXT("/Game/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype") &&
             Expected!=TEXT("/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest")))
            return Done(TEXT("MissingOrUnapprovedExpectedMap"));
        UWorld* W=nullptr;
        for(const FWorldContext& Context:GEngine->GetWorldContexts())
            if(UWorld* Candidate=Context.World();Candidate && Candidate->WorldType==EWorldType::Game && Candidate->HasBegunPlay())
                {W=Candidate;break;}
        if(!W||!W->GetFirstPlayerController()||!W->GetFirstPlayerController()->GetPawn())return true;
        if(W->GetOutermost()->GetName()!=Expected)return Done(TEXT("IncorrectSavedMap"));
        auto* PC=W->GetFirstPlayerController();
        auto* P=Cast<AMainPlayerCharacter>(PC->GetPawn());
        if(!IsValid(P)||!P->IsPlayerControlled())return Done(TEXT("NotRealPlayerPawn"));
        UCameraComponent* Camera=P->FindComponentByClass<UCameraComponent>();
        UPlayerInteractionComponent* Interaction=P->GetInteractionComponent();
        APedroEncounterDirector* D=nullptr;APedroObserverCharacter* PhysicalObserver=nullptr;
        int32 Directors=0,Observers=0,Integrations=0;
        APedroPlayableIntegrationActor* Integration=nullptr;
        for(TActorIterator<APedroEncounterDirector> It(W);It;++It){++Directors;D=*It;}
        for(TActorIterator<APedroObserverCharacter> It(W);It;++It){++Observers;PhysicalObserver=*It;}
        for(TActorIterator<APedroPlayableIntegrationActor> It(W);It;++It){++Integrations;Integration=*It;}
        if(!IsValid(Camera)||!IsValid(Interaction)||Directors!=1||Observers!=1||Integrations!=1||
           !IsValid(D)||!IsValid(PhysicalObserver)||!IsValid(Integration)||!Integration->IsPlayableActive()||
           Integration->Observer!=PhysicalObserver||Integration->Director!=D||
           D->DirectObservationActor!=PhysicalObserver||!D->bUseDirectObservationAcquisition||
           D->bUsePrototypeAssumedAcquisition||D->CompletionPolicy!=EPedroCompletionPolicy::EvidenceDriven)
            return Done(TEXT("RealPlayerOrDirectorConfiguration"));
        S.World=W;S.Player=P;S.Controller=PC;S.Camera=Camera;S.Interaction=Interaction;
        S.Observer=PhysicalObserver;S.Director=D;S.Started=W->GetTimeSeconds();
        for(TActorIterator<APedroPrototypeInteractableActor> It(W);It;++It)
        {
            APedroPrototypeInteractableActor* Prop=*It;
            auto* Source=Prop->FindComponentByClass<UPedroObservationSourceComponent>();
            if(!IsValid(Source)||!Source->bEvidenceEnabled||Source->SubjectId.IsNone()||
               Source->OpportunityId.IsNone())continue;
            bool bCore=false;
            for(const auto& Definition:D->OpportunityDefinitions)
                if(Definition.OpportunityId==Source->OpportunityId &&
                   Definition.Tier==EPedroOpportunityTier::Core)bCore=true;
            if(bCore)
            {
                FTarget Target;
                Target.Actor=Prop;Target.Source=Source;Target.bCore=true;
                S.Targets.Add(Target);
            }
        }
        S.Targets.Sort([](const FTarget& A,const FTarget& B){
            return A.Actor->GetName()<B.Actor->GetName();});
        const auto& State=D->GetObservationState();
        if(State.Opportunities.GetCoreCount()!=4 ||
           State.Opportunities.GetRequiredCoreSamples(D->CoreSampleFraction)!=3 ||
           S.Targets.Num()<3 || State.GetEvidenceCount()!=0 ||
           State.Opportunities.GetSampledCoreCount()!=0 || D->HasFinalClassification())
            return Done(TEXT("UnexpectedCoreOpportunityBaseline"));
        UE_LOG(LogTemp,Display,TEXT("PEDRO_INTERACTION_TEST status=BEGIN map=%s expected=6 core=4 required=3 props=%d"),
            *Expected,S.Targets.Num());
        UE_LOG(LogTemp,Display,TEXT("PEDRO_INTERACTION_TEST status=PASS case=Configuration"));
        S.Step=1;return true;
    }
    if(!S.World.IsValid()||!S.Player.IsValid()||!S.Observer.IsValid()||!S.Director.IsValid()||
       !S.Controller.IsValid()||!S.Interaction.IsValid())return Done(TEXT("RuntimeActorLost"));
    if(S.World->GetTimeSeconds()-S.Started>35.)return Done(TEXT("Deadline"));
    const auto& State=S.Director->GetObservationState();
    if(S.Step==1)
    {
        S.Controller->SetControlRotation(FRotator(-88.f,0.f,0.f));
        S.Step=2;return true;
    }
    if(S.Step==2)
    {
        if(!S.Camera.IsValid())return Done(TEXT("CameraLost"));
        if(S.Interaction->TryInteract() || State.GetEvidenceCount()!=0 ||
           State.Opportunities.GetSampledCoreCount()!=0)
            return Done(TEXT("FailedDispatchProducedAction"));
        UE_LOG(LogTemp,Display,TEXT("PEDRO_INTERACTION_TEST status=PASS case=FailedDispatch"));
        S.Step=3;return true;
    }
    if(S.Step==3)
    {
        if(!PositionForRealDispatch(false))return Done(TEXT("NoReachableHiddenCoreInteraction"));
        S.Step=4;return true;
    }
    if(S.Step==4)
    {
        if(++S.WaitFrames<2)return true;
        if(!DispatchPrepared(false))return Done(TEXT("HiddenRealDispatchNotProved"));
        if(S.CurrentProp->GetSuccessfulInspectionCount()!=S.InspectionBefore+1 ||
           State.GetEvidenceCount()!=S.EvidenceBefore ||
           State.Opportunities.GetSampledCoreCount()!=0)
            return Done(TEXT("HiddenWorldTruthLeakedIntoKnowledge"));
        UE_LOG(LogTemp,Display,TEXT("PEDRO_INTERACTION_TEST status=PASS case=HiddenWorldTruthNotKnowledge inspected=%d evidence=%d"),
            S.CurrentProp->GetSuccessfulInspectionCount(),State.GetEvidenceCount());
        S.NoReplaySince=S.World->GetTimeSeconds();
        S.Step=5;return true;
    }
    if(S.Step==5)
    {
        if(S.World->GetTimeSeconds()-S.NoReplaySince<.5)return true;
        if(State.GetEvidenceCount()!=0 || State.Opportunities.GetSampledCoreCount()!=0)
            return Done(TEXT("MissedEvidenceReplayed"));
        UE_LOG(LogTemp,Display,TEXT("PEDRO_INTERACTION_TEST status=PASS case=NoReplay"));
        S.Step=6;return true;
    }
    if(S.Step==6)
    {
        if(!PositionForRealDispatch(true))return Done(TEXT("NoReachableVisibleCoreInteraction"));
        S.Step=7;return true;
    }
    if(S.Step==7)
    {
        if(++S.WaitFrames<2)return true;
        if(!DispatchPrepared(true))return Done(TEXT("VisibleRealDispatchNotProved"));
        if(S.CurrentProp->GetSuccessfulInspectionCount()!=S.InspectionBefore+1 ||
           State.GetEvidenceCount()!=S.EvidenceBefore+1 ||
           State.Opportunities.GetSampledCoreCount()!=1)
            return Done(TEXT("VisibleWorldEventNotAcquired"));
        const auto& Record=State.EvidenceHistory.Last();
        const auto* Source=S.CurrentProp->FindComponentByClass<UPedroObservationSourceComponent>();
        if(!Source || Record.KnowledgeSource!=EPedroKnowledgeSource::DirectObservation ||
           Record.Event.Kind!=EPedroEvidenceKind::InspectedOptionalObject ||
           Record.Event.SubjectActor.Get()!=S.CurrentProp.Get() ||
           Record.Event.OpportunityId!=Source->OpportunityId || !Record.Event.EventId.IsValid() ||
           S.Director->HasFinalClassification())
            return Done(TEXT("IncorrectDirectEvidenceProvenance"));
        UE_LOG(LogTemp,Display,TEXT("PEDRO_INTERACTION_TEST status=PASS case=VisibleDirectObservation evidence=%d core=%d event=%s"),
            State.GetEvidenceCount(),State.Opportunities.GetSampledCoreCount(),*Record.Event.EventId.ToString());
        S.InspectionBefore=S.CurrentProp->GetSuccessfulInspectionCount();
        S.EvidenceBefore=State.GetEvidenceCount();
        S.RepeatStarted=FPlatformTime::Seconds();
        S.WaitFrames=0;
        S.Step=8;return true;
    }
    if(S.Step==8)
    {
        // The first interaction can change Pedro's attention on the next tick.
        // Wait for live geometry, never replay an action that was dispatched.
        if (!S.CurrentProp.IsValid()||!S.Camera.IsValid())return Done(TEXT("RepeatActorLost"));
        if (S.CurrentProp->GetSuccessfulInspectionCount()!=S.InspectionBefore ||
            State.GetEvidenceCount()!=S.EvidenceBefore ||
            State.Opportunities.GetSampledCoreCount()!=1 || S.Director->HasFinalClassification())
            return Done(TEXT("RepeatBaselineChangedWhileWaiting"));
        FHitResult Hit;
        const auto Readiness=CheckPreparedDispatch(true,Hit);
        if (Readiness==EDispatchReadiness::InvalidSetup)
        {
            LogDispatchBlocker(Readiness,Hit);
            return Done(TEXT("RepeatInvalidSetup"));
        }
        if (FPlatformTime::Seconds()-S.RepeatStarted>=5.0)
        {
            if (Readiness!=EDispatchReadiness::Ready)LogDispatchBlocker(Readiness,Hit);
            return Done(TEXT("RepeatGeometryDeadline"));
        }
        if (Readiness!=EDispatchReadiness::Ready)
        {
            if (++S.WaitFrames==1)LogDispatchBlocker(Readiness,Hit);
            if (Readiness==EDispatchReadiness::PlayerTraceMismatch)
            {
                // Re-aim only the test player at the same prop; no teleport,
                // observer manipulation, or synthetic interaction.
                FVector Origin,Extent;
                S.CurrentProp->GetActorBounds(false,Origin,Extent);
                S.Controller->SetControlRotation((Origin-S.Camera->GetComponentLocation()).Rotation());
            }
            return true;
        }
        if(!S.Interaction->TryInteract())return Done(TEXT("RepeatTryInteractRejectedVerifiedTrace"));
        if(S.CurrentProp->GetSuccessfulInspectionCount()!=S.InspectionBefore+1 ||
           State.Opportunities.GetSampledCoreCount()!=1 ||
           State.GetEvidenceCount()<S.EvidenceBefore ||
           State.GetEvidenceCount()>S.EvidenceBefore+1 ||
           S.Director->HasFinalClassification())
            return Done(TEXT("RepeatedCoreInflatedOrPrematureResolution"));
        UE_LOG(LogTemp,Display,TEXT("PEDRO_INTERACTION_TEST status=PASS case=RepeatNoExtraCore core=%d evidence=%d"),
            State.Opportunities.GetSampledCoreCount(),State.GetEvidenceCount());
        return Done();
    }
    return Done(TEXT("UnreachableStage"));
}
[[maybe_unused]] const auto PedroInteractionTicker=
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
} // namespace PedroInteractionSmoke

#endif
