// Opt-in standalone Unreal Editor game-process tests. No asset writes.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
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
    bool bSetup = false, bIntegration = false, bBegan = false, bAttention = false;
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

int32 ExpectedCases() { return Run.bIntegration?6:(Run.bAttention?24:25); }
const TCHAR* SuiteName() { return Run.bAttention?AttentionSuite():(Run.bIntegration?TEXT("integration"):TEXT("world")); }
bool Finish(const TCHAR* Error=nullptr)
{
    if (Error)
    {
        UE_LOG(LogTemp,Error,TEXT("PEDRO_AWARENESS_TEST status=FAIL index=%d reason=%s"),Run.Index,Error);
        if (Run.Steps.IsValidIndex(Run.Index)) { if(Run.bAttention) LogAttentionSample(TEXT("DIAGNOSTIC"),Run.Steps[Run.Index]);else LogStep(TEXT("DIAGNOSTIC"),Run.Steps[Run.Index]); }
        LogFixtureMotion(TEXT("Observer"), Run.Observer.Get());
        LogFixtureMotion(TEXT("Player"), Run.Player.Get());
        LogFixtureMotion(TEXT("SecondObserver"), Run.SecondObserver.Get());
        LogFixtureMotion(TEXT("Distractor"), Run.Distractor.Get());
    }
    const int32 Expected=ExpectedCases();
    if (Run.Box.IsValid()) Run.Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (Run.Blocker.IsValid()) Run.Blocker->Destroy();
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
    UE_LOG(LogTemp,Display,TEXT("PEDRO_AWARENESS_TEST status=SUMMARY suite=%s passed=%d expected=%d result=%s"),
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
    const int32 Invocations=int32(bWorld)+int32(bIntegration)+int32(bAttentionWorld)+int32(bAttentionIntegration);
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
        Run.bBegan=true; Run.bIntegration=bIntegration||bAttentionIntegration; Run.bAttention=bAttentionWorld||bAttentionIntegration; Run.World=World;
        Run.PC=World->GetFirstPlayerController(); Run.Player=Run.PC->GetPawn();
        UE_LOG(LogTemp,Display,TEXT("PEDRO_AWARENESS_TEST status=BEGIN suite=%s map=%s expected=%d"),
            SuiteName(),Map,ExpectedCases());
        if (Invocations!=1 || !Run.Player->IsPlayerControlled() || !Progress()) return Finish(TEXT("InvalidInvocationOrPlayer"));
        Run.InitialProgress=Progress()->GetResolvedEncounterCount();
        if (Run.InitialProgress!=0) return Finish(TEXT("ProgressNotFresh"));
        if (Run.bIntegration) {if(Run.bAttention) MakeAttentionIntegrationSteps();else MakeIntegrationSteps();}
        else
        {
            if (!Freeze(Run.Player.Get())) return Finish(TEXT("PlayerFreezeFailed"));
            Run.Observer=SpawnObserver(ArenaOrigin,FRotator::ZeroRotator);
            if (!Run.Observer.IsValid() || !CreateWall() || !TurnObserver(0)) return Finish(TEXT("ArenaSetupFailed"));
            if(Run.bAttention) MakeAttentionSteps();else MakeWorldSteps();
        }
    }
    if (!Run.World.IsValid() || !Run.PC.IsValid()) return Finish(TEXT("WorldOrPlayerControllerLost"));
    if (!Run.Steps.IsValidIndex(Run.Index)) return Finish();
    const FStep& Step=Run.Steps[Run.Index];
    if (!Run.bSetup)
    {
        if(Run.bAttention && Run.Index>0 && !GuardAttention(Run.Steps[Run.Index-1],false)) return Finish(TEXT("BetweenStepPoseOrRateInvariant"));
        Run.bSetup=true; Run.StartTime=FPlatformTime::Seconds(); Run.StableTime=0; Run.StableFrames=0;
        if (!Step.Setup()) return Finish(TEXT("SetupFailed"));
        if(Run.bAttention){CaptureAttentionPose();LogAttentionSample(TEXT("STEP_BEGIN"),Step);}
        else LogStep(TEXT("STEP_BEGIN"),Step);
        return true; // Never accept a setup in the same tick that configured it.
    }
    const double Now=FPlatformTime::Seconds();
    if (Run.bAttention && !GuardAttention(Step)) return Finish(TEXT("AttentionPoseOrRateInvariant"));
    if (Step.Guard && !Step.Guard()) return Finish(TEXT("InvariantViolated"));
    if (Step.Check())
    {
        const double HoldClock=Run.bAttention?Run.World->GetTimeSeconds():Now;
        if (Run.StableFrames++==0) Run.StableTime=HoldClock;
        if (Run.StableFrames>=4 && HoldClock-Run.StableTime>=Step.Hold)
        {
            if(Run.bAttention)LogAttentionSample(Step.Case.IsEmpty()?TEXT("CHECK"):TEXT("PASS"),Step);
            else LogStep(Step.Case.IsEmpty()?TEXT("CHECK"):TEXT("PASS"),Step);
            if (!Step.Case.IsEmpty()) ++Run.Passed;
            ++Run.Index; Run.bSetup=false;
            return true;
        }
    }
    else {Run.StableFrames=0;Run.StableTime=0;}
    if (Now-Run.StartTime>8.0) return Finish(TEXT("DeadlineExceeded"));
    return true;
}
// This is the same process-ticker mechanism proven by the B2b-5b2 mechanical
// occlusion fixture. Normal launches immediately remove the callback.
[[maybe_unused]] const auto PedroAwarenessTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
}
#endif
