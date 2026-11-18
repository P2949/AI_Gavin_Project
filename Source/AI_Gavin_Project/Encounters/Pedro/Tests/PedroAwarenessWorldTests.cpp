// Opt-in standalone Unreal Editor game-process tests. No asset writes.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/BrushComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/NavMeshPath.h"
#include "Navigation/PathFollowingComponent.h"
#include "Components/SceneComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Perception/AIPerceptionComponent.h"
#include "UObject/UnrealType.h"
#include "Encounters/Pedro/PedroDirectObservation.h"
#include "Encounters/Pedro/PedroEncounterDirector.h"
#include "Encounters/Pedro/PedroObserverAIController.h"
#include "Encounters/Pedro/PedroObserverWatchPoint.h"
#include "Combat/Health/HealthComponent.h"
#include "Encounters/Pedro/PedroObserverCharacter.h"
#include "Encounters/Pedro/PedroObservationSourceComponent.h"
#include "Encounters/Pedro/PedroPrototypeInteractableActor.h"
#include "Interaction/Interactable.h"
#include "World/Encounters/EncounterDefinition.h"
#include "World/Encounters/EncounterProgressSubsystem.h"

namespace PedroAwarenessRuntime
{
constexpr const TCHAR* Map = TEXT("/Game/AI_Gavin/Encounters/Pedro/Validation/L_Pedro_ObserverActorTest");
const FVector ArenaOrigin(100000,100000,5000);
using EStatus = EPedroAwarenessStatus;
using ETrace = EPedroDirectObservationStatus;
using FCheck = TFunction<bool()>;
struct FStep
{
    FString Case;
    FString Name;
    FCheck Setup;
    FCheck Check;
    FCheck Guard;
    double Hold = 0.25;
};
struct FRun
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<APlayerController> PC;
    TWeakObjectPtr<APawn> Player;
    TWeakObjectPtr<APedroObserverCharacter> Observer, SecondObserver, Distractor;
    TWeakObjectPtr<APedroObserverAIController> Controller, SecondController;
    TWeakObjectPtr<AActor> Blocker;
    TWeakObjectPtr<UBoxComponent> Box;
    TWeakObjectPtr<APedroEncounterDirector> Director;
    TWeakObjectPtr<APedroPrototypeInteractableActor> A, B, C;
    TWeakObjectPtr<UEncounterDefinition> Definition;
    TWeakObjectPtr<AActor> Gentle, Seeking, Disruptive;
    TArray<FGuid> Emitted, ExpectedAcquired;
    FPedroAwarenessState Frozen;
    FVector ObserverLocation = FVector::ZeroVector;
    FRotator ObserverRotation = FRotator::ZeroRotator;
    FTimerHandle DestroyedTimer;
    TArray<FStep> Steps;
    int32 Index = 0, Passed = 0, StableFrames = 0;
    int32 InitialProgress = 0;
    double StartTime = 0, StableTime = 0;
    bool bSetup = false, bIntegration = false, bBegan = false, bAttention = false, bNavigation = false, bNavProbe = false, bContext = false, bContextProbe = false;
};
FRun Run;

const TCHAR* StatusName(EStatus S)
{
    switch (S) { case EStatus::Unaware: return TEXT("Unaware"); case EStatus::Perceived: return TEXT("Perceived"); case EStatus::LostSight: return TEXT("LostSight"); }
    return TEXT("Invalid");
}
const TCHAR* Yes(bool B) { return B ? TEXT("yes") : TEXT("no"); }
const FPedroAwarenessState& State()
{
    static const FPedroAwarenessState Empty;
    return Run.Controller.IsValid() ? Run.Controller->GetAwarenessState() : Empty;
}
FString VectorString(const FVector& V)
{
    return FString::Printf(TEXT("%.3f,%.3f,%.3f"),V.X,V.Y,V.Z);
}
FString Ids(const TArray<FGuid>& Events)
{
    FString Result;
    for (const FGuid& Id : Events) { if (!Result.IsEmpty()) Result += TEXT(","); Result += Id.ToString(EGuidFormats::Digits); }
    return Result.IsEmpty() ? FString(TEXT("-")) : Result;
}
FString EvidenceIds()
{
    TArray<FGuid> Events;
    if (Run.Director.IsValid()) for (const auto& R : Run.Director->GetObservationState().EvidenceHistory) Events.Add(R.Event.EventId);
    return Ids(Events);
}
UEncounterProgressSubsystem* Progress()
{
    auto* GI = Run.World.IsValid() ? Run.World->GetGameInstance() : nullptr;
    return GI ? GI->GetSubsystem<UEncounterProgressSubsystem>() : nullptr;
}
int32 EvidenceCount() { return Run.Director.IsValid() ? Run.Director->GetObservationState().GetEvidenceCount() : 0; }
int32 SampledCount() { return Run.Director.IsValid() ? Run.Director->GetObservationState().Opportunities.GetSampledCoreCount() : 0; }
bool Finalized() { return Run.Director.IsValid() && Run.Director->HasFinalClassification(); }
void LogStep(const TCHAR* Kind, const FStep& S)
{
    const auto& A = State();
    const FVector Actual = Run.Player.IsValid() ? Run.Player->GetActorLocation() : FVector::ZeroVector;
    FVector NativeLocation = FVector::ZeroVector; float NativeAge = -1.0f;
    const bool bNative = Run.Controller.IsValid() && Run.Controller->GetNativeSightSample(Run.Player.Get(),NativeLocation,NativeAge);
    FVector Eye = FVector::ZeroVector; FRotator Facing = FRotator::ZeroRotator;
    if (Run.Observer.IsValid()) Run.Observer->GetActorEyesViewPoint(Eye,Facing);
    const FPedroAwarenessState* Other = Run.SecondController.IsValid() ? &Run.SecondController->GetAwarenessState() : nullptr;
    UE_LOG(LogTemp, Display, TEXT("PEDRO_AWARENESS_TEST status=%s case=%s step=%s awareness=%s native=%s has=%s known=%s actual=%s time=%.6f running=%s target=%s player=%s evidence=%d sampled=%d emitted=%s acquired=%s final=%s progress=%d nativeLoc=%s nativeAge=%.6f eye=%s forward=%s other=%s otherNative=%s distractorNative=%s destroyedTimerActive=%s"),
        Kind, S.Case.IsEmpty() ? TEXT("-") : *S.Case, *S.Name, StatusName(A.Status),
        Yes(bNative),
        Yes(A.bHasLastKnownPlayerLocation), *VectorString(A.LastKnownPlayerLocation), *VectorString(Actual),
        A.LastSuccessfulSightingTime, Yes(Run.Controller.IsValid() && Run.Controller->IsAwarenessRunning()),
        *GetNameSafe(A.ObservedPlayer.Get()), *GetNameSafe(Run.Player.Get()), EvidenceCount(), SampledCount(),
        *Ids(Run.Emitted), *EvidenceIds(), Yes(Finalized()), Progress() ? Progress()->GetResolvedEncounterCount() : -1,
        *VectorString(NativeLocation),NativeAge,*VectorString(Eye),*VectorString(Facing.Vector()),
        Other ? StatusName(Other->Status) : TEXT("None"),
        Yes(Run.SecondController.IsValid() && Run.SecondController->HasNativeSight(Run.Player.Get())),
        Yes(Run.Controller.IsValid() && Run.Controller->HasNativeSight(Run.Distractor.Get())),
        Yes(Run.World.IsValid() && Run.DestroyedTimer.IsValid() && Run.World->GetTimerManager().TimerExists(Run.DestroyedTimer)));
}
// Fixture only: ACharacter::Restart can restore walking/falling on possession.
// Freeze after each possession; never re-freeze per tick to hide pose drift.
void LogFixtureMotion(const TCHAR* Label, APawn* P)
{
    const auto* Character = Cast<ACharacter>(P);
    const auto* Movement = IsValid(Character) ? Character->GetCharacterMovement() : nullptr;
    UE_LOG(LogTemp, Display, TEXT("PEDRO_AWARENESS_FIXTURE role=%s pawn=%s controller=%s position=%s rotation=%s mode=%d tick=%s velocity=%s"),
        Label, *GetNameSafe(P), *GetNameSafe(IsValid(P) ? P->GetController() : nullptr),
        *VectorString(IsValid(P) ? P->GetActorLocation() : FVector::ZeroVector),
        *(IsValid(P) ? P->GetActorRotation().ToString() : FString(TEXT("None"))),
        IsValid(Movement) ? static_cast<int32>(static_cast<EMovementMode>(Movement->MovementMode)) : -1,
        Yes(IsValid(Movement) && Movement->IsComponentTickEnabled()),
        *VectorString(IsValid(Movement) ? Movement->Velocity : FVector::ZeroVector));
}
bool Freeze(APawn* P)
{
    auto* Character = Cast<ACharacter>(P);
    auto* Movement = IsValid(Character) ? Character->GetCharacterMovement() : nullptr;
    if (!IsValid(Movement)) return false;
    Movement->StopMovementImmediately();
    Movement->DisableMovement();
    Movement->SetComponentTickEnabled(false);
    LogFixtureMotion(TEXT("Freeze"), P);
    return Movement->MovementMode == MOVE_None && !Movement->IsComponentTickEnabled() && Movement->Velocity.IsNearlyZero();
}
bool Place(AActor* Actor, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator)
{
    if (!IsValid(Actor)) return false;
    Actor->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    return Actor->GetActorLocation().Equals(Location,0.1) && Actor->GetActorRotation().Equals(Rotation,0.1);
}
bool MovePlayer(const FVector& Relative)
{
    return Run.Player.IsValid() && Place(Run.Player.Get(),ArenaOrigin + Relative);
}
bool TurnObserver(double Yaw)
{
    if (!Run.Observer.IsValid()) return false;
    const FRotator R(0,Yaw,0);
    Run.Observer->SetActorRotation(R, ETeleportType::TeleportPhysics);
    Run.ObserverLocation = Run.Observer->GetActorLocation();
    Run.ObserverRotation = R;
    return Run.Observer->GetActorRotation().Equals(R,0.1);
}
bool PoseUnchanged()
{
    const bool bUnchanged = Run.Observer.IsValid() && Run.Observer->GetActorLocation().Equals(Run.ObserverLocation,0.1) &&
        Run.Observer->GetActorRotation().Equals(Run.ObserverRotation,0.1);
    if (!bUnchanged)
    {
        UE_LOG(LogTemp, Error, TEXT("PEDRO_AWARENESS_FIXTURE guard=PoseUnchanged expectedPosition=%s expectedRotation=%s"),
            *VectorString(Run.ObserverLocation), *Run.ObserverRotation.ToString());
        LogFixtureMotion(TEXT("PoseUnchanged"), Run.Observer.Get());
    }
    return bUnchanged;
}
template<class T> T* SpawnAt(const FVector& At, const FRotator& R = FRotator::ZeroRotator)
{
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    return Run.World->SpawnActor<T>(At,R,Params);
}
APedroObserverCharacter* SpawnObserver(const FVector& At, const FRotator& Rotation)
{
    auto* O = SpawnAt<APedroObserverCharacter>(At,Rotation);
    if (!IsValid(O)) return nullptr;
    if (auto* Existing = O->GetController(); IsValid(Existing)) { Existing->UnPossess(); Existing->Destroy(); }
    if (!Freeze(O)) { O->Destroy(); return nullptr; }
    return O;
}
APedroObserverAIController* Attach(APedroObserverCharacter* O)
{
    if (!IsValid(O)) return nullptr;
    if (auto* Existing = O->GetController(); IsValid(Existing)) { Existing->UnPossess(); Existing->Destroy(); }
    auto* NewController = SpawnAt<APedroObserverAIController>(O->GetActorLocation());
    if (!IsValid(NewController)) return nullptr;
    NewController->Possess(O);
    if (NewController->GetPawn() != O || !Freeze(O))
    {
        NewController->UnPossess(); NewController->Destroy(); return nullptr;
    }
    return NewController;
}
void DestroyController(TWeakObjectPtr<APedroObserverAIController>& C)
{
    if (C.IsValid()) { C->UnPossess(); C->Destroy(); }
    C.Reset();
}
bool FreshController()
{
    DestroyController(Run.Controller);
    Run.Controller = Attach(Run.Observer.Get());
    return Run.Controller.IsValid() && Run.Controller->GetPawn() == Run.Observer.Get() && Run.Controller->IsAwarenessRunning();
}
bool EmptyAwareness()
{
    const auto& S = State();
    return S.Status == EStatus::Unaware && !S.bHasLastKnownPlayerLocation && !S.ObservedPlayer.IsValid() &&
        S.LastKnownPlayerLocation.IsZero() && S.LastSuccessfulSightingTime == 0;
}
bool NoSighting()
{
    return Run.Controller.IsValid() && Run.Controller->IsAwarenessRunning() && EmptyAwareness() && !Run.Controller->HasNativeSight(Run.Player.Get());
}
bool Visible()
{
    return Run.Controller.IsValid() && Run.Controller->IsAwarenessRunning() && Run.Player.IsValid() &&
        Run.Player->IsPlayerControlled() && State().Status == EStatus::Perceived &&
        State().ObservedPlayer.Get() == Run.Player.Get() && State().bHasLastKnownPlayerLocation &&
        State().LastKnownPlayerLocation.Equals(Run.Player->GetActorLocation(), 1.0) && Run.Controller->HasNativeSight(Run.Player.Get());
}
bool FreezeMemory()
{
    const auto& S = State();
    if (!S.bHasLastKnownPlayerLocation) return false;
    Run.Frozen = S;
    return true;
}
bool MemoryUnchanged()
{
    const auto& S = State();
    return S.bHasLastKnownPlayerLocation && S.ObservedPlayer == Run.Frozen.ObservedPlayer &&
        S.LastKnownPlayerLocation == Run.Frozen.LastKnownPlayerLocation && S.LastSuccessfulSightingTime == Run.Frozen.LastSuccessfulSightingTime;
}
bool Lost()
{
    return Run.Controller.IsValid() && Run.Controller->IsAwarenessRunning() && State().Status == EStatus::LostSight &&
        MemoryUnchanged() && !Run.Controller->HasNativeSight(Run.Player.Get());
}
bool CreateWall()
{
    auto* Actor = SpawnAt<AActor>(ArenaOrigin);
    if (!IsValid(Actor)) return false;
    Run.Blocker = Actor;
    auto* Component = NewObject<UBoxComponent>(Actor, FName(TEXT("PedroAwarenessValidationBox")),RF_Transient);
    if (!IsValid(Component)) return false;
    Run.Box = Component;
    Actor->SetRootComponent(Component);
    Actor->AddInstanceComponent(Component);
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetCollisionObjectType(ECC_WorldStatic);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCollisionResponseToAllChannels(ECR_Ignore);
    Component->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->RegisterComponent();
    return true;
}
bool WallOff()
{
    if (!Run.Box.IsValid()) return false;
    Run.Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    return true;
}
bool WallAt(const FVector& At, const FVector& Extent)
{
    if (!WallOff()) return false;
    Run.Box->SetBoxExtent(Extent);
    if (!Place(Run.Blocker.Get(),At)) return false;
    Run.Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    return true;
}
bool BroadWall()
{
    return WallAt(ArenaOrigin+FVector(300,0,64), FVector(35,1200,500));
}
bool PlayerRayHitsWall()
{
    if (!Run.Observer.IsValid() || !Run.Player.IsValid() || !Run.Box.IsValid()) return false;
    FVector Eye; FRotator Facing; FHitResult Hit;
    Run.Observer->GetActorEyesViewPoint(Eye,Facing);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PedroAwarenessPlayerWallProbe),false);
    Params.AddIgnoredActor(Run.Observer.Get());
    return Run.World->LineTraceSingleByChannel(Hit,Eye,Run.Player->GetActorLocation(),ECC_Visibility,Params) &&
        Hit.GetActor()==Run.Blocker.Get() && Hit.GetComponent()==Run.Box.Get() && !Hit.bStartPenetrating && Hit.Time>0.0f && Hit.Time<1.0f;
}
bool AimMatchesEye()
{
    if (!Run.Controller.IsValid() || !Run.Observer.IsValid()) return false;
    FVector Eye, Listener, Direction; FRotator Facing;
    Run.Observer->GetActorEyesViewPoint(Eye,Facing);
    Run.Controller->GetAIPerceptionComponent()->GetLocationAndDirection(Listener,Direction);
    return Eye.Equals(Listener,0.1) && Facing.Vector().Equals(Direction,0.01);
}
void Add(const TCHAR* Case, const TCHAR* Name, FCheck Setup, FCheck Check, double Hold = 0.25, FCheck Guard = {})
{
    Run.Steps.Add(FStep{Case,Name,MoveTemp(Setup),MoveTemp(Check),MoveTemp(Guard),Hold});
}
bool Noop() { return true; }

// A real source event is captured at occurrence; this driver never inserts evidence.
UPedroObservationSourceComponent* Source(APedroPrototypeInteractableActor* Prop)
{
    return IsValid(Prop) ? Prop->FindComponentByClass<UPedroObservationSourceComponent>() : nullptr;
}
ETrace TraceProp(APedroPrototypeInteractableActor* Prop)
{
    auto* S = Source(Prop);
    if (!S || !Run.Director.IsValid()) return ETrace::InvalidEvent;
    FPedroBehaviourEvent Probe;
    Probe.EventId = FGuid::NewGuid(); Probe.Kind = EPedroEvidenceKind::InspectedOptionalObject;
    Probe.SubjectActor = Prop; Probe.SubjectId = S->SubjectId; Probe.OpportunityId = S->OpportunityId;
    Probe.EventLocation = Prop->GetActorLocation();
    return PedroDirectObservation::Evaluate(Run.World.Get(), Run.Observer.Get(), Probe,
        Run.Director->DirectObservationMaxDistance, Run.Director->DirectObservationViewHalfAngleDegrees);
}
bool WallForProp(APedroPrototypeInteractableActor* Prop, double Extent)
{
    if (!IsValid(Prop)) return false;
    FVector Eye; FRotator Facing;
    Run.Observer->GetActorEyesViewPoint(Eye,Facing);
    const FVector Segment = Prop->GetActorLocation()-Eye;
    if (Segment.Size() < Extent*4) return false;
    return WallAt(Eye+Segment*0.5,FVector(Extent));
}
bool InspectExpected(APedroPrototypeInteractableActor* Prop, ETrace Expected)
{
    const bool bVisible=Expected==ETrace::Visible;
    auto* S = Source(Prop);
    if (!IsValid(S) || !S->bEvidenceEnabled || !Run.Player.IsValid() || !Run.Player->IsPlayerControlled() ||
        TraceProp(Prop) != Expected) return false;
    if (Expected==ETrace::Occluded)
    {
        FVector Eye; FRotator Facing; FHitResult Hit;
        Run.Observer->GetActorEyesViewPoint(Eye,Facing);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(PedroAwarenessFixture),false);
        Params.AddIgnoredActor(Run.Observer.Get());
        if (!Run.World->LineTraceSingleByChannel(Hit,Eye,Prop->GetActorLocation(),ECC_Visibility,Params) ||
            Hit.GetActor()!=Run.Blocker.Get() || Hit.GetComponent()!=Run.Box.Get() || Hit.bStartPenetrating) return false;
    }
    const int32 Before = Prop->GetSuccessfulInspectionCount();
    int32 Count = 0; FPedroBehaviourEvent Event;
    const auto Handle = S->OnWorldEventProduced.AddLambda([&](const FPedroBehaviourEvent& E){++Count; Event=E;});
    IInteractable::Execute_Interact(Prop,Run.Player.Get());
    S->OnWorldEventProduced.Remove(Handle);
    if (Count != 1 || Prop->GetSuccessfulInspectionCount()!=Before+1 || !Event.EventId.IsValid() ||
        Run.Emitted.Contains(Event.EventId) || Event.SubjectActor.Get()!=Prop || Event.SubjectId!=S->SubjectId ||
        Event.OpportunityId!=S->OpportunityId || !Event.EventLocation.Equals(Prop->GetActorLocation(),0.01) ||
        Event.Kind!=(Before==0 ? EPedroEvidenceKind::InspectedOptionalObject : EPedroEvidenceKind::RepeatedInteraction)) return false;
    Run.Emitted.Add(Event.EventId);
    if (bVisible) Run.ExpectedAcquired.Add(Event.EventId);
    if(Run.bContext)
    {UE_LOG(LogTemp,Display,TEXT("PEDRO_CONTEXT_EVENT id=%s subject=%s opportunity=%s trace=%s location=%s time=%.6f director=%s"),*Event.EventId.ToString(EGuidFormats::Digits),*Event.SubjectId.ToString(),*Event.OpportunityId.ToString(),PedroDirectObservation::StatusName(Expected),*VectorString(Event.EventLocation),Event.WorldTimeSeconds,*GetNameSafe(Run.Director.Get()));}
    UE_LOG(LogTemp, Display,TEXT("PEDRO_AWARENESS_TEST status=EVENT id=%s subject=%s opportunity=%s expected=%s inspection=%d"),
        *Event.EventId.ToString(EGuidFormats::Digits),*Event.SubjectId.ToString(),*Event.OpportunityId.ToString(),
        PedroDirectObservation::StatusName(Expected),Before+1);
    return true;
}
bool Inspect(APedroPrototypeInteractableActor* Prop, bool bVisible)
{
    return InspectExpected(Prop,bVisible?ETrace::Visible:ETrace::Occluded);
}
bool EvidenceMatches(int32 Expected, int32 Sampled, bool bFinal = false)
{
    if (!Run.Director.IsValid() || !Progress()) return false;
    const auto& S = Run.Director->GetObservationState();
    if (S.EvidenceHistory.Num()!=Expected || Run.ExpectedAcquired.Num()!=Expected || SampledCount()!=Sampled ||
        Finalized()!=bFinal || Progress()->GetResolvedEncounterCount()!=Run.InitialProgress+(bFinal?1:0)) return false;
    for (int32 I=0;I<Expected;++I)
        if (S.EvidenceHistory[I].Event.EventId!=Run.ExpectedAcquired[I] ||
            S.EvidenceHistory[I].KnowledgeSource!=EPedroKnowledgeSource::DirectObservation) return false;
    return true;
}
APedroPrototypeInteractableActor* SpawnProp(const TCHAR* Name, const FVector& Offset)
{
    auto* Prop = SpawnAt<APedroPrototypeInteractableActor>(ArenaOrigin+Offset);
    auto* S = Source(Prop);
    if (!IsValid(S)) return nullptr;
    S->bEvidenceEnabled=true; S->SubjectId=FName(Name); S->OpportunityId=FName(Name);
    return Run.Director->RegisterObservationSource(S) ? Prop : nullptr;
}
bool BuildEvidenceRig()
{
    if (Run.Director.IsValid()) Run.Director->Destroy();
    if (Run.A.IsValid()) Run.A->Destroy();
    if (Run.B.IsValid()) Run.B->Destroy();
    Run.Emitted.Reset(); Run.ExpectedAcquired.Reset();
    UE_LOG(LogTemp, Display,TEXT("PEDRO_AWARENESS_TEST status=RESET_EVIDENCE"));
    FTransform At(FRotator::ZeroRotator,ArenaOrigin+FVector(0,0,-500));
    auto* D = Run.World->SpawnActorDeferred<APedroEncounterDirector>(APedroEncounterDirector::StaticClass(),At,
        nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(D)) return false;
    Run.Director=D;
    D->SetFlags(RF_Transient);
    D->bUseDirectObservationAcquisition=true; D->bUsePrototypeAssumedAcquisition=false;
    D->DirectObservationActor=Run.Observer.Get(); D->CompletionPolicy=EPedroCompletionPolicy::EvidenceDriven;
    D->DirectObservationMaxDistance=1500; D->DirectObservationViewHalfAngleDegrees=75;
    for (const TCHAR* Name : {TEXT("AwarenessPropA"),TEXT("AwarenessPropB"),TEXT("AwarenessUnusedC"),TEXT("AwarenessUnusedD")})
    { FPedroOpportunityDefinition Def; Def.OpportunityId=FName(Name); D->OpportunityDefinitions.Add(Def); }
    D->FinishSpawning(At);
    Run.A=SpawnProp(TEXT("AwarenessPropA"),FVector(600,300,64));
    Run.B=SpawnProp(TEXT("AwarenessPropB"),FVector(800,-300,64));
    return Run.A.IsValid() && Run.B.IsValid() && D->GetObservationState().Opportunities.GetCoreCount()==4;
}

void MakeWorldSteps()
{
    Add(TEXT("W01"),TEXT("InitiallyVisible"),[]{return MovePlayer(FVector(600,0,0)) && FreshController();},Visible,0.35,PoseUnchanged);
    Add(TEXT("W02"),TEXT("InitiallyBehind"),[]{return TurnObserver(180) && FreshController();},NoSighting,0.8,EmptyAwareness);
    Add(TEXT("W03"),TEXT("InitiallyOutsideRange"),[]{return TurnObserver(0) && MovePlayer(FVector(2200,0,0)) && FreshController();},NoSighting,0.8,EmptyAwareness);
    Add(TEXT("W04"),TEXT("InitiallyBehindCover"),[]{return BroadWall() && MovePlayer(FVector(600,0,0)) && FreshController() && PlayerRayHitsWall();},[]{return NoSighting() && PlayerRayHitsWall();},0.8,EmptyAwareness);
    Add(TEXT(""),TEXT("ClearAfterInitialCover"),WallOff,Visible);
    Add(TEXT("W05"),TEXT("VisiblePlayerMovement"),[]{return MovePlayer(FVector(750,100,0));},Visible,0.5,PoseUnchanged);
    Add(TEXT("W06"),TEXT("ObserverTurnsAway"),[]{return FreezeMemory() && TurnObserver(180);},Lost,0.35);
    Add(TEXT("W07"),TEXT("ObserverTurnsBack"),[]{return TurnObserver(0);},Visible);
    Add(TEXT(""),TEXT("LostBeforeEnterRange"),[]{return FreezeMemory() && MovePlayer(FVector(2200,0,0));},Lost,0.4,MemoryUnchanged);
    Add(TEXT("W08"),TEXT("PlayerEntersRange"),[]{return MovePlayer(FVector(1400,0,0));},Visible);
    Add(TEXT(""),TEXT("RetainInHysteresisMargin"),[]{return MovePlayer(FVector(1650,0,0));},Visible,0.5);
    Add(TEXT(""),TEXT("LoseBeyondHysteresis"),[]{return FreezeMemory() && MovePlayer(FVector(2050,0,0));},Lost,0.5,MemoryUnchanged);
    Add(TEXT(""),TEXT("NoReacquireInMargin"),[]{return MovePlayer(FVector(1650,0,0));},Lost,0.8,MemoryUnchanged);
    Add(TEXT("W09"),TEXT("ReacquireInsideRadius"),[]{return MovePlayer(FVector(600,0,0));},Visible);
    Add(TEXT("W10"),TEXT("WallBlocksSight"),[]{return FreezeMemory() && BroadWall() && MovePlayer(FVector(650,-250,0)) && PlayerRayHitsWall();},[]{return Lost() && PlayerRayHitsWall();},0.4,MemoryUnchanged);
    Add(TEXT("W11"),TEXT("OccludedPlayerMovement"),[]{return MovePlayer(FVector(900,400,0)) && PlayerRayHitsWall();},[]{return Lost() && PlayerRayHitsWall();},1.0,MemoryUnchanged);
    Add(TEXT("W12"),TEXT("WallRemoved"),WallOff,Visible,0.5);
    Add(TEXT(""),TEXT("EyeOnlyTurnsAway"),[]{if (!FreezeMemory()) return false; Run.Observer->GetObserverEye()->SetRelativeRotation(FRotator(0,180,0)); return true;},[]{return Lost() && AimMatchesEye();},0.4,PoseUnchanged);
    Add(TEXT("W13"),TEXT("EyeOffsetAndFacingRestored"),[]{Run.Observer->GetObserverEye()->SetRelativeLocationAndRotation(FVector(0,80,84),FRotator::ZeroRotator);return true;},[]{return Visible() && AimMatchesEye();},0.4,PoseUnchanged);
    Add(TEXT("W14"),TEXT("DistractorIgnored"),[]{Run.Observer->GetObserverEye()->SetRelativeLocation(FVector(0,0,64)); Run.Distractor=SpawnObserver(ArenaOrigin+FVector(500,200,0),FRotator::ZeroRotator);return Run.Distractor.IsValid() && MovePlayer(FVector(-600,0,0)) && FreshController();},[]{return NoSighting() && Run.Controller->HasNativeSight(Run.Distractor.Get());},0.8,EmptyAwareness);
    Add(TEXT(""),TEXT("VisibleBeforeDestroy"),[]{return MovePlayer(FVector(600,0,0));},Visible);
    Add(TEXT("W15"),TEXT("PlayerDestroyed"),[]{auto* P=Run.Player.Get();return IsValid(P) && P->Destroy();},[]{return !Run.Player.IsValid() && EmptyAwareness() && Run.Controller->IsAwarenessRunning();},0.5);
    Add(TEXT(""),TEXT("ReplacementInitiallyHidden"),[]{if(!Run.PC.IsValid())return false;auto* P=SpawnObserver(ArenaOrigin+FVector(-600,0,0),FRotator::ZeroRotator);if(!IsValid(P))return false;Run.PC->Possess(P);Run.Player=P;return P->IsPlayerControlled() && Freeze(P);},NoSighting,0.8,EmptyAwareness);
    Add(TEXT("W16"),TEXT("ReplacementNeedsOwnSighting"),[]{return MovePlayer(FVector(600,0,0));},Visible);
    Add(TEXT("W17"),TEXT("UnpossessClears"),[]{Run.Controller->UnPossess();return true;},[]{return EmptyAwareness() && !Run.Controller->IsAwarenessRunning() && !Run.Controller->GetPawn() && !Run.Controller->GetAIPerceptionComponent()->OnTargetPerceptionUpdated.IsBound();},0.7);
    Add(TEXT(""),TEXT("RepossessWhileHidden"),[]{if(!MovePlayer(FVector(-600,0,0)))return false;Run.Controller->Possess(Run.Observer.Get());return Run.Controller->GetPawn()==Run.Observer.Get() && Freeze(Run.Observer.Get());},NoSighting,0.8,EmptyAwareness);
    Add(TEXT("W18"),TEXT("RepossessedFreshSighting"),[]{return MovePlayer(FVector(600,0,0));},Visible);
    Add(TEXT("W19"),TEXT("IndependentObservers"),[]{Run.SecondObserver=SpawnObserver(ArenaOrigin+FVector(0,-500,0),FRotator(0,180,0)); Run.SecondController=Attach(Run.SecondObserver.Get());return Run.SecondController.IsValid();},[]{return Visible() && Run.SecondController.IsValid() && Run.SecondController->IsAwarenessRunning() && !Run.SecondController->HasNativeSight(Run.Player.Get()) && Run.SecondController->GetAwarenessState().Status==EStatus::Unaware && !Run.SecondController->GetAwarenessState().bHasLastKnownPlayerLocation;},0.8);
    Add(TEXT("W20"),TEXT("ControllerDestroyed"),[]{Run.DestroyedTimer=Run.Controller->GetAwarenessRefreshTimerHandle(); if(!Run.DestroyedTimer.IsValid())return false;return Run.Controller->Destroy();},[]{return !Run.Controller.IsValid() && !IsValid(Run.Observer->GetController()) && !Run.World->GetTimerManager().TimerExists(Run.DestroyedTimer) && Run.SecondController.IsValid() && Run.SecondController->IsAwarenessRunning();},0.8);
    Add(TEXT("W21"),TEXT("AwarenessOnlyNoEvidence"),[]{if(Run.SecondController.IsValid())DestroyController(Run.SecondController);if(Run.SecondObserver.IsValid())Run.SecondObserver->Destroy();if(Run.Distractor.IsValid())Run.Distractor->Destroy();return FreshController() && BuildEvidenceRig();},[]{return Visible() && EvidenceMatches(0,0);},0.6);
    Add(TEXT("W22"),TEXT("VisiblePlayerOccludedEvent"),[]{return WallForProp(Run.A.Get(),35) && Inspect(Run.A.Get(),false);},[]{return Visible() && EvidenceMatches(0,0);},0.5);
    Add(TEXT(""),TEXT("PlayerHiddenEventClear"),[]{return WallOff() && FreezeMemory() && MovePlayer(FVector(-600,0,0));},Lost,0.4);
    Add(TEXT("W23"),TEXT("UnseenPlayerVisibleEvent"),[]{return Inspect(Run.A.Get(),true);},[]{return Lost() && EvidenceMatches(1,1);},0.5);
    // Fresh evidence rig for the six-step demonstration; the perception/controller is real throughout.
    Add(TEXT(""),TEXT("DemoDetect"),[]{return BuildEvidenceRig() && MovePlayer(FVector(600,0,0));},[]{return Visible() && EvidenceMatches(0,0);});
    Add(TEXT(""),TEXT("DemoVisibleAction"),[]{return Inspect(Run.A.Get(),true);},[]{return Visible() && EvidenceMatches(1,1);});
    Add(TEXT(""),TEXT("DemoLoseSight"),[]{return FreezeMemory() && BroadWall();},[]{return Lost() && EvidenceMatches(1,1);});
    Add(TEXT(""),TEXT("DemoHiddenAction"),[]{return MovePlayer(FVector(900,400,0)) && Inspect(Run.B.Get(),false);},[]{return Lost() && EvidenceMatches(1,1);},0.8,MemoryUnchanged);
    Add(TEXT("W24"),TEXT("DemoReacquireNoRetroactive"),WallOff,[]{return Visible() && EvidenceMatches(1,1);},0.8);
    Add(TEXT("W25"),TEXT("DemoNewActionAcquired"),[]{return Inspect(Run.B.Get(),true);},[]{return Visible() && EvidenceMatches(2,2);},0.5);
}

template<class T> T* ReflectedObject(UObject* Owner, const TCHAR* Field)
{
    if (!IsValid(Owner)) return nullptr;
    const auto* P=FindFProperty<FObjectPropertyBase>(Owner->GetClass(),FName(Field));
    return P ? Cast<T>(P->GetObjectPropertyValue_InContainer(Owner)) : nullptr;
}
bool AllOutcomesHidden()
{
    for (AActor* Actor : {Run.Gentle.Get(),Run.Seeking.Get(),Run.Disruptive.Get()})
        if (!IsValid(Actor) || !Actor->IsHidden() || Actor->GetActorEnableCollision()) return false;
    return true;
}
bool SavedMapConfig()
{
    int32 Directors=0;
    for (TActorIterator<APedroEncounterDirector> It(Run.World.Get()); It;++It) {Run.Director=*It;++Directors;}
    auto* D=Run.Director.Get();
    if (Directors!=1 || !IsValid(D)) return false;
    Run.Observer=Cast<APedroObserverCharacter>(D->DirectObservationActor.Get());
    Run.Definition=ReflectedObject<UEncounterDefinition>(D,TEXT("EncounterDefinition"));
    Run.Gentle=ReflectedObject<AActor>(D,TEXT("GentleOutcomeActor"));
    Run.Seeking=ReflectedObject<AActor>(D,TEXT("SeekingOutcomeActor"));
    Run.Disruptive=ReflectedObject<AActor>(D,TEXT("DisruptiveOutcomeActor"));
    if (!Run.Observer.IsValid() || !Run.Observer->GetClass()->GetName().Contains(TEXT("BP_PedroObserver_Validation")) ||
        Cast<APedroObserverAIController>(Run.Observer->GetController()) || !Run.Definition.IsValid() ||
        Run.Definition->GetEncounterId()!=FName(TEXT("PedroObserverActorTest")) ||
        Run.Definition->GetEncounterMap().ToSoftObjectPath().GetLongPackageName()!=Map ||
        !D->bUseDirectObservationAcquisition || D->bUsePrototypeAssumedAcquisition ||
        D->CompletionPolicy!=EPedroCompletionPolicy::EvidenceDriven || !D->bEnableCompletionGateShadowEvaluation ||
        !FMath::IsNearlyEqual(D->CoreSampleFraction,0.75f) || !FMath::IsNearlyEqual(D->DirectObservationMaxDistance,5000.0f) ||
        !FMath::IsNearlyEqual(D->DirectObservationViewHalfAngleDegrees,75.0f) ||
        !AllOutcomesHidden() || Run.Gentle==Run.Seeking || Run.Gentle==Run.Disruptive || Run.Seeking==Run.Disruptive ||
        !Progress() || Progress()->IsEncounterResolved(Run.Definition.Get())) return false;
    int32 ACount=0,BCount=0,CCount=0;
    for (TActorIterator<APedroPrototypeInteractableActor> It(Run.World.Get());It;++It)
    {
        const auto* S=Source(*It);
        if (!IsValid(S) || !S->bEvidenceEnabled) continue;
        if (S->OpportunityId==FName(TEXT("Inspect_Statue"))) {Run.A=*It;++ACount;}
        if (S->OpportunityId==FName(TEXT("Inspect_Radio"))) {Run.B=*It;++BCount;}
        if (S->OpportunityId==FName(TEXT("Inspect_Lantern"))) {Run.C=*It;++CCount;}
    }
    if (ACount!=1 || BCount!=1 || CCount!=1 || !EvidenceMatches(0,0) ||
        D->GetObservationState().Opportunities.GetCoreCount()!=4 || D->GetObservationState().Opportunities.GetBonusCount()!=1) return false;
    for (auto* Prop : {Run.A.Get(),Run.B.Get(),Run.C.Get()})
    {
        const auto* S=Source(Prop);
        const auto* Entry=D->GetObservationState().Opportunities.Find(S->OpportunityId);
        if (!Entry || Entry->Tier!=EPedroOpportunityTier::Core || Entry->IsSampled() || Prop->GetSuccessfulInspectionCount()!=0) return false;
    }
    return true;
}
// Move only the runtime pawn. Geometry is independently checked after placement.
// Preserve the saved observer's pose whenever possible; controlled rotation is
// allowed in the integration fixture when needed to aim at a source.
bool PlacePlayerVisible(APedroPrototypeInteractableActor* RequiredProp)
{
    if (!Run.Player.IsValid() || !Run.Observer.IsValid()) return false;
    FVector Eye; FRotator Facing;
    Run.Observer->GetActorEyesViewPoint(Eye,Facing);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PedroAwarenessIntegrationPlayer),false);
    Params.AddIgnoredActor(Run.Observer.Get()); Params.AddIgnoredActor(Run.Player.Get());
    for (double Yaw : {30.0,-30.0,50.0,-50.0,0.0})
    {
        const FVector Direction=Facing.Vector().RotateAngleAxis(Yaw,FVector::UpVector);
        for (double Distance : {200.0,300.0,450.0,600.0})
        {
            const FVector At=Eye+Direction*Distance;
            if (PedroDirectObservation::CheckGeometry(Eye,Facing.Vector(),At,1500,75)!=ETrace::Visible) continue;
            FHitResult Hit;
            if (Run.World->LineTraceSingleByChannel(Hit,Eye,At,ECC_Visibility,Params)) continue;
            if (Place(Run.Player.Get(),At) && TraceProp(RequiredProp)==ETrace::Visible) return true;
        }
    }
    return false;
}
bool AimAtProp(APedroPrototypeInteractableActor* Prop)
{
    if (!IsValid(Prop) || !Run.Observer.IsValid()) return false;
    if (TraceProp(Prop)==ETrace::Visible) return true;
    FVector Eye; FRotator Facing;
    Run.Observer->GetActorEyesViewPoint(Eye,Facing);
    if (!TurnObserver((Prop->GetActorLocation()-Eye).Rotation().Yaw)) return false;
    return TraceProp(Prop)==ETrace::Visible;
}
bool FinalPresentation()
{
    return State().bHasLastKnownPlayerLocation && EvidenceMatches(3,3,true) && Progress()->IsEncounterResolved(Run.Definition.Get()) &&
        Run.Director->GetFinalClassification().Interpretation==EPedroInterpretation::Seeking &&
        Run.Seeking.IsValid() && !Run.Seeking->IsHidden() && Run.Seeking->GetActorEnableCollision() &&
        Run.Gentle.IsValid() && Run.Gentle->IsHidden() && !Run.Gentle->GetActorEnableCollision() &&
        Run.Disruptive.IsValid() && Run.Disruptive->IsHidden() && !Run.Disruptive->GetActorEnableCollision();
}
void MakeIntegrationSteps()
{
    Add(TEXT("I01"),TEXT("SavedConfiguration"),[]{return SavedMapConfig() && Freeze(Run.Player.Get()) && Freeze(Run.Observer.Get());},[]{return EvidenceMatches(0,0) && AllOutcomesHidden() && !Run.Controller.IsValid();});
    Add(TEXT("I02"),TEXT("RuntimeAwarenessOptIn"),[]{if (!Freeze(Run.Player.Get()) || !Freeze(Run.Observer.Get())) return false;Run.ObserverLocation=Run.Observer->GetActorLocation();Run.ObserverRotation=Run.Observer->GetActorRotation();return AimAtProp(Run.A.Get()) && PlacePlayerVisible(Run.A.Get()) && FreshController() && CreateWall();},[]{return Visible() && AimMatchesEye() && EvidenceMatches(0,0);},0.5,PoseUnchanged);
    Add(TEXT("I03"),TEXT("RealStatueInspection"),[]{return Inspect(Run.A.Get(),true);},[]{return Visible() && EvidenceMatches(1,1) && AllOutcomesHidden();});
    Add(TEXT(""),TEXT("PrepareRadio"),[]{return AimAtProp(Run.B.Get()) && PlacePlayerVisible(Run.B.Get());},Visible);
    Add(TEXT("I04"),TEXT("RealRadioOccluded"),[]{return WallForProp(Run.B.Get(),30) && Inspect(Run.B.Get(),false);},[]{return State().bHasLastKnownPlayerLocation && EvidenceMatches(1,1) && AllOutcomesHidden();},0.5);
    Add(TEXT("I05"),TEXT("RestoreSightNoRetroactive"),[]{return WallOff() && PlacePlayerVisible(Run.B.Get());},[]{return Visible() && EvidenceMatches(1,1) && AllOutcomesHidden();},0.8);
    Add(TEXT(""),TEXT("RadioVisibleNewAction"),[]{return Inspect(Run.B.Get(),true);},[]{return Visible() && EvidenceMatches(2,2) && AllOutcomesHidden();});
    Add(TEXT(""),TEXT("PrepareLantern"),[]{return AimAtProp(Run.C.Get()) && PlacePlayerVisible(Run.C.Get());},Visible);
    Add(TEXT("I06"),TEXT("LanternSeekingCompletion"),[]{return Inspect(Run.C.Get(),true);},FinalPresentation,0.8);
}

// New suites reuse the existing arena, source, Director and lifecycle helpers.
using EAttention = EPedroAttentionMode;
const TCHAR* AttentionName(EAttention Mode)
{
    switch (Mode) { case EAttention::Disabled:return TEXT("Disabled"); case EAttention::Idle:return TEXT("Idle");
        case EAttention::TrackObserved:return TEXT("TrackObserved"); case EAttention::HoldLastKnown:return TEXT("HoldLastKnown"); }
    return TEXT("Invalid");
}
const FPedroAttentionDecision& Attention()
{
    static const FPedroAttentionDecision Empty;
    return Run.Controller.IsValid() ? Run.Controller->GetAttentionDecision() : Empty;
}
bool IsMode(EAttention Mode) { return Attention().Mode==Mode; }
bool EnableAttention() { if(!Run.Controller.IsValid())return false;Run.Controller->SetAttentionEnabled(true);return Run.Controller->IsAttentionRunning(); }
bool DisableAttention() { if(!Run.Controller.IsValid())return false;Run.Controller->SetAttentionEnabled(false);return !Run.Controller->IsAttentionRunning(); }
bool MoveBearing(double Yaw,double Distance=700)
{
    return MovePlayer(FRotator(0,Yaw,0).Vector()*Distance);
}
double BodyYaw() { return Run.Observer.IsValid()?Run.Observer->GetActorRotation().Yaw:0; }
double YawError(double Goal) { return FMath::Abs(FMath::FindDeltaAngleDegrees(BodyYaw(),Goal)); }
bool EyeAligned(APedroObserverCharacter* Observer,const FVector& At)
{
    if(!IsValid(Observer))return false;
    FVector Eye; FRotator Rotation;Observer->GetActorEyesViewPoint(Eye,Rotation);
    const FVector Offset=At-Eye;
    if(Offset.Size2D()<10 || Rotation.Vector().Size2D()<0.001)return false;
    return FMath::Abs(FMath::FindDeltaAngleDegrees(Rotation.Yaw,Offset.Rotation().Yaw))<=2.6;
}
bool Tracking()
{
    return Visible() && IsMode(EAttention::TrackObserved) && Attention().LookPoint.IsSet() &&
        Attention().LookPoint.GetValue().Equals(State().LastKnownPlayerLocation,0.01);
}
bool Aligned() { return Tracking() && EyeAligned(Run.Observer.Get(),State().LastKnownPlayerLocation) && AimMatchesEye(); }
bool IdleAtAnchor() { return IsMode(EAttention::Idle) && !Attention().LookPoint.IsSet() && YawError(Attention().IdleYaw)<=2.1; }
bool Holding() { return State().Status==EStatus::LostSight && MemoryUnchanged() && IsMode(EAttention::HoldLastKnown) &&
    Attention().LookPoint.IsSet() && Attention().LookPoint.GetValue()==Run.Frozen.LastKnownPlayerLocation && PlayerRayHitsWall(); }
bool FreshAttention(double Body,double PlayerBearing,bool bEnable=true)
{
    if(Run.Controller.IsValid() && !DisableAttention()) return false;
    if(!WallOff() || !TurnObserver(Body) || !MoveBearing(PlayerBearing) || !FreshController()) return false;
    return !bEnable || EnableAttention();
}
struct FAttentionPose
{
    TWeakObjectPtr<APedroObserverCharacter> Actor;
    FVector Position=FVector::ZeroVector;
    FRotator Rotation=FRotator::ZeroRotator;
    FTransform Eye;
    double PreviousYaw=0, PreviousTime=0, Total=0, Travel=0;
    int32 Frames=0;
    void Capture(APedroObserverCharacter* Observer,double Now)
    {
        *this=FAttentionPose{};Actor=Observer;
        if(!IsValid(Observer))return;
        Position=Observer->GetActorLocation();Rotation=Observer->GetActorRotation();
        Eye=Observer->GetObserverEye()->GetRelativeTransform();PreviousYaw=Rotation.Yaw;PreviousTime=Now;
    }
    bool Check(APedroObserverAIController* Controller,double Now,double& Dt,double& Delta)
    {
        Dt=0;Delta=0;if(!Actor.IsValid())return false;
        const auto R=Actor->GetActorRotation();
        Dt=Now-PreviousTime;Delta=FMath::FindDeltaAngleDegrees(PreviousYaw,R.Yaw);
        const auto Mode=IsValid(Controller)?Controller->GetAttentionDecision().Mode:EAttention::Disabled;
        const double Speed=Mode==EAttention::Disabled?0:(Mode==EAttention::Idle?45:90);
        if(!FMath::IsFinite(Dt)||Dt<0||!FMath::IsFinite(Delta)||
            FMath::Abs(Delta)>Speed*FMath::Min(Dt,0.1)+0.12 ||
            !Actor->GetActorLocation().Equals(Position,0.1)||
            FMath::Abs(FMath::FindDeltaAngleDegrees(Rotation.Pitch,R.Pitch))>0.1||
            FMath::Abs(FMath::FindDeltaAngleDegrees(Rotation.Roll,R.Roll))>0.1||
            !Actor->GetObserverEye()->GetRelativeTransform().Equals(Eye,0.001)) return false;
        PreviousYaw=R.Yaw;PreviousTime=Now;Total+=Delta;Travel+=FMath::Abs(Delta);++Frames;
        return true;
    }
};
FAttentionPose AttentionPose, SecondPose;
// Independent anchors retained only in the test fixture, not the controller.
double AnchorExpected=0, PausedYaw=0;
FString OriginalPlayerName;
int32 GuardEvidence=0,GuardSampled=0,GuardProgress=0;
bool GuardFinal=false;
FString GuardEmitted,GuardAcquired;
bool bEarlyReacquired=false;
const TCHAR* AttentionSuite() { return Run.bIntegration?TEXT("attention-integration"):TEXT("attention"); }
void LogAttentionSample(const TCHAR* Kind,const FStep& Step,double Dt=0,double Delta=0,double OtherDt=0,double OtherDelta=0)
{
    const auto& K=State();const auto& D=Attention();
    const auto* O=Run.Observer.Get();const auto* P=Run.Player.Get();
    FVector Eye=FVector::ZeroVector;FRotator Facing=FRotator::ZeroRotator;
    if(IsValid(O))O->GetActorEyesViewPoint(Eye,Facing);
    const FRotator Body=IsValid(O)?O->GetActorRotation():FRotator::ZeroRotator;
    const FTransform Relative=IsValid(O)?O->GetObserverEye()->GetRelativeTransform():FTransform::Identity;
    const auto* Other=Run.SecondController.Get();const auto* OtherO=Run.SecondObserver.Get();
    FVector OtherEye=FVector::ZeroVector;FRotator OtherFacing=FRotator::ZeroRotator;
    if(IsValid(OtherO))OtherO->GetActorEyesViewPoint(OtherEye,OtherFacing);
    UE_LOG(LogTemp,Display,TEXT("PEDRO_ATTENTION_TEST status=%s case=%s step=%s mode=%s awareness=%s has=%s look=%s known=%s actual=%s time=%.6f now=%.6f target=%s player=%s running=%s attention=%s body=%.6f,%.6f,%.6f root=%s eye=%s forward=%s rel=%s relRot=%s idle=%.6f error=%.6f request=%.6f dt=%.6f delta=%.6f total=%.6f travel=%.6f frames=%d evidence=%d sampled=%d emitted=%s acquired=%s final=%s progress=%d other=%s otherYaw=%.6f otherDt=%.6f otherDelta=%.6f otherTotal=%.6f otherAwareness=%s otherLook=%s otherEye=%s otherForward=%s timerActive=%s"),
        Kind,Step.Case.IsEmpty()?TEXT("-"):*Step.Case,*Step.Name,AttentionName(D.Mode),StatusName(K.Status),Yes(K.bHasLastKnownPlayerLocation),
        D.LookPoint.IsSet()?*VectorString(D.LookPoint.GetValue()):TEXT("-"),*VectorString(K.LastKnownPlayerLocation),
        *VectorString(IsValid(P)?P->GetActorLocation():FVector::ZeroVector),K.LastSuccessfulSightingTime,Run.World.IsValid()?Run.World->GetTimeSeconds():0.0,
        *GetNameSafe(K.ObservedPlayer.Get()),*GetNameSafe(P),Yes(Run.Controller.IsValid()&&Run.Controller->IsAwarenessRunning()),
        Yes(Run.Controller.IsValid()&&Run.Controller->IsAttentionRunning()),Body.Pitch,Body.Yaw,Body.Roll,
        *VectorString(IsValid(O)?O->GetActorLocation():FVector::ZeroVector),*VectorString(Eye),*VectorString(Facing.Vector()),
        *VectorString(Relative.GetLocation()),*VectorString(FVector(Relative.Rotator().Pitch,Relative.Rotator().Yaw,Relative.Rotator().Roll)),
        D.IdleYaw,D.AngularError,D.RequestedYawStep,Dt,Delta,AttentionPose.Total,AttentionPose.Travel,AttentionPose.Frames,
        EvidenceCount(),SampledCount(),*Ids(Run.Emitted),*EvidenceIds(),Yes(Finalized()),Progress()?Progress()->GetResolvedEncounterCount():-1,
        IsValid(Other)?AttentionName(Other->GetAttentionDecision().Mode):TEXT("None"),IsValid(OtherO)?OtherO->GetActorRotation().Yaw:0,
        OtherDt,OtherDelta,SecondPose.Total,
        IsValid(Other)?StatusName(Other->GetAwarenessState().Status):TEXT("None"),
        IsValid(Other)&&Other->GetAttentionDecision().LookPoint.IsSet()?*VectorString(Other->GetAttentionDecision().LookPoint.GetValue()):TEXT("-"),
        *VectorString(OtherEye),*VectorString(OtherFacing.Vector()),
        Yes(Run.World.IsValid()&&Run.DestroyedTimer.IsValid()&&Run.World->GetTimerManager().TimerExists(Run.DestroyedTimer)));
}
void CaptureAttentionPose()
{
    const double Now=Run.World->GetTimeSeconds();
    AttentionPose.Capture(Run.Observer.Get(),Now);SecondPose.Capture(Run.SecondObserver.Get(),Now);
    GuardEvidence=EvidenceCount();GuardSampled=SampledCount();GuardProgress=Progress()?Progress()->GetResolvedEncounterCount():-1;
    GuardFinal=Finalized();GuardEmitted=Ids(Run.Emitted);GuardAcquired=EvidenceIds();
}
bool GuardAttention(const FStep& Step,bool bLog=true)
{
    double Dt=0,Delta=0,OtherDt=0,OtherDelta=0;const double Now=Run.World->GetTimeSeconds();
    const auto Reject=[&Step](const TCHAR* Detail)
    {
        UE_LOG(LogTemp,Error,TEXT("Pedro attention invariant: step=%s detail=%s"),*Step.Name,Detail);
        return false;
    };
    if(!AttentionPose.Check(Run.Controller.Get(),Now,Dt,Delta))return Reject(TEXT("ObserverPoseOrRate"));
    if(Run.SecondObserver.IsValid()&&!SecondPose.Check(Run.SecondController.Get(),Now,OtherDt,OtherDelta))return Reject(TEXT("SecondObserverPoseOrRate"));
    // Interactions acquire evidence synchronously; the Director resolves on a later tick.
    if(EvidenceCount()!=GuardEvidence||SampledCount()!=GuardSampled||
        Ids(Run.Emitted)!=GuardEmitted||EvidenceIds()!=GuardAcquired)return Reject(TEXT("EvidenceHistoryChanged"));
    if(!Progress())return Reject(TEXT("MissingProgressSubsystem"));
    const bool bFinal=Finalized();
    const int32 CurrentProgress=Progress()->GetResolvedEncounterCount();
    const bool bCompletionWindow=Run.bAttention&&Run.bIntegration&&Step.Case==TEXT("J06")&&Step.Name==TEXT("SavedLanternSeeking");
    const bool bUnchanged=bFinal==GuardFinal&&CurrentProgress==GuardProgress;
    const bool bExpectedTransition=bCompletionWindow&&!GuardFinal&&GuardProgress==Run.InitialProgress&&
        bFinal&&CurrentProgress==Run.InitialProgress+1&&GuardEvidence==3&&GuardSampled==3;
    if(!bUnchanged&&!bExpectedTransition)return Reject(TEXT("UnexpectedCompletionTransition"));
    if(Run.Director.IsValid()&&!EvidenceMatches(GuardEvidence,GuardSampled,bFinal))return Reject(TEXT("EvidenceOrProgressMismatch"));
    if(bCompletionWindow&&(!Run.Director.IsValid()||(!bFinal&&!AllOutcomesHidden())))
        return Reject(TEXT("PrematureCompletionPresentation"));
    // FinalPresentation remains the terminal predicate, held stable for 0.5 game seconds.
    // Latch only the coherent 0 -> 1 transition; never permit rollback or a second increment.
    if(bExpectedTransition){GuardFinal=bFinal;GuardProgress=CurrentProgress;}
    if(Step.Name==TEXT("EarlyReacquire")&&Tracking()&&Now-Run.Frozen.LastSuccessfulSightingTime<2)bEarlyReacquired=true;
    // Every accepted frame retains the physical and evidence checks.
    if (bLog) LogAttentionSample(TEXT("FRAME"),Step,Dt,Delta,OtherDt,OtherDelta);
    return true;
}
void MakeAttentionSteps()
{
    Add(TEXT("A01"),TEXT("DefaultDisabled"),[]{return FreshAttention(0,40,false);},[]{return Visible()&&IsMode(EAttention::Disabled)&&YawError(0)<0.1;},0.35,PoseUnchanged);
    Add(TEXT("A02"),TEXT("EnableWithoutSight"),[]{return FreshAttention(0,180);},[]{return NoSighting()&&IdleAtAnchor();},0.5,EmptyAwareness);
    Add(TEXT("A03"),TEXT("InitiallyBehind"),Noop,[]{return NoSighting()&&IdleAtAnchor()&&YawError(0)<0.1;},0.7,EmptyAwareness);
    Add(TEXT("A04"),TEXT("TurnRight"),[]{return MoveBearing(45);},[]{return Aligned()&&AttentionPose.Total>35&&YawError(45)<2.6;});
    Add(TEXT("A05"),TEXT("TurnLeft"),[]{return MoveBearing(-10);},[]{return Aligned()&&AttentionPose.Total< -45&&YawError(-10)<2.6;});
    Add(TEXT("A06"),TEXT("StationaryConverges"),Noop,[]{return Aligned()&&AttentionPose.Travel<0.15;},0.5);
    Add(TEXT("A07"),TEXT("VisibleMovingPlayer"),[]{return MoveBearing(35);},[]{return Aligned()&&AttentionPose.Total>35;});
    Add(TEXT("A08"),TEXT("BoundedAngularSpeed"),[]{return MoveBearing(-25);},[]{return Aligned()&&AttentionPose.Total< -50&&AttentionPose.Frames>=4;});
    Add(TEXT("A09"),TEXT("WrapShortestPath"),[]{return FreshAttention(170,-150);},[]{return Aligned()&&AttentionPose.Total>30&&AttentionPose.Total<45&&BodyYaw()< -145;});
    Add(TEXT("A10"),TEXT("OffsetEye"),[]{if(!DisableAttention())return false;Run.Observer->GetObserverEye()->SetRelativeLocationAndRotation(FVector(20,80,84),FRotator(0,10,0));return FreshAttention(0,50);},[]{return Aligned()&&AttentionPose.Total>20;});
    Add(TEXT("A11"),TEXT("StationaryBody"),Noop,[]{return Aligned()&&AttentionPose.Travel<0.15;},0.5);
    Add(TEXT(""),TEXT("BeforeHoldWhileTurning"),[]{return MoveBearing(70,1400);},[]{return Tracking()&&!EyeAligned(Run.Observer.Get(),State().LastKnownPlayerLocation);},0.0);
    Add(TEXT("A12"),TEXT("HoldOnLoss"),[]{return FreezeMemory()&&BroadWall()&&PlayerRayHitsWall();},[]{return Holding()&&EyeAligned(Run.Observer.Get(),Run.Frozen.LastKnownPlayerLocation)&&AttentionPose.Total>1;},0.12,MemoryUnchanged);
    Add(TEXT("A13"),TEXT("HiddenLateralMovement"),[]{return MoveBearing(-45,950)&&PlayerRayHitsWall();},[]{return Holding()&&EyeAligned(Run.Observer.Get(),Run.Frozen.LastKnownPlayerLocation)&&AttentionPose.Travel<0.2;},0.15,MemoryUnchanged);
    Add(TEXT("A14"),TEXT("AttentionExpires"),Noop,[]{return State().Status==EStatus::LostSight&&MemoryUnchanged()&&IsMode(EAttention::Idle)&&!Attention().LookPoint.IsSet()&&Run.World->GetTimeSeconds()-Run.Frozen.LastSuccessfulSightingTime>=2;},0.1,MemoryUnchanged);
    Add(TEXT("A15"),TEXT("IdleReturn"),Noop,[]{return IdleAtAnchor()&&MemoryUnchanged()&&AttentionPose.Total< -10;},0.3,MemoryUnchanged);
    Add(TEXT(""),TEXT("BeforeEarlyLoss"),[]{if(!DisableAttention()||!Run.Observer.IsValid())return false;Run.Observer->GetObserverEye()->SetRelativeLocationAndRotation(FVector(0,0,64),FRotator::ZeroRotator);return FreshAttention(0,40);},Aligned);
    Add(TEXT(""),TEXT("EarlyLoss"),[]{return FreezeMemory()&&BroadWall();},Holding,0.1,MemoryUnchanged);
    Add(TEXT("A16"),TEXT("EarlyReacquire"),[]{bEarlyReacquired=false;return WallOff()&&MoveBearing(15);},[]{return bEarlyReacquired&&Aligned()&&State().LastSuccessfulSightingTime>Run.Frozen.LastSuccessfulSightingTime;});
    Add(TEXT(""),TEXT("LateLoss"),[]{return FreezeMemory()&&BroadWall();},[]{return IdleAtAnchor()&&MemoryUnchanged()&&State().Status==EStatus::LostSight;},0.15,MemoryUnchanged);
    Add(TEXT("A17"),TEXT("LateReacquire"),[]{return WallOff()&&MoveBearing(-40);},[]{return Aligned()&&State().LastSuccessfulSightingTime>Run.Frozen.LastSuccessfulSightingTime&&AttentionPose.Total< -30;});
    Add(TEXT(""),TEXT("BeginMovingBeforeDisable"),[]{return MoveBearing(20);},[]{return Tracking()&&AttentionPose.Total>5&&AttentionPose.Total<45;},0.0);
    Add(TEXT("A18"),TEXT("DisableMidTurn"),[]{PausedYaw=BodyYaw();return DisableAttention();},[]{return Visible()&&IsMode(EAttention::Disabled)&&YawError(PausedYaw)<0.1&&AttentionPose.Travel<0.1;},0.5);
    Add(TEXT("A19"),TEXT("ReenableFreshAnchor"),[]{AnchorExpected=BodyYaw();return EnableAttention();},[]{return Aligned()&&YawError(AnchorExpected)>10&&FMath::Abs(FMath::FindDeltaAngleDegrees(AnchorExpected,Attention().IdleYaw))<0.1;});
    Add(TEXT(""),TEXT("RepeatedEnableKeepsAnchor"),EnableAttention,[]{return Aligned()&&FMath::Abs(FMath::FindDeltaAngleDegrees(AnchorExpected,Attention().IdleYaw))<0.1;});
    Add(TEXT("A20"),TEXT("DestroyPlayer"),[]{OriginalPlayerName=GetNameSafe(Run.Player.Get());return Run.Player.IsValid()&&Run.Player->Destroy();},[]{return !Run.Player.IsValid()&&EmptyAwareness()&&IdleAtAnchor();},0.3);
    Add(TEXT(""),TEXT("ReplaceHidden"),[]{auto* P=SpawnObserver(ArenaOrigin-FRotator(0,BodyYaw(),0).Vector()*700,FRotator::ZeroRotator);if(!IsValid(P))return false;Run.PC->Possess(P);Run.Player=P;return P->IsPlayerControlled()&&Freeze(P);},[]{return NoSighting()&&IdleAtAnchor()&&GetNameSafe(Run.Player.Get())!=OriginalPlayerName;},0.5,EmptyAwareness);
    Add(TEXT(""),TEXT("ReplaceVisible"),[]{return MoveBearing(AnchorExpected+30);},Aligned);
    Add(TEXT("A21"),TEXT("UnpossessClearsAttention"),[]{Run.Controller->UnPossess();return true;},[]{return EmptyAwareness()&&!Run.Controller->IsAwarenessRunning()&&!Run.Controller->IsAttentionRunning()&&IsMode(EAttention::Disabled);},0.3);
    Add(TEXT(""),TEXT("RepossessFreshAnchor"),[]{AnchorExpected=BodyYaw();if(!MoveBearing(AnchorExpected+180))return false;Run.Controller->Possess(Run.Observer.Get());return Freeze(Run.Observer.Get());},[]{return NoSighting()&&IdleAtAnchor()&&YawError(AnchorExpected)<0.1;},0.4,EmptyAwareness);
    Add(TEXT(""),TEXT("RepossessNewSight"),[]{return MoveBearing(AnchorExpected-35);},Aligned);
    Add(TEXT(""),TEXT("DestroyAttentionController"),[]{Run.DestroyedTimer=Run.Controller->GetAwarenessRefreshTimerHandle();PausedYaw=BodyYaw();return Run.DestroyedTimer.IsValid()&&Run.Controller->Destroy();},[]{return !Run.Controller.IsValid()&&!IsValid(Run.Observer->GetController())&&!Run.World->GetTimerManager().TimerExists(Run.DestroyedTimer)&&IsMode(EAttention::Disabled)&&YawError(PausedYaw)<0.1;},0.3);
    Add(TEXT("A22"),TEXT("IndependentTurning"),[]{if(!FreshAttention(0,30))return false;Run.SecondObserver=SpawnObserver(ArenaOrigin+FVector(0,750,0),FRotator::ZeroRotator);Run.SecondController=Attach(Run.SecondObserver.Get());if(!Run.SecondController.IsValid())return false;Run.SecondController->SetAttentionEnabled(true);return true;},[]{const auto* C=Run.SecondController.Get();return Aligned()&&IsValid(C)&&C->GetAttentionDecision().Mode==EAttention::TrackObserved&&C->GetAwarenessState().Status==EStatus::Perceived&&EyeAligned(Run.SecondObserver.Get(),Run.Player->GetActorLocation())&&AttentionPose.Total>20&&SecondPose.Total< -20;});
    Add(TEXT(""),TEXT("DemoSetup"),[]{DestroyController(Run.SecondController);if(Run.SecondObserver.IsValid())Run.SecondObserver->Destroy();Run.SecondObserver.Reset();return FreshAttention(0,0)&&BuildEvidenceRig()&&Place(Run.A.Get(),ArenaOrigin+FVector(600,-500,64))&&Place(Run.B.Get(),ArenaOrigin+FVector(800,400,64));},[]{return Aligned()&&EvidenceMatches(0,0)&&TraceProp(Run.A.Get())==ETrace::Visible;});
    Add(TEXT(""),TEXT("DemoVisibleAction"),[]{return Inspect(Run.A.Get(),true);},[]{return Aligned()&&EvidenceMatches(1,1);});
    Add(TEXT(""),TEXT("DemoTurnAwayFromEvent"),[]{return MoveBearing(60);},[]{return Aligned()&&AttentionPose.Total>45&&TraceProp(Run.A.Get())==ETrace::OutsideViewCone&&EvidenceMatches(1,1);});
    Add(TEXT("A23"),TEXT("OutsideConeAction"),[]{return InspectExpected(Run.A.Get(),ETrace::OutsideViewCone);},[]{return Aligned()&&EvidenceMatches(1,1);});
    Add(TEXT(""),TEXT("DemoHold"),[]{return FreezeMemory()&&BroadWall();},Holding,0.1,MemoryUnchanged);
    Add(TEXT(""),TEXT("DemoHiddenAction"),[]{return MoveBearing(-35,950)&&PlayerRayHitsWall()&&Inspect(Run.B.Get(),false);},[]{return Holding()&&EvidenceMatches(1,1);},0.1,MemoryUnchanged);
    Add(TEXT(""),TEXT("DemoIdle"),Noop,[]{return IdleAtAnchor()&&MemoryUnchanged()&&EvidenceMatches(1,1);},0.2,MemoryUnchanged);
    Add(TEXT(""),TEXT("DemoReacquire"),[]{return WallOff()&&MoveBearing(40);},[]{return Aligned()&&EvidenceMatches(1,1)&&TraceProp(Run.B.Get())==ETrace::Visible;},0.4);
    Add(TEXT("A24"),TEXT("DemoNewAction"),[]{return Inspect(Run.B.Get(),true);},[]{return Aligned()&&EvidenceMatches(2,2);},0.4);
}

// Saved layout has already supported a single initial facing for all three
// real props. Search a visible player point whose predicted final facing keeps
// all three sources in cone. No manual aim after enabling attention.
bool PlaceSavedAttentionPlayer()
{
    FVector Eye;FRotator Facing;Run.Observer->GetActorEyesViewPoint(Eye,Facing);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PedroAttentionSavedPlayer),false);
    Params.AddIgnoredActor(Run.Observer.Get());Params.AddIgnoredActor(Run.Player.Get());
    for(double Offset:{30.0,-30.0,20.0,-20.0,40.0,-40.0,17.0,-17.0})
    for(double Distance:{200.0,300.0,450.0,600.0})
    {
        const FVector At=Eye+Facing.Vector().RotateAngleAxis(Offset,FVector::UpVector)*Distance;
        FHitResult Hit;if(Run.World->LineTraceSingleByChannel(Hit,Eye,At,ECC_Visibility,Params))continue;
        const FVector Future=Facing.Vector().RotateAngleAxis(Offset,FVector::UpVector);
        bool bAll=true;
        for(auto* Prop:{Run.A.Get(),Run.B.Get(),Run.C.Get()})
            if(!IsValid(Prop)||PedroDirectObservation::CheckGeometry(Eye,Future,Prop->GetActorLocation(),Run.Director->DirectObservationMaxDistance,73)!=ETrace::Visible)bAll=false;
        if(bAll&&Place(Run.Player.Get(),At))return true;
    }
    return false;
}
void MakeAttentionIntegrationSteps()
{
    Add(TEXT("J01"),TEXT("SavedConfiguration"),[]{return SavedMapConfig()&&Freeze(Run.Observer.Get())&&Freeze(Run.Player.Get());},[]{return EvidenceMatches(0,0)&&AllOutcomesHidden()&&!Run.Controller.IsValid();});
    Add(TEXT("J02"),TEXT("AutonomousSavedTurn"),[]{return PlaceSavedAttentionPlayer()&&FreshController()&&CreateWall()&&EnableAttention();},[]{return Aligned()&&AttentionPose.Travel>15&&EvidenceMatches(0,0)&&TraceProp(Run.A.Get())==ETrace::Visible&&TraceProp(Run.B.Get())==ETrace::Visible&&TraceProp(Run.C.Get())==ETrace::Visible;});
    Add(TEXT("J03"),TEXT("SavedStatue"),[]{return Inspect(Run.A.Get(),true);},[]{return Aligned()&&EvidenceMatches(1,1)&&AllOutcomesHidden();});
    Add(TEXT("J04"),TEXT("SavedRadioOccluded"),[]{return WallForProp(Run.B.Get(),30)&&Inspect(Run.B.Get(),false);},[]{return EvidenceMatches(1,1)&&AllOutcomesHidden();},0.3);
    Add(TEXT(""),TEXT("SavedRestoreNoRetroactive"),WallOff,[]{return Aligned()&&EvidenceMatches(1,1)&&TraceProp(Run.B.Get())==ETrace::Visible;},0.5);
    Add(TEXT("J05"),TEXT("SavedRadioNewAction"),[]{return Inspect(Run.B.Get(),true);},[]{return Aligned()&&EvidenceMatches(2,2)&&AllOutcomesHidden();});
    Add(TEXT("J06"),TEXT("SavedLanternSeeking"),[]{
        return EvidenceMatches(2,2)&&AllOutcomesHidden()&&Inspect(Run.C.Get(),true)&&
            EvidenceMatches(3,3)&&AllOutcomesHidden();
    },FinalPresentation,0.5);
}

// Navigation fixture is opt-in and never alters the legacy frozen fixtures.
using NavMode=EPedroNavigationMode;
using NavReason=EPedroNavigationReason;
struct FNavigationFixture
{
    TArray<TWeakObjectPtr<AActor>> Geometry;
    TWeakObjectPtr<ANavMeshBoundsVolume> Bounds;
    TWeakObjectPtr<AActor> PhysicalBlocker;
    FVector Previous=FVector::ZeroVector,Start=FVector::ZeroVector,EyeRelative=FVector::ZeroVector,OtherPrevious=FVector::ZeroVector;
    FRotator EyeRelativeRotation=FRotator::ZeroRotator;
    TWeakObjectPtr<APedroObserverCharacter> OtherActor;
    FVector FixedGoal=FVector::ZeroVector,FixedSighting=FVector::ZeroVector;
    double PrevTime=0,PrevYaw=0,Travel=0,YawTravel=0,MaxSpeed=0,MaxSide=0,FixedTime=0;
    double OriginalSpeed=0,OriginalAccel=0,OriginalBrake=0;
    uint32 Submissions=0,FixedRequest=0;
    bool bOriginalAcceleration=false,bCapability=false,bSavedReady=false;
    bool bSavedPair=false;
    FVector SavedSecondPlayer=FVector::ZeroVector, SavedSecondGoal=FVector::ZeroVector, SavedSecondLanding=FVector::ZeroVector;
    FVector SavedRelocationRoot=FVector::ZeroVector;
    uint32 SavedRelocationSubmissions=0;
    FVector SavedRestorePlayer=FVector::ZeroVector;
    double SavedRestoreSightingTime=0;
    uint32 SavedRestoreSubmissions=0;
};
FNavigationFixture NavTest;
constexpr double NavGroundOffset=88;
const TCHAR* NavModeName(NavMode M)
{
    switch(M){case NavMode::Disabled:return TEXT("Disabled");case NavMode::Idle:return TEXT("Idle");case NavMode::FollowObserved:return TEXT("FollowObserved");case NavMode::ContinueLastKnown:return TEXT("ContinueLastKnown");case NavMode::Blocked:return TEXT("Blocked");}
    return TEXT("Invalid");
}
const TCHAR* NavSuite(){return Run.bNavProbe?TEXT("navigation-probe"):(Run.bIntegration?TEXT("navigation-integration"):TEXT("navigation"));}
FVector NavRoot(){return Run.Observer.IsValid()?Run.Observer->GetActorLocation():FVector::ZeroVector;}
FVector NavVelocity(){return Run.Observer.IsValid()?Run.Observer->GetCharacterMovement()->Velocity:FVector::ZeroVector;}
NavMode NavigationMode(){return Run.Controller.IsValid()?Run.Controller->GetNavigationDecision().Mode:NavMode::Disabled;}
bool NavActive(){return Run.Controller.IsValid()&&Run.Controller->GetNavigationIntent().bActive&&Run.Controller->GetPedroMoveId().IsValid()&&Run.Controller->HasValidPedroPath();}
bool NavStopped(){return !NavActive()&&NavVelocity().Size2D()<1;}
uint32 NavSubmissions(){return Run.Controller.IsValid()?Run.Controller->GetPedroMoveSubmissions():0;}
bool NavSeen(){return Visible();}
bool NavFollowing(){return NavSeen()&&NavActive()&&NavigationMode()==NavMode::FollowObserved;}
bool NavArrived(){return NavSeen()&&NavStopped()&&NavigationMode()==NavMode::Idle&&FVector::Dist2D(NavRoot(),State().LastKnownPlayerLocation)<=505;}
bool NavBlocked(){return NavSeen()&&NavigationMode()==NavMode::Blocked&&NavStopped();}
UNavigationSystemV1* NavSystem(){return FNavigationSystem::GetCurrent<UNavigationSystemV1>(Run.World.Get());}
const ANavigationData* NavData()
{
    auto* NS=NavSystem();auto* M=Run.Observer.IsValid()?Run.Observer->GetCharacterMovement():nullptr;
    return NS&&M?NS->GetNavDataForProps(M->GetNavAgentPropertiesRef(),M->GetFeetLocation(),FVector::ZeroVector):nullptr;
}
bool NavProject(const FVector& At,FNavLocation& Out,const FVector& Extent=FVector(75,75,180))
{
    return NavData()&&NavSystem()->ProjectPointToNavigation(At,Out,Extent,NavData());
}
FNavPathSharedPtr NavQuery(const FVector& From,const FVector& To)
{
    const auto* Data=NavData();if(!Data)return {};
    FPathFindingQuery Q(Run.Controller.Get(),*Data,From,To);Q.SetAllowPartialPaths(false);
    Q.SetNavAgentProperties(Run.Observer->GetCharacterMovement()->GetNavAgentPropertiesRef());
    const auto R=NavSystem()->FindPathSync(Q);
    if(!R.IsSuccessful()||!R.Path.IsValid()||!R.Path->IsValid()||R.Path->IsPartial())return {};
    return R.Path;
}
AStaticMeshActor* NavBox(const FVector& Center,const FVector& Size,bool bAffectNav=true)
{
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* A=SpawnAt<AStaticMeshActor>(Center);if(!IsValid(A)||!IsValid(Cube))return nullptr;
    auto* C=A->GetStaticMeshComponent();C->SetMobility(EComponentMobility::Movable);
    C->SetCanEverAffectNavigation(false);C->SetStaticMesh(Cube);C->SetCollisionProfileName(TEXT("BlockAll"));C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    A->SetActorScale3D(Size/100.0);C->SetCanEverAffectNavigation(bAffectNav);
    NavTest.Geometry.Add(A);return A;
}
bool NavBounds(const FVector& Center,const FVector& Extent)
{
    auto* NS=NavSystem();if(!NS)return false;
    auto* V=SpawnAt<ANavMeshBoundsVolume>(Center);if(!IsValid(V)||!V->GetBrushComponent())return false;
    // A spawned brush has no authored model. Use the runtime bounds explicitly;
    // real projection/path tests below are mandatory, not a bounds-only pass.
    V->GetBrushComponent()->Bounds=FBoxSphereBounds(FBox(Center-Extent,Center+Extent));
    NS->OnNavigationBoundsUpdated(V);NavTest.Bounds=V;
    UE_LOG(LogTemp,Display,TEXT("PEDRO_NAV_FIXTURE transientBounds center=%s extent=%s"),*VectorString(Center),*VectorString(Extent));
    NS->Build();
    return true;
}
bool EnableLocomotion(APedroObserverCharacter* O)
{
    if(!IsValid(O))return false;
    auto* M=O->GetCharacterMovement();O->GetCapsuleComponent()->SetCanEverAffectNavigation(false);
    M->SetComponentTickEnabled(true);M->StopMovementImmediately();M->SetMovementMode(MOVE_Walking);
    return M->IsComponentTickEnabled()&&M->IsMovingOnGround();
}
APedroObserverAIController* AttachMovable(APedroObserverCharacter* O)
{
    if(!IsValid(O))return nullptr;
    if(auto* Existing=O->GetController();IsValid(Existing)){Existing->UnPossess();Existing->Destroy();}
    auto* C=SpawnAt<APedroObserverAIController>(O->GetActorLocation());if(!IsValid(C))return nullptr;
    C->Possess(O);if(C->GetPawn()!=O||!EnableLocomotion(O)){C->Destroy();return nullptr;}return C;
}
bool NavEnable(bool Enabled=true)
{
    if(!Run.Controller.IsValid())return false;
    Run.Controller->SetNavigationEnabled(Enabled);
    return Run.Controller->IsNavigationRunning()==Enabled;
}
bool NavReset(const FVector& ObserverOffset,const FVector& PlayerOffset,bool bEnable=true,bool bAttention=false,double Yaw=0)
{
    DestroyController(Run.Controller);
    if(!Run.Observer.IsValid()||!Run.Player.IsValid()||!WallOff())return false;
    Run.Observer->GetObserverEye()->SetRelativeLocationAndRotation(FVector(0,0,64),FRotator::ZeroRotator);
    if(!Place(Run.Observer.Get(),ArenaOrigin+ObserverOffset,FRotator(0,Yaw,0))||!MovePlayer(PlayerOffset))return false;
    Run.Controller=AttachMovable(Run.Observer.Get());if(!Run.Controller.IsValid())return false;
    Run.Controller->SetAttentionEnabled(bAttention);
    return !bEnable||NavEnable();
}
bool PrepareNavigationArena()
{
    if(auto* P=Cast<ACharacter>(Run.Player.Get()))P->GetCapsuleComponent()->SetCanEverAffectNavigation(false);
    if(!NavBox(ArenaOrigin+FVector(0,0,-NavGroundOffset-25),FVector(3000,3000,50))||
       !NavBox(ArenaOrigin+FVector(2100,0,-NavGroundOffset-25),FVector(600,800,50))||
       !NavBox(ArenaOrigin+FVector(-2400,0,-NavGroundOffset-25),FVector(600,800,50),false)||
       !NavBox(ArenaOrigin+FVector(250,900,-NavGroundOffset+35),FVector(80,440,70)))return false;
    if(!NavReset(FVector::ZeroVector,FVector(900,0,0),false))return false;
    if(!NavBounds(ArenaOrigin+FVector(400,0,0),FVector(2300,1750,350)))return false;
    Run.Box->SetCanEverAffectNavigation(false);
    return true;
}
bool NavCapability()
{
    const auto* Data=Cast<ARecastNavMesh>(NavData());if(!IsValid(Data)||Data->GetRuntimeGenerationMode()!=ERuntimeGenerationType::Dynamic)return false;
    FNavLocation Start,End,Island;
    if(!NavProject(ArenaOrigin,Start)||!NavProject(ArenaOrigin+FVector(700,0,0),End)||!NavProject(ArenaOrigin+FVector(2100,0,0),Island))return false;
    const auto Connected=NavQuery(Start.Location,End.Location);
    if(!Connected.IsValid()||Connected->GetLength()<600||NavQuery(Start.Location,Island.Location).IsValid())return false;
    if(!NavTest.bCapability)UE_LOG(LogTemp,Display,TEXT("PEDRO_NAV_TEST status=CAPABILITY dynamic=yes connected=yes disconnected=yes start=%s end=%s island=%s length=%.3f"),*VectorString(Start.Location),*VectorString(End.Location),*VectorString(Island.Location),Connected->GetLength());
    NavTest.bCapability=true;return true;
}
bool NavRemember()
{
    if(!NavFollowing()||!FreezeMemory())return false;
    NavTest.FixedGoal=Run.Controller->GetPedroMoveGoal();NavTest.FixedRequest=Run.Controller->GetPedroMoveId().GetID();
    NavTest.FixedSighting=Run.Controller->GetNavigationIntent().Sighting;NavTest.FixedTime=Run.Controller->GetNavigationIntent().SightingTime;
    NavTest.Submissions=NavSubmissions();return true;
}
bool NavFrozenRoute()
{
    if(!MemoryUnchanged()||NavSubmissions()!=NavTest.Submissions)return false;
    return !NavActive()||(Run.Controller->GetPedroMoveId().GetID()==NavTest.FixedRequest&&Run.Controller->GetPedroMoveGoal().Equals(NavTest.FixedGoal,.01)&&
        Run.Controller->GetNavigationIntent().Sighting.Equals(NavTest.FixedSighting,.01)&&Run.Controller->GetNavigationIntent().SightingTime==NavTest.FixedTime);
}
bool NavCover(){return WallAt(ArenaOrigin+FVector(900,0,0),FVector(35,1000,500));}
void CaptureNavPose()
{
    NavTest.Start=NavTest.Previous=NavRoot();NavTest.PrevTime=Run.World->GetTimeSeconds();NavTest.PrevYaw=BodyYaw();
    NavTest.Travel=NavTest.YawTravel=NavTest.MaxSpeed=NavTest.MaxSide=0;
    NavTest.EyeRelative=Run.Observer->GetObserverEye()->GetRelativeLocation();NavTest.EyeRelativeRotation=Run.Observer->GetObserverEye()->GetRelativeRotation();
    NavTest.OtherActor=Run.SecondObserver;NavTest.OtherPrevious=Run.SecondObserver.IsValid()?Run.SecondObserver->GetActorLocation():FVector::ZeroVector;
}
void LogNavSample(const TCHAR* Kind,const FStep& Step,double Dt=0)
{
    const auto* C=Run.Controller.Get();const auto& A=State();const auto* M=Run.Observer.IsValid()?Run.Observer->GetCharacterMovement():nullptr;
    const auto* Intent=C?&C->GetNavigationIntent():nullptr;
    const FVector Actual=Run.Player.IsValid()?Run.Player->GetActorLocation():FVector::ZeroVector;
    const FString Owner=GetNameSafe(A.ObservedPlayer.Get()),Player=GetNameSafe(Run.Player.Get());
    const auto Rotation=Run.Observer.IsValid()?Run.Observer->GetActorRotation():FRotator::ZeroRotator;
    FVector Eye=FVector::ZeroVector;FRotator Facing=FRotator::ZeroRotator;if(Run.Observer.IsValid())Run.Observer->GetActorEyesViewPoint(Eye,Facing);
    const FVector Rel=Run.Observer.IsValid()?Run.Observer->GetObserverEye()->GetRelativeLocation():FVector::ZeroVector;
    const FRotator RelRot=Run.Observer.IsValid()?Run.Observer->GetObserverEye()->GetRelativeRotation():FRotator::ZeroRotator;
    const auto* Other=Run.SecondController.Get();
    UE_LOG(LogTemp,Display,TEXT("PEDRO_NAV_TEST status=%s case=%s step=%s now=%.6f dt=%.6f root=%s start=%s velocity=%s body=%.6f,%.6f,%.6f eye=%s forward=%s rel=%s relRot=%.6f,%.6f,%.6f travel=%.3f yawTravel=%.3f maxSide=%.3f mode=%s reason=%d running=%s attention=%d active=%s id=%u valid=%s goal=%s sighting=%s sightTime=%.6f planTime=%.6f length=%.3f submissions=%u controller=%s speedLimit=%.3f accel=%.3f brake=%.3f accelPaths=%s awareness=%s has=%s known=%s actual=%s time=%.6f target=%s player=%s grounded=%s evidence=%d sampled=%d emitted=%s acquired=%s final=%s progress=%d otherId=%u otherRoot=%s"),
        Kind,Step.Case.IsEmpty()?TEXT("-"):*Step.Case,*Step.Name,Run.World->GetTimeSeconds(),Dt,*VectorString(NavRoot()),*VectorString(NavTest.Start),*VectorString(NavVelocity()),Rotation.Pitch,Rotation.Yaw,Rotation.Roll,
        *VectorString(Eye),*VectorString(Facing.Vector()),*VectorString(Rel),RelRot.Pitch,RelRot.Yaw,RelRot.Roll,NavTest.Travel,NavTest.YawTravel,NavTest.MaxSide,
        NavModeName(NavigationMode()),C?int32(C->GetNavigationDecision().Reason):0,Yes(C&&C->IsNavigationRunning()),C?int32(C->GetAttentionDecision().Mode):0,
        Yes(Intent&&Intent->bActive),C&&C->GetPedroMoveId().IsValid()?C->GetPedroMoveId().GetID():0,Yes(C&&C->HasValidPedroPath()),*VectorString(C?C->GetPedroMoveGoal():FVector::ZeroVector),
        *VectorString(Intent?Intent->Sighting:FVector::ZeroVector),Intent?Intent->SightingTime:0,Intent?Intent->LastPlan:-1,C?C->GetPedroPathLength():0,NavSubmissions(),*GetNameSafe(C),M?M->MaxWalkSpeed:0,M?M->MaxAcceleration:0,M?M->BrakingDecelerationWalking:0,Yes(M&&M->GetNavMovementProperties().bUseAccelerationForPaths),StatusName(A.Status),Yes(A.bHasLastKnownPlayerLocation),
        *VectorString(A.LastKnownPlayerLocation),*VectorString(Actual),A.LastSuccessfulSightingTime,*Owner,*Player,Yes(M&&M->IsMovingOnGround()),EvidenceCount(),SampledCount(),*Ids(Run.Emitted),*EvidenceIds(),Yes(Finalized()),Progress()?Progress()->GetResolvedEncounterCount():-1,
        Other&&Other->GetPedroMoveId().IsValid()?Other->GetPedroMoveId().GetID():0,*VectorString(Run.SecondObserver.IsValid()?Run.SecondObserver->GetActorLocation():FVector::ZeroVector));
}
bool GuardNav(const FStep& Step,bool bLog=true)
{
    if(!Run.Observer.IsValid())return false;
    const double Now=Run.World->GetTimeSeconds(),Dt=Now-NavTest.PrevTime;
    if(Dt<0)return false;
    if(Dt==0)return true;
    const auto Root=NavRoot();const double Distance=FVector::Dist2D(Root,NavTest.Previous);
    const double YawDelta=FMath::Abs(FMath::FindDeltaAngleDegrees(NavTest.PrevYaw,BodyYaw()));
    const auto R=Run.Observer->GetActorRotation();
    if(Root.ContainsNaN()||NavVelocity().ContainsNaN()||Distance>190*Dt+.5||NavVelocity().Size2D()>185||YawDelta>90*FMath::Min(Dt,.1)+.8||
        FMath::Abs(R.Pitch)>.5||FMath::Abs(R.Roll)>.5||FMath::Abs(Root.Z-NavTest.Start.Z)>12)return false;
    if(!Run.Observer->GetObserverEye()->GetRelativeLocation().Equals(NavTest.EyeRelative,.01)||!Run.Observer->GetObserverEye()->GetRelativeRotation().Equals(NavTest.EyeRelativeRotation,.01))return false;
    if(NavTest.OtherActor.IsValid())
    {
        if(NavTest.OtherActor!=Run.SecondObserver||FVector::Dist2D(NavTest.OtherPrevious,Run.SecondObserver->GetActorLocation())>190*Dt+.5||Run.SecondObserver->GetVelocity().Size2D()>185)return false;
        NavTest.OtherPrevious=Run.SecondObserver->GetActorLocation();
    }
    if(Step.Case==TEXT("N12"))
    {
        const FVector Relative=Root-ArenaOrigin;const double X=FMath::Clamp(Relative.X,210.,290.),Y=FMath::Clamp(Relative.Y,680.,1120.);
        if(FVector::Dist2D(Relative,FVector(X,Y,Relative.Z))<Run.Observer->GetCapsuleComponent()->GetScaledCapsuleRadius()-2)return false;
    }
    if(Run.Controller.IsValid()&&Run.Controller->GetNavigationIntent().bActive&&!NavActive())return false;
    if(NavActive()&&(!Run.Controller->IsNavigationRunning()||Run.Controller->GetPathFollowingComponent()->GetCurrentRequestId().GetID()!=Run.Controller->GetPedroMoveId().GetID()||
        Run.Controller->GetPedroPathLength()>2200.1||!Run.Observer->GetCharacterMovement()->IsMovingOnGround()))return false;
    if(!Run.Controller.IsValid()||!Run.Controller->IsAttentionRunning())if(YawDelta>.1)return false;
    NavTest.Travel+=Distance;NavTest.YawTravel+=YawDelta;NavTest.MaxSpeed=FMath::Max(NavTest.MaxSpeed,NavVelocity().Size2D());
    NavTest.MaxSide=FMath::Max(NavTest.MaxSide,FMath::Abs(Root.Y-NavTest.Start.Y));
    NavTest.Previous=Root;NavTest.PrevTime=Now;NavTest.PrevYaw=BodyYaw();if(bLog)LogNavSample(TEXT("FRAME"),Step,Dt);
    return true;
}
void MakeNavigationSteps()
{
    Add(TEXT("N01"),TEXT("DisabledWithNavmesh"),PrepareNavigationArena,[]{return NavCapability()&&NavSeen()&&NavStopped()&&NavigationMode()==NavMode::Disabled&&NavTest.Travel<2;},.5);
    if(!Run.bNavProbe)
    {
        Add(TEXT("N02"),TEXT("InitiallyHidden"),[]{return NavReset(FVector::ZeroVector,FVector(-900,0,0));},[]{return EmptyAwareness()&&NavStopped()&&NavSubmissions()==0;},.6);
        Add(TEXT("N03"),TEXT("WithinBand"),[]{return NavReset(FVector::ZeroVector,FVector(450,0,0));},[]{return NavSeen()&&NavStopped()&&NavSubmissions()==0;},.5);
    }
    Add(TEXT("N04"),TEXT("NativeFollow"),[]{return NavReset(FVector::ZeroVector,FVector(1200,0,0));},[]{return NavFollowing()&&NavVelocity().Size2D()>50&&NavTest.Travel>80;},.05);
    if(Run.bNavProbe)return;
    Add(TEXT("N05"),TEXT("StandoffArrival"),Noop,[]{return NavArrived()&&FVector::Dist2D(NavRoot(),State().LastKnownPlayerLocation)>=400;},.4);
    Add(TEXT("N06"),TEXT("HysteresisIdle"),[]{NavTest.Submissions=NavSubmissions();return Place(Run.Player.Get(),NavRoot()+FVector(575,0,0));},[]{return NavSeen()&&NavStopped()&&NavSubmissions()==NavTest.Submissions&&NavTest.Travel<2;},.6);
    Add(TEXT(""),TEXT("ApproachWhileMoving"),[]{return NavReset(FVector::ZeroVector,FVector(1200,0,0));},[]{return NavFollowing()&&NavTest.Travel>70;},.05);
    Add(TEXT("N07"),TEXT("PlayerApproaches"),[]{return Place(Run.Player.Get(),NavRoot()+FVector(350,0,0));},[]{return NavSeen()&&NavStopped()&&NavTest.Travel<50;},.3);
    Add(TEXT("N08"),TEXT("SpeedAndBraking"),[]{return NavReset(FVector::ZeroVector,FVector(1200,0,0));},[]{return NavArrived()&&NavTest.Travel>600&&NavTest.MaxSpeed>150&&NavTest.MaxSpeed<=185;},.4);
    Add(TEXT(""),TEXT("BeforeReplan"),[]{return NavReset(FVector::ZeroVector,FVector(1300,0,0));},[]{return NavFollowing()&&NavTest.Travel>50;},.05);
    Add(TEXT("N09"),TEXT("ReplanVisible"),[]{NavTest.FixedGoal=Run.Controller->GetPedroMoveGoal();NavTest.Submissions=NavSubmissions();return MovePlayer(FVector(1300,240,0));},[]{return NavFollowing()&&NavSubmissions()==NavTest.Submissions+1&&FVector::Dist2D(NavTest.FixedGoal,Run.Controller->GetPedroMoveGoal())>80;},.05);
    Add(TEXT("N10"),TEXT("SmallMovementRetained"),[]{NavTest.FixedRequest=Run.Controller->GetPedroMoveId().GetID();NavTest.Submissions=NavSubmissions();return MovePlayer(FVector(1300,275,0));},[]{return NavFollowing()&&NavSeen()&&NavSubmissions()==NavTest.Submissions&&Run.Controller->GetPedroMoveId().GetID()==NavTest.FixedRequest;},.2);
    Add(TEXT(""),TEXT("RateFirst"),[]{NavTest.Submissions=NavSubmissions();return MovePlayer(FVector(1300,-200,0));},[]{return NavFollowing()&&NavSubmissions()==NavTest.Submissions+1;},0);
    Add(TEXT(""),TEXT("RateSecond"),[]{return MovePlayer(FVector(1300,200,0));},[]{return NavFollowing()&&NavSubmissions()==NavTest.Submissions+2;},0);
    Add(TEXT("N11"),TEXT("BoundedReplanning"),[]{return MovePlayer(FVector(1300,-200,0));},[]{return NavFollowing()&&NavSubmissions()==NavTest.Submissions+3;},0);
    Add(TEXT("N12"),TEXT("NativeDetour"),[]{return NavReset(FVector(0,900,0),FVector(1150,900,0));},[]{return NavArrived()&&NavTest.MaxSide>230&&NavTest.Travel>750;},.3);
    Add(TEXT("N13"),TEXT("NoNavigableStart"),[]{return NavReset(FVector(-2400,0,0),FVector(-1500,0,0));},[]{return NavBlocked()&&NavSubmissions()==0&&NavTest.Travel<2;},.3);
    Add(TEXT("N14"),TEXT("OffmeshGoal"),[]{return NavReset(FVector(900,-1150,0),FVector(2000,-1150,0));},[]{return NavSeen()&&(NavBlocked()||(NavActive()&&Run.Controller->HasValidPedroPath()&&Run.Controller->GetPedroMoveGoal().X<ArenaOrigin.X+1500));},.3);
    Add(TEXT("N15"),TEXT("DisconnectedRoute"),[]{return NavReset(FVector(1100,0,0),FVector(2200,0,0));},[]{return NavBlocked()&&NavSubmissions()==0&&NavTest.Travel<2;},.3);
    Add(TEXT("N16"),TEXT("PhysicalStall"),[]{if(!NavReset(FVector::ZeroVector,FVector(1300,0,0)))return false;NavTest.PhysicalBlocker=NavBox(ArenaOrigin+FVector(110,0,-NavGroundOffset+35),FVector(50,500,70),false);return NavTest.PhysicalBlocker.IsValid();},[]{return NavBlocked()&&NavSubmissions()>0&&NavRoot().X<ArenaOrigin.X+110;},.1);
    Add(TEXT("N17"),TEXT("RetryAfterBlockCleared"),[]{if(!NavTest.PhysicalBlocker.IsValid())return false;NavTest.PhysicalBlocker->Destroy();return true;},[]{return NavFollowing()&&NavSubmissions()>1&&NavRoot().X>ArenaOrigin.X+200;},.1);
    Add(TEXT(""),TEXT("BeforeStationaryLoss"),[]{return NavReset(FVector::ZeroVector,FVector(575,0,0));},[]{return NavSeen()&&NavStopped();},.2);
    Add(TEXT("N18"),TEXT("StationaryLoss"),[]{return FreezeMemory()&&WallAt(ArenaOrigin+FVector(300,0,0),FVector(35,1000,500));},[]{return Lost()&&NavStopped()&&NavSubmissions()==0;},.4,MemoryUnchanged);
    Add(TEXT(""),TEXT("BeforeFollowingLoss"),[]{return NavReset(FVector::ZeroVector,FVector(1300,0,0),true,true);},[]{return NavFollowing()&&NavTest.Travel>60;},.05);
    Add(TEXT("N19"),TEXT("ContinueFixedRoute"),[]{return NavRemember()&&NavCover();},[]{return Lost()&&NavActive()&&NavigationMode()==NavMode::ContinueLastKnown&&NavFrozenRoute()&&NavTest.Travel>30;},.05,NavFrozenRoute);
    Add(TEXT("N20"),TEXT("HiddenMoveNoRetarget"),[]{return MovePlayer(FVector(1300,350,0))&&PlayerRayHitsWall();},[]{return Lost()&&NavActive()&&NavigationMode()==NavMode::ContinueLastKnown&&NavFrozenRoute()&&NavTest.Travel>15;},.05,NavFrozenRoute);
    Add(TEXT("N21"),TEXT("ExpiredStops"),Noop,[]{return Lost()&&NavFrozenRoute()&&NavStopped()&&Run.World->GetTimeSeconds()-Run.Frozen.LastSuccessfulSightingTime>=2;},.4,NavFrozenRoute);
    Add(TEXT("N22"),TEXT("ReacquireNewRoute"),WallOff,[]{return NavFollowing()&&NavSubmissions()>NavTest.Submissions&&State().LastSuccessfulSightingTime>Run.Frozen.LastSuccessfulSightingTime;},.1);
    Add(TEXT(""),TEXT("BeforeDisable"),[]{if(!NavReset(FVector::ZeroVector,FVector(1300,0,0),false))return false;auto* M=Run.Observer->GetCharacterMovement();NavTest.OriginalSpeed=M->MaxWalkSpeed;NavTest.OriginalAccel=M->MaxAcceleration;NavTest.OriginalBrake=M->BrakingDecelerationWalking;NavTest.bOriginalAcceleration=M->GetNavMovementProperties()->bUseAccelerationForPaths;return NavEnable();},[]{return NavFollowing()&&NavTest.Travel>60;},.05);
    Add(TEXT(""),TEXT("DisabledStopsAndRestores"),[]{return NavEnable(false);},[]{auto* M=Run.Observer->GetCharacterMovement();return NavStopped()&&NavSeen()&&NavigationMode()==NavMode::Disabled&&NavTest.Travel<2&&M->MaxWalkSpeed==NavTest.OriginalSpeed&&M->MaxAcceleration==NavTest.OriginalAccel&&M->BrakingDecelerationWalking==NavTest.OriginalBrake&&M->GetNavMovementProperties()->bUseAccelerationForPaths==NavTest.bOriginalAcceleration;},.4);
    Add(TEXT("N23"),TEXT("ReenableFresh"),[]{return NavEnable()&&NavEnable();},[]{return NavFollowing()&&NavTest.Travel>35&&NavSubmissions()==1;},.05);
    Add(TEXT("N24"),TEXT("PlayerReplacement"),[]{auto* Old=Run.Player.Get();if(!IsValid(Old)||!Old->Destroy())return false;auto* P=SpawnObserver(NavRoot()-FVector(700,0,0),FRotator::ZeroRotator);if(!IsValid(P))return false;Run.PC->Possess(P);Run.Player=P;P->GetCapsuleComponent()->SetCanEverAffectNavigation(false);return Freeze(P);},[]{return EmptyAwareness()&&NavStopped();},.4);
    Add(TEXT(""),TEXT("BeforeUnpossess"),[]{return NavReset(FVector::ZeroVector,FVector(1300,0,0));},[]{return NavFollowing()&&NavTest.Travel>50;},.05);
    Add(TEXT(""),TEXT("UnpossessStops"),[]{Run.Controller->UnPossess();return true;},[]{return NavStopped()&&!Run.Controller->IsNavigationRunning()&&!Run.Controller->GetPawn()&&EmptyAwareness();},.3);
    Add(TEXT(""),TEXT("RepossessMoves"),[]{Run.Controller->Possess(Run.Observer.Get());return Run.Controller->GetPawn()==Run.Observer.Get();},[]{return NavFollowing()&&NavTest.Travel>50;},.05);
    Add(TEXT("N25"),TEXT("DestroyControllerStops"),[]{return Run.Controller->Destroy();},[]{return !Run.Controller.IsValid()&&!IsValid(Run.Observer->GetController())&&NavStopped()&&NavTest.Travel<2;},.4);
    Add(TEXT("N26"),TEXT("IndependentRoutes"),[]{if(!NavReset(FVector::ZeroVector,FVector(1300,0,0)))return false;Run.SecondObserver=SpawnObserver(ArenaOrigin+FVector(0,-600,0),FRotator::ZeroRotator);Run.SecondController=AttachMovable(Run.SecondObserver.Get());if(!Run.SecondController.IsValid())return false;Run.SecondController->SetNavigationEnabled(true);return Run.SecondController->IsNavigationRunning();},[]{return NavFollowing()&&NavTest.Travel>70&&Run.SecondController.IsValid()&&Run.SecondController->HasValidPedroPath()&&Run.SecondController->GetPedroMoveId().GetID()!=Run.Controller->GetPedroMoveId().GetID()&&Run.SecondObserver->GetActorLocation().X>ArenaOrigin.X+70;},.05);
    Add(TEXT("N27"),TEXT("AttentionWhileWalking"),[]{DestroyController(Run.SecondController);if(Run.SecondObserver.IsValid())Run.SecondObserver->Destroy();return NavReset(FVector::ZeroVector,FVector(1000,650,0),true,true);},[]{return NavFollowing()&&Aligned()&&NavTest.Travel>120&&NavTest.YawTravel>20;},.1);
    // Demonstration: new physical position makes previously distant source visible,
    // then an occluded event is missed while the observer continues his fixed route.
    Add(TEXT(""),TEXT("DemoBeforeMovement"),[]{if(!NavReset(FVector::ZeroVector,FVector(1200,0,0),false,true)||!BuildEvidenceRig())return false;Run.Director->DirectObservationMaxDistance=1000;return Place(Run.A.Get(),ArenaOrigin+FVector(1150,100,64))&&Place(Run.B.Get(),ArenaOrigin+FVector(1400,650,64));},[]{return NavSeen()&&TraceProp(Run.A.Get())==ETrace::TooFar&&EvidenceMatches(0,0);},.2);
    Add(TEXT(""),TEXT("DemoRelocate"),[]{return NavEnable();},[]{return NavArrived()&&TraceProp(Run.A.Get())==ETrace::Visible&&NavTest.Travel>550&&EvidenceMatches(0,0);},.2);
    Add(TEXT(""),TEXT("DemoVisibleEvent"),[]{return Inspect(Run.A.Get(),true);},[]{return EvidenceMatches(1,1)&&NavStopped();});
    Add(TEXT(""),TEXT("DemoResumeMovement"),[]{return MovePlayer(FVector(1400,600,0));},[]{return NavFollowing()&&NavTest.Travel>35;},.05);
    Add(TEXT(""),TEXT("DemoLoseSight"),[]{return NavRemember()&&WallAt(NavRoot()+FVector(390,0,0),FVector(25,1500,500))&&PlayerRayHitsWall();},[]{return Lost()&&NavFrozenRoute()&&TraceProp(Run.B.Get())==ETrace::Occluded&&EvidenceMatches(1,1);},.05,NavFrozenRoute);
    Add(TEXT(""),TEXT("DemoHiddenEvent"),[]{return MovePlayer(FVector(1400,1100,0))&&PlayerRayHitsWall()&&Inspect(Run.B.Get(),false);},[]{return Lost()&&NavFrozenRoute()&&EvidenceMatches(1,1);},.15,NavFrozenRoute);
    Add(TEXT(""),TEXT("DemoStopNoEvidence"),Noop,[]{return NavStopped()&&State().Status==EStatus::LostSight&&Run.World->GetTimeSeconds()-Run.Frozen.LastSuccessfulSightingTime>=2&&EvidenceMatches(1,1);},.4,NavFrozenRoute);
    Add(TEXT(""),TEXT("DemoReacquireNoReplay"),WallOff,[]{return NavSeen()&&NavArrived()&&NavSubmissions()>NavTest.Submissions&&EvidenceMatches(1,1)&&TraceProp(Run.B.Get())==ETrace::Visible;},.3);
    Add(TEXT("N28"),TEXT("DemoNewEvent"),[]{return Inspect(Run.B.Get(),true);},[]{return EvidenceMatches(2,2);},.3);
}
// Saved-map fixture: verify authored assets first, project real walkable starts
// and choose visible player positions. No props or saved assets are moved.
bool PrepareSavedNavigation()
{
    if(!SavedMapConfig()||!Freeze(Run.Player.Get())||!Freeze(Run.Observer.Get())||!CreateWall())return false;
    if(auto* P=Cast<ACharacter>(Run.Player.Get()))P->GetCapsuleComponent()->SetCanEverAffectNavigation(false);
    Run.Box->SetCanEverAffectNavigation(false);
    FVector Eye;FRotator Facing;Run.Observer->GetActorEyesViewPoint(Eye,Facing);
    const auto At=NavRoot();const auto Yaw=BodyYaw();
    Run.Observer->GetCapsuleComponent()->SetCanEverAffectNavigation(false);
    if(!Place(Run.Observer.Get(),At,FRotator(0,Yaw,0)))return false;
    // Preserve authored eye inclination while making the capsule upright.
    Run.Observer->GetObserverEye()->SetRelativeRotation(FRotator(Facing.Pitch,FMath::FindDeltaAngleDegrees(Yaw,Facing.Yaw),Facing.Roll));
    FBox Bounds(ForceInit);Bounds+=At;for(auto* P:{Run.A.Get(),Run.B.Get(),Run.C.Get()})Bounds+=P->GetActorLocation();
    const auto Center=Bounds.GetCenter();const auto Extent=Bounds.GetExtent()+FVector(1600,1600,400);
    return NavBounds(Center,Extent);
}
bool SavedNavigationReady()
{
    FNavLocation At;const auto* D=Cast<ARecastNavMesh>(NavData());
    if(!D||D->GetRuntimeGenerationMode()!=ERuntimeGenerationType::Dynamic||!NavProject(NavRoot(),At,FVector(150,150,250)))return false;
    if(!EvidenceMatches(0,0)||!AllOutcomesHidden())return false;
    if(!NavTest.bSavedReady)UE_LOG(LogTemp,Display,TEXT("PEDRO_NAV_TEST status=SAVED_READY original=yes dynamic=yes projected=%s"),*VectorString(At.Location));
    NavTest.bSavedReady=true;return true;
}
bool ClearSavedCapsule(const FVector& Root,double Radius,double HalfHeight)
{
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PedroSavedNavClear),false);Params.AddIgnoredActor(Run.Observer.Get());Params.AddIgnoredActor(Run.Player.Get());
    return !Run.World->OverlapBlockingTestByChannel(Root,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,HalfHeight),Params);
}
// Plan both legs against the actual saved geometry before starting the AI.
// A predicted landing is only a search hint, never a movement instruction.
struct FSavedNavLeg
{
    FVector Root=FVector::ZeroVector,Player=FVector::ZeroVector,Goal=FVector::ZeroVector,Landing=FVector::ZeroVector;
    double SeedYaw=0,LandingYaw=0,Length=0;
};
struct FSavedNavSearch
{
    int32 Starts=0,StartRejected=0,Attempts=0,Projection=0,Clearance=0,Sight=0,Path=0,Landing=0,Props=0,Seed=0,Feasible=0,PairsRejected=0;
    void Log(const TCHAR* Phase) const
    {
        UE_LOG(LogTemp,Display,TEXT("PEDRO_NAV_FIXTURE routeSearch phase=%s starts=%d startRejected=%d attempts=%d projection=%d clearance=%d sight=%d path=%d landing=%d props=%d seed=%d feasible=%d pairsRejected=%d"),
            Phase,Starts,StartRejected,Attempts,Projection,Clearance,Sight,Path,Landing,Props,Seed,Feasible,PairsRejected);
    }
};
bool SavedSightAt(const FVector& Root,double Yaw,const FVector& Target,AActor* Subject,double Range,double Cone)
{
    const auto* EyeComponent=Run.Observer->GetObserverEye();
    const FRotator Rel=EyeComponent->GetRelativeRotation();
    const FVector Eye=Root+FRotator(0,Yaw,0).RotateVector(EyeComponent->GetRelativeLocation());
    const FVector Forward=FRotator(Rel.Pitch,Yaw+Rel.Yaw,Rel.Roll).Vector();
    if(PedroDirectObservation::CheckGeometry(Eye,Forward,Target,Range,Cone)!=ETrace::Visible)return false;
    FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(PedroSavedNavRoute),false);
    Params.AddIgnoredActor(Run.Observer.Get());Params.AddIgnoredActor(Run.Player.Get());
    return !Run.World->LineTraceSingleByChannel(Hit,Eye,Target,ECC_Visibility,Params)||(Subject&&Hit.GetActor()==Subject);
}
double SavedFaceYaw(const FVector& Root,const FVector& Target)
{
    // The saved eye has zero horizontal offset; retain authored relative yaw.
    return FRotator::NormalizeAxis((Target-Root).Rotation().Yaw-Run.Observer->GetObserverEye()->GetRelativeRotation().Yaw);
}
bool SavedLeg(const FVector& Root,double OldYaw,const FVector& Player,bool bFirst,FSavedNavLeg& Out,FSavedNavSearch& Stats)
{
    ++Stats.Attempts;
    const auto* Capsule=Run.Observer->GetCapsuleComponent();
    const auto* PlayerChar=Cast<ACharacter>(Run.Player.Get());if(!PlayerChar)return false;
    const double ObserverHalf=Capsule->GetScaledCapsuleHalfHeight(),ObserverRadius=Capsule->GetScaledCapsuleRadius();
    const double PlayerHalf=PlayerChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),PlayerRadius=PlayerChar->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const FPedroNavigationSettings S{};
    const double Separation=FVector::Dist2D(Player,Root);
    if(Separation<725||Separation>1350||!ClearSavedCapsule(Player,PlayerRadius,PlayerHalf)){++Stats.Clearance;return false;}
    FNavLocation Start,Goal;
    const FVector Feet=Root-FVector(0,0,ObserverHalf);
    if(!NavProject(Feet,Start,FVector(50,50,100))||FVector::Dist2D(Feet,Start.Location)>50||FMath::Abs(Feet.Z-Start.Location.Z)>100){++Stats.Projection;return false;}
    const auto Candidates=PedroNavigationPolicy::Candidates(Root,Player,S);
    if(Candidates.IsEmpty()||!NavProject(Candidates[0],Goal,FVector(S.ProjectionXY,S.ProjectionXY,S.ProjectionZ))||
       !PedroNavigationPolicy::AcceptProjection(Candidates[0],Goal.Location,Player,S)){++Stats.Projection;return false;}
    const auto Path=NavQuery(Start.Location,Goal.Location);
    if(!Path.IsValid()||Path->GetLength()<180||Path->GetLength()>1500||Path->ContainsAnyCustomLink()){++Stats.Path;return false;}
    const auto& Points=Path->GetPathPoints();
    for(int32 J=0;J<Points.Num();++J)
    {
        if(FNavMeshNodeFlags(Points[J].Flags).IsNavLink()||Points[J].CustomNavLinkId.IsValid()||Points[J].Location.ContainsNaN()){++Stats.Path;return false;}
        if(J>0)
        {
            const auto& A=Points[J-1].Location;const auto& B=Points[J].Location;
            if(FMath::PointDistToSegment(FVector(Player.X,Player.Y,0),FVector(A.X,A.Y,0),FVector(B.X,B.Y,0))<S.PersonalSpace){++Stats.Path;return false;}
        }
    }
    // This integration fixture needs predictable landing geometry, not another
    // detour test (N12 already proves that). Prefer a nearly straight native
    // path and a direct stand-off goal with enough room to stop naturally.
    if(Path->GetLength()>FVector::Dist2D(Start.Location,Goal.Location)+30||FVector::Dist2D(Goal.Location,Player)>470||
       FVector::Dist2D(Candidates[0],Goal.Location)>30||FMath::Abs(Goal.Location.Z-Start.Location.Z)>8){++Stats.Path;return false;}
    double SeedYaw=OldYaw;
    if(bFirst)
    {
        bool bSeed=false;
        for(double Offset:{25.,-25.,15.,-15.})
        {
            const double Yaw=FRotator::NormalizeAxis(SavedFaceYaw(Root,Player)+Offset);
            if(SavedSightAt(Root,Yaw,Player,nullptr,1500,70)){SeedYaw=Yaw;bSeed=true;break;}
        }
        if(!bSeed){++Stats.Seed;return false;}
    }
    else if(!SavedSightAt(Root,OldYaw,Player,nullptr,1500,70)){++Stats.Sight;return false;}
    // Bracket the 500 cm cancellation boundary on the straight path. Check
    // real capsule clearance, walkable ground, sight, and all original props
    // throughout the plausible stopping envelope; actual motion is tested later.
    const FVector GoalRoot=Goal.Location+FVector(0,0,ObserverHalf+2);
    const FVector GroundRoot(Start.Location.X,Start.Location.Y,Start.Location.Z+ObserverHalf+2);
    const FVector Direction=(GoalRoot-GroundRoot).GetSafeNormal2D();
    if(Direction.IsNearlyZero()){++Stats.Landing;return false;}
    for(double Stop:{480.,500.,520.})
    {
        double Lo=0,Hi=1;
        for(int32 I=0;I<24;++I)
        {
            const double Mid=(Lo+Hi)*.5;
            if(FVector::Dist2D(FMath::Lerp(GroundRoot,GoalRoot,Mid),Player)>Stop)Lo=Mid;else Hi=Mid;
        }
        const FVector Future=FMath::Lerp(GroundRoot,GoalRoot,(Lo+Hi)*.5);
        if(FVector::Dist2D(Root,Future)<=150){++Stats.Landing;return false;}
        FNavLocation Floor;
        if(!NavProject(Future-FVector(0,0,ObserverHalf),Floor,FVector(40,40,100))||
           FVector::Dist2D(Floor.Location,Future)>10||!ClearSavedCapsule(Future,ObserverRadius,ObserverHalf)){++Stats.Landing;return false;}
        const double Face=SavedFaceYaw(Future,Player);
        if(!SavedSightAt(Future,Face,Player,nullptr,1500,70)){++Stats.Sight;return false;}
        for(auto* Prop:{Run.A.Get(),Run.B.Get(),Run.C.Get()})
            if(!SavedSightAt(Future,Face,Prop->GetActorLocation(),Prop,Run.Director->DirectObservationMaxDistance,68)){++Stats.Props;return false;}
        if(Stop==500){Out.Landing=Future;Out.LandingYaw=Face;}
    }
    Out.Root=Root;Out.Player=Player;Out.Goal=Goal.Location;Out.SeedYaw=SeedYaw;Out.Length=Path->GetLength();++Stats.Feasible;return true;
}
bool SavedProjectedPlayer(const FVector& At,FVector& Player,FSavedNavSearch& Stats)
{
    const auto* C=Cast<ACharacter>(Run.Player.Get());FNavLocation P;
    if(!C||!NavProject(At,P,FVector(75,75,220))||FVector::Dist2D(P.Location,At)>75){++Stats.Projection;return false;}
    Player=P.Location+FVector(0,0,C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2);return true;
}
bool FindSavedSecond(const FVector& Root,double Yaw,const FVector& PreviousPlayer,FSavedNavLeg& Out,FSavedNavSearch& Stats)
{
    for(double Angle:{0.,15.,-15.,25.,-25.,40.,-40.,55.,-55.,65.,-65.})
    for(double Distance:{800.,750.,900.,1000.,1100.,1250.})
    {
        FVector Player;
        if(!SavedProjectedPlayer(Root+FRotator(0,Yaw+Angle,0).Vector()*Distance,Player,Stats))continue;
        if(FVector::Dist2D(Player,PreviousPlayer)<125){++Stats.Clearance;continue;}
        if(SavedLeg(Root,Yaw,Player,false,Out,Stats))return true;
    }
    return false;
}
void LogSavedLeg(const TCHAR* Phase,const FSavedNavLeg& Leg)
{
    UE_LOG(LogTemp,Display,TEXT("PEDRO_NAV_FIXTURE savedRoute phase=%s start=%s player=%s goal=%s length=%.3f seedYaw=%.3f predictedLanding=%s landingYaw=%.3f"),
        Phase,*VectorString(Leg.Root),*VectorString(Leg.Player),*VectorString(Leg.Goal),Leg.Length,Leg.SeedYaw,*VectorString(Leg.Landing),Leg.LandingYaw);
}
bool SavedRoutePosition(bool bFirst)
{
    if(!Run.Observer.IsValid()||!Run.Player.IsValid()||!Run.A.IsValid()||!Run.B.IsValid()||!Run.C.IsValid())return false;
    const FVector OldRoot=NavRoot();const double OldYaw=BodyYaw();
    FSavedNavSearch FirstStats,SecondStats;
    if(bFirst)
    {
        NavTest.bSavedPair=false;
        const auto* Capsule=Run.Observer->GetCapsuleComponent();
        const double Half=Capsule->GetScaledCapsuleHalfHeight(),Radius=Capsule->GetScaledCapsuleRadius();
        // Start farther back when possible, reserving space for a second leg.
        // Search is bounded and converges before any fixture actor is moved.
        for(double X:{-300.,-450.,-600.,-150.,0.,150.,300.})
        for(double Y:{0.,-150.,150.,-300.,300.})
        {
            ++FirstStats.Starts;FNavLocation Start;
            if(!NavProject(OldRoot+FRotator(0,OldYaw,0).RotateVector(FVector(X,Y,0)),Start,FVector(90,90,250))){++FirstStats.StartRejected;continue;}
            const FVector Root=Start.Location+FVector(0,0,Half+2);
            if(!ClearSavedCapsule(Root,Radius,Half)){++FirstStats.StartRejected;continue;}
            for(double Angle:{0.,15.,-15.,25.,-25.,40.,-40.,55.,-55.})
            for(double Distance:{800.,750.,900.,1000.,1100.,1250.})
            {
                FVector Player;FSavedNavLeg First,Second;
                if(!SavedProjectedPlayer(Root+FRotator(0,OldYaw+Angle,0).Vector()*Distance,Player,FirstStats)||!SavedLeg(Root,OldYaw,Player,true,First,FirstStats))continue;
                if(!FindSavedSecond(First.Landing,First.LandingYaw,First.Player,Second,SecondStats)){++FirstStats.PairsRejected;continue;}
                // Only initial setup may place the observer, prior to attaching AI.
                if(!Place(Run.Observer.Get(),Root,FRotator(0,First.SeedYaw,0))||!Place(Run.Player.Get(),Player))return false;
                NavTest.bSavedPair=true;NavTest.SavedSecondPlayer=Second.Player;NavTest.SavedSecondGoal=Second.Goal;NavTest.SavedSecondLanding=Second.Landing;
                LogSavedLeg(TEXT("first"),First);LogSavedLeg(TEXT("planned-second"),Second);
                FirstStats.Log(TEXT("first"));SecondStats.Log(TEXT("lookahead"));return true;
            }
        }
        FirstStats.Log(TEXT("first"));SecondStats.Log(TEXT("lookahead"));
    }
    else
    {
        if(!NavTest.bSavedPair||!NavArrived()||!Aligned())
        {
            UE_LOG(LogTemp,Error,TEXT("PEDRO_NAV_FIXTURE secondRoutePrecondition pair=%s arrived=%s aligned=%s awareness=%s native=%s"),
                Yes(NavTest.bSavedPair),Yes(NavArrived()),Yes(Aligned()),StatusName(State().Status),Yes(Run.Controller.IsValid()&&Run.Controller->HasNativeSight(Run.Player.Get())));
            return false;
        }
        const FVector PreviousPlayer=Run.Player->GetActorLocation();FSavedNavLeg Second;
        // Revalidate against the actual stopping position; never move or reset
        // Pedro to fit a predicted start. Search again only if the hint fails.
        const bool bPlanned=FVector::Dist2D(PreviousPlayer,NavTest.SavedSecondPlayer)>=125&&SavedLeg(OldRoot,OldYaw,NavTest.SavedSecondPlayer,false,Second,SecondStats);
        if(bPlanned||FindSavedSecond(OldRoot,OldYaw,PreviousPlayer,Second,SecondStats))
        {
            NavTest.SavedSecondPlayer=Second.Player;NavTest.SavedSecondGoal=Second.Goal;NavTest.SavedSecondLanding=Second.Landing;
            NavTest.SavedRelocationRoot=OldRoot;NavTest.SavedRelocationSubmissions=NavSubmissions();
            if(!Place(Run.Player.Get(),Second.Player))return false;
            LogSavedLeg(TEXT("actual-second"),Second);SecondStats.Log(TEXT("actual-second"));return true;
        }
        SecondStats.Log(TEXT("actual-second"));
    }
    UE_LOG(LogTemp,Error,TEXT("PEDRO_NAV_FIXTURE no suitable saved-map two-leg route phase=%s observer=%s player=%s statue=%s radio=%s lantern=%s; no asset edits attempted"),
        bFirst?TEXT("first/lookahead"):TEXT("actual-second"),*VectorString(OldRoot),*VectorString(Run.Player->GetActorLocation()),*VectorString(Run.A->GetActorLocation()),*VectorString(Run.B->GetActorLocation()),*VectorString(Run.C->GetActorLocation()));
    return false;
}
bool SavedAllVisible(){return TraceProp(Run.A.Get())==ETrace::Visible&&TraceProp(Run.B.Get())==ETrace::Visible&&TraceProp(Run.C.Get())==ETrace::Visible;}
void MakeNavigationIntegrationSteps()
{
    Add(TEXT("K01"),TEXT("SavedConfigurationAndNavmesh"),PrepareSavedNavigation,SavedNavigationReady,.25);
    Add(TEXT("K02"),TEXT("SavedAutonomousFollow"),[]{if(!SavedRoutePosition(true))return false;Run.Controller=AttachMovable(Run.Observer.Get());if(!Run.Controller.IsValid())return false;Run.Controller->SetAttentionEnabled(true);return NavEnable();},[]{return NavArrived()&&Aligned()&&NavTest.Travel>150&&NavTest.YawTravel>5&&SavedAllVisible()&&EvidenceMatches(0,0);},.3);
    Add(TEXT("K03"),TEXT("SavedStatue"),[]{return Inspect(Run.A.Get(),true);},[]{return NavStopped()&&EvidenceMatches(1,1)&&AllOutcomesHidden();});
    Add(TEXT("K04"),TEXT("SavedRadioOccluded"),[]{return WallForProp(Run.B.Get(),30)&&Inspect(Run.B.Get(),false);},[]{return EvidenceMatches(1,1)&&AllOutcomesHidden();},.25);
    // Clearing collision is synchronous; reacquiring native sight is not.
    // Keep the existing player, observer, controller and evidence stationary
    // until a genuine new sighting and tracking decision have settled.
    Add(TEXT(""),TEXT("SavedRestoreSightNoReplay"),[]{
        if(!NavTest.bSavedPair||!Run.Player.IsValid()||!Run.Controller.IsValid()||!NavStopped()||!EvidenceMatches(1,1)||!AllOutcomesHidden())return false;
        NavTest.SavedRestorePlayer=Run.Player->GetActorLocation();
        NavTest.SavedRestoreSightingTime=State().LastSuccessfulSightingTime;
        NavTest.SavedRestoreSubmissions=NavSubmissions();
        return WallOff();
    },[]{return NavArrived()&&Aligned()&&State().LastSuccessfulSightingTime>NavTest.SavedRestoreSightingTime&&
        Run.World->GetTimeSeconds()-State().LastSuccessfulSightingTime<=.35&&SavedAllVisible();
    },.25,[]{return Run.Player.IsValid()&&Run.Controller.IsValid()&&Run.Player->GetActorLocation().Equals(NavTest.SavedRestorePlayer,.01)&&
        NavStopped()&&NavSubmissions()==NavTest.SavedRestoreSubmissions&&NavTest.Travel<2&&EvidenceMatches(1,1)&&AllOutcomesHidden();});
    Add(TEXT(""),TEXT("SavedRelocationNoReplay"),[]{return SavedRoutePosition(false);},[]{return NavArrived()&&Aligned()&&NavTest.Travel>150&&FVector::Dist2D(NavRoot(),NavTest.SavedRelocationRoot)>150&&NavSubmissions()>NavTest.SavedRelocationSubmissions&&EvidenceMatches(1,1)&&SavedAllVisible();},.3);
    Add(TEXT("K05"),TEXT("SavedNewRadio"),[]{return Inspect(Run.B.Get(),true);},[]{return EvidenceMatches(2,2)&&AllOutcomesHidden()&&NavStopped();});
    Add(TEXT("K06"),TEXT("SavedLanternSeeking"),[]{return EvidenceMatches(2,2)&&AllOutcomesHidden()&&Inspect(Run.C.Get(),true)&&EvidenceMatches(3,3)&&AllOutcomesHidden();},FinalPresentation,.5);
}

#include "PedroContextWorldTests.inl"

int32 ExpectedCases() { return Run.bContext?(Run.bContextProbe?4:Run.bIntegration?6:30):Run.bNavigation?(Run.bNavProbe?2:(Run.bIntegration?6:28)):(Run.bIntegration?6:(Run.bAttention?24:25)); }
const TCHAR* SuiteName() { return Run.bContext?ContextSuite():Run.bNavigation?NavSuite():Run.bAttention?AttentionSuite():(Run.bIntegration?TEXT("integration"):TEXT("world")); }
bool Finish(const TCHAR* Error=nullptr)
{
    if (Error)
    {
        if(Run.bNavigation)
        {
            const auto* Data=NavData();
            UE_LOG(LogTemp,Error,TEXT("PEDRO_NAV_FIXTURE failure navSystem=%s navData=%s dynamic=%s capability=%s savedReady=%s"),
                *GetNameSafe(NavSystem()),*GetNameSafe(Data),Yes(Data&&Data->GetRuntimeGenerationMode()==ERuntimeGenerationType::Dynamic),Yes(NavTest.bCapability),Yes(NavTest.bSavedReady));
            if(Run.bIntegration&&NavTest.bSavedReady&&Run.Observer.IsValid()&&Run.Player.IsValid()&&Run.A.IsValid()&&Run.B.IsValid()&&Run.C.IsValid())
            {
                UE_LOG(LogTemp,Error,TEXT("PEDRO_NAV_FIXTURE savedChecks arrived=%s aligned=%s travel=%.3f yawTravel=%.3f statueTrace=%d radioTrace=%d lanternTrace=%d evidence=%d sampled=%d"),
                    Yes(NavArrived()),Yes(Aligned()),NavTest.Travel,NavTest.YawTravel,int32(TraceProp(Run.A.Get())),int32(TraceProp(Run.B.Get())),int32(TraceProp(Run.C.Get())),EvidenceCount(),SampledCount());
            }
        }
        UE_LOG(LogTemp,Error,TEXT("PEDRO_AWARENESS_TEST status=FAIL index=%d reason=%s"),Run.Index,Error);
        if (Run.Steps.IsValidIndex(Run.Index)) { if(Run.bContext)LogContextSample(TEXT("DIAGNOSTIC"),Run.Steps[Run.Index]);else if(Run.bNavigation)LogNavSample(TEXT("DIAGNOSTIC"),Run.Steps[Run.Index]);else if(Run.bAttention) LogAttentionSample(TEXT("DIAGNOSTIC"),Run.Steps[Run.Index]);else LogStep(TEXT("DIAGNOSTIC"),Run.Steps[Run.Index]); }
        LogFixtureMotion(TEXT("Observer"), Run.Observer.Get());
        LogFixtureMotion(TEXT("Player"), Run.Player.Get());
        LogFixtureMotion(TEXT("SecondObserver"), Run.SecondObserver.Get());
        LogFixtureMotion(TEXT("Distractor"), Run.Distractor.Get());
    }
    const int32 Expected=ExpectedCases();
    if (Run.Box.IsValid()) Run.Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (Run.Blocker.IsValid()) Run.Blocker->Destroy();
    if(Run.bContext){DestroyContextPoints();CleanContextOther();}
    DestroyController(Run.Controller); DestroyController(Run.SecondController);
    if (!Run.bIntegration)
    {
        if (Run.Director.IsValid()) Run.Director->Destroy();
        if (Run.A.IsValid()) Run.A->Destroy();
        if (Run.B.IsValid()) Run.B->Destroy();
        if (Run.Observer.IsValid()) Run.Observer->Destroy();
        if (Run.SecondObserver.IsValid()) Run.SecondObserver->Destroy();
        if (Run.Distractor.IsValid()) Run.Distractor->Destroy();
    }
    if(Run.bNavigation)
    {
        for(const auto& A:NavTest.Geometry)if(A.IsValid())A->Destroy();
        if(NavTest.Bounds.IsValid())NavTest.Bounds->Destroy();
        if(Run.bContext)
        {UE_LOG(LogTemp,Display,TEXT("PEDRO_CONTEXT_TEST status=SUMMARY suite=%s passed=%d expected=%d result=%s"),SuiteName(),Run.Passed,Expected,!Error&&Run.Passed==Expected?TEXT("PASS"):TEXT("FAIL"));}
        else
        {UE_LOG(LogTemp,Display,TEXT("PEDRO_NAV_TEST status=SUMMARY suite=%s passed=%d expected=%d result=%s"),SuiteName(),Run.Passed,Expected,!Error&&Run.Passed==Expected?TEXT("PASS"):TEXT("FAIL"));}
    }
    else UE_LOG(LogTemp,Display,TEXT("PEDRO_AWARENESS_TEST status=SUMMARY suite=%s passed=%d expected=%d result=%s"),
        SuiteName(),Run.Passed,Expected,!Error&&Run.Passed==Expected?TEXT("PASS"):TEXT("FAIL"));
    FPlatformMisc::RequestExit(false);
    return false;
}
bool Tick(float)
{
    const bool bWorld=FParse::Param(FCommandLine::Get(),TEXT("PedroAwarenessWorld"));
    const bool bIntegration=FParse::Param(FCommandLine::Get(),TEXT("PedroAwarenessIntegration"));
    const bool bAttentionWorld=FParse::Param(FCommandLine::Get(),TEXT("PedroAttentionWorld"));
    const bool bAttentionIntegration=FParse::Param(FCommandLine::Get(),TEXT("PedroAttentionIntegration"));
    const bool bNavWorld=FParse::Param(FCommandLine::Get(),TEXT("PedroNavigationWorld"));
    const bool bNavIntegration=FParse::Param(FCommandLine::Get(),TEXT("PedroNavigationIntegration"));
    const bool bNavProbe=FParse::Param(FCommandLine::Get(),TEXT("PedroNavigationProbe"));
    const bool bContextWorld=FParse::Param(FCommandLine::Get(),TEXT("PedroContextWorld"));
    const bool bContextIntegration=FParse::Param(FCommandLine::Get(),TEXT("PedroContextIntegration"));
    const bool bContextProbe=FParse::Param(FCommandLine::Get(),TEXT("PedroContextProbe"));
    const int32 Invocations=int32(bContextWorld)+int32(bContextIntegration)+int32(bContextProbe)+int32(bWorld)+int32(bIntegration)+int32(bAttentionWorld)+int32(bAttentionIntegration)+int32(bNavWorld)+int32(bNavIntegration)+int32(bNavProbe);
    if (!Invocations) return false;
    if (!GEngine) return true;
    if (!Run.bBegan)
    {
        UWorld* World=nullptr;
        for (const auto& Context:GEngine->GetWorldContexts())
        {
            UWorld* Candidate=Context.World();
            if (Candidate && Candidate->WorldType==EWorldType::Game && Candidate->HasBegunPlay() &&
                Candidate->GetOutermost()->GetName()==Map) {World=Candidate;break;}
        }
        if (!World || !World->GetFirstPlayerController() || !World->GetFirstPlayerController()->GetPawn()) return true;
        Run.bBegan=true; Run.bContext=bContextWorld||bContextIntegration||bContextProbe;Run.bContextProbe=bContextProbe;Run.bIntegration=bIntegration||bAttentionIntegration||bNavIntegration||bContextIntegration; Run.bAttention=bAttentionWorld||bAttentionIntegration; Run.bNavigation=bNavWorld||bNavIntegration||bNavProbe||Run.bContext; Run.bNavProbe=bNavProbe; Run.World=World;
        Run.PC=World->GetFirstPlayerController(); Run.Player=Run.PC->GetPawn();
        if(Run.bContext)
        {UE_LOG(LogTemp,Display,TEXT("PEDRO_CONTEXT_TEST status=BEGIN suite=%s map=%s expected=%d"),SuiteName(),Map,ExpectedCases());}
        else if (Run.bNavigation)
        {
            UE_LOG(LogTemp,Display,TEXT("PEDRO_NAV_TEST status=BEGIN suite=%s map=%s expected=%d"),
                SuiteName(),Map,ExpectedCases());
        }
        else
        {
            UE_LOG(LogTemp,Display,TEXT("PEDRO_AWARENESS_TEST status=BEGIN suite=%s map=%s expected=%d"),
                SuiteName(),Map,ExpectedCases());
        }
        if (Invocations!=1 || !Run.Player->IsPlayerControlled() || !Progress()) return Finish(TEXT("InvalidInvocationOrPlayer"));
        Run.InitialProgress=Progress()->GetResolvedEncounterCount();
        if (Run.InitialProgress!=0) return Finish(TEXT("ProgressNotFresh"));
        if (Run.bIntegration) {if(Run.bContext)MakeContextIntegrationSteps();else if(Run.bNavigation)MakeNavigationIntegrationSteps();else if(Run.bAttention) MakeAttentionIntegrationSteps();else MakeIntegrationSteps();}
        else
        {
            if (!Freeze(Run.Player.Get())) return Finish(TEXT("PlayerFreezeFailed"));
            Run.Observer=SpawnObserver(ArenaOrigin,FRotator::ZeroRotator);
            if (!Run.Observer.IsValid() || !CreateWall() || !TurnObserver(0)) return Finish(TEXT("ArenaSetupFailed"));
            if(Run.bContext)MakeContextSteps();else if(Run.bNavigation)MakeNavigationSteps();else if(Run.bAttention) MakeAttentionSteps();else MakeWorldSteps();
        }
    }
    if (!Run.World.IsValid() || !Run.PC.IsValid()) return Finish(TEXT("WorldOrPlayerControllerLost"));
    if (!Run.Steps.IsValidIndex(Run.Index)) return Finish();
    const FStep& Step=Run.Steps[Run.Index];
    if (!Run.bSetup)
    {
        if(Run.bNavigation&&Run.Index>0&&!GuardNav(Run.Steps[Run.Index-1],false))return Finish(TEXT("BetweenNavigationStepInvariant"));
        if(Run.bAttention && Run.Index>0 && !GuardAttention(Run.Steps[Run.Index-1],false)) return Finish(TEXT("BetweenStepPoseOrRateInvariant"));
        Run.bSetup=true; Run.StartTime=FPlatformTime::Seconds(); Run.StableTime=0; Run.StableFrames=0;
        if (!Step.Setup()) return Finish(TEXT("SetupFailed"));
        if(Run.bContext){CaptureNavPose();CaptureContextPose();LogContextSample(TEXT("STEP_BEGIN"),Step);}
        else if(Run.bNavigation){CaptureNavPose();LogNavSample(TEXT("STEP_BEGIN"),Step);}
        else if(Run.bAttention){CaptureAttentionPose();LogAttentionSample(TEXT("STEP_BEGIN"),Step);}
        else LogStep(TEXT("STEP_BEGIN"),Step);
        return true; // Never accept a setup in the same tick that configured it.
    }
    const double Now=FPlatformTime::Seconds();
    if(Run.bContext)
    {
        const double Dt=Run.World->GetTimeSeconds()-NavTest.PrevTime;
        if(!GuardNav(Step,false)||!GuardContext())return Finish(TEXT("ContextPoseRateOrOwnershipInvariant"));
        if(Dt>0)LogContextSample(TEXT("FRAME"),Step,Dt);
    }
    else if (Run.bNavigation && !GuardNav(Step))return Finish(TEXT("NavigationPoseRateOrOwnershipInvariant"));
    if (Run.bAttention && !GuardAttention(Step)) return Finish(TEXT("AttentionPoseOrRateInvariant"));
    if (Step.Guard && !Step.Guard()) return Finish(TEXT("InvariantViolated"));
    if (Step.Check())
    {
        const double HoldClock=(Run.bAttention||Run.bNavigation)?Run.World->GetTimeSeconds():Now;
        if (Run.StableFrames++==0) Run.StableTime=HoldClock;
        if (Run.StableFrames>=4 && HoldClock-Run.StableTime>=Step.Hold)
        {
            if(Run.bContext)LogContextSample(Step.Case.IsEmpty()?TEXT("CHECK"):TEXT("PASS"),Step);
            else if(Run.bNavigation)LogNavSample(Step.Case.IsEmpty()?TEXT("CHECK"):TEXT("PASS"),Step);
            else if(Run.bAttention)LogAttentionSample(Step.Case.IsEmpty()?TEXT("CHECK"):TEXT("PASS"),Step);
            else LogStep(Step.Case.IsEmpty()?TEXT("CHECK"):TEXT("PASS"),Step);
            if (!Step.Case.IsEmpty()) ++Run.Passed;
            ++Run.Index; Run.bSetup=false;
            return true;
        }
    }
    else {Run.StableFrames=0;Run.StableTime=0;}
    if (Now-Run.StartTime>(Run.bNavigation?35.0:8.0)) return Finish(TEXT("DeadlineExceeded"));
    return true;
}
// This is the same process-ticker mechanism proven by the B2b-5b2 mechanical
// occlusion fixture. Normal launches immediately remove the callback.
[[maybe_unused]] const auto PedroAwarenessTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
}
#endif
