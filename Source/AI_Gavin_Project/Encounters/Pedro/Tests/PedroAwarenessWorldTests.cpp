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
    bool bSetup = false, bIntegration = false, bBegan = false;
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
bool Inspect(APedroPrototypeInteractableActor* Prop, bool bVisible)
{
    auto* S = Source(Prop);
    if (!IsValid(S) || !S->bEvidenceEnabled || !Run.Player.IsValid() || !Run.Player->IsPlayerControlled() ||
        TraceProp(Prop) != (bVisible ? ETrace::Visible : ETrace::Occluded)) return false;
    if (!bVisible)
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
        bVisible?TEXT("Visible"):TEXT("Occluded"),Before+1);
    return true;
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

bool Finish(const TCHAR* Error=nullptr)
{
    if (Error)
    {
        UE_LOG(LogTemp,Error,TEXT("PEDRO_AWARENESS_TEST status=FAIL index=%d reason=%s"),Run.Index,Error);
        if (Run.Steps.IsValidIndex(Run.Index)) LogStep(TEXT("DIAGNOSTIC"),Run.Steps[Run.Index]);
        LogFixtureMotion(TEXT("Observer"), Run.Observer.Get());
        LogFixtureMotion(TEXT("Player"), Run.Player.Get());
        LogFixtureMotion(TEXT("SecondObserver"), Run.SecondObserver.Get());
        LogFixtureMotion(TEXT("Distractor"), Run.Distractor.Get());
    }
    const int32 Expected=Run.bIntegration?6:25;
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
        Run.bIntegration?TEXT("integration"):TEXT("world"),Run.Passed,Expected,!Error&&Run.Passed==Expected?TEXT("PASS"):TEXT("FAIL"));
    FPlatformMisc::RequestExit(false);
    return false;
}
bool Tick(float)
{
    const bool bWorld=FParse::Param(FCommandLine::Get(),TEXT("PedroAwarenessWorld"));
    const bool bIntegration=FParse::Param(FCommandLine::Get(),TEXT("PedroAwarenessIntegration"));
    if (!bWorld && !bIntegration) return false;
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
        Run.bBegan=true; Run.bIntegration=bIntegration; Run.World=World;
        Run.PC=World->GetFirstPlayerController(); Run.Player=Run.PC->GetPawn();
        UE_LOG(LogTemp,Display,TEXT("PEDRO_AWARENESS_TEST status=BEGIN suite=%s map=%s expected=%d"),
            bIntegration?TEXT("integration"):TEXT("world"),Map,bIntegration?6:25);
        if (bWorld==bIntegration || !Run.Player->IsPlayerControlled() || !Progress()) return Finish(TEXT("InvalidInvocationOrPlayer"));
        Run.InitialProgress=Progress()->GetResolvedEncounterCount();
        if (Run.InitialProgress!=0) return Finish(TEXT("ProgressNotFresh"));
        if (bIntegration) MakeIntegrationSteps();
        else
        {
            if (!Freeze(Run.Player.Get())) return Finish(TEXT("PlayerFreezeFailed"));
            Run.Observer=SpawnObserver(ArenaOrigin,FRotator::ZeroRotator);
            if (!Run.Observer.IsValid() || !CreateWall() || !TurnObserver(0)) return Finish(TEXT("ArenaSetupFailed"));
            MakeWorldSteps();
        }
    }
    if (!Run.World.IsValid() || !Run.PC.IsValid()) return Finish(TEXT("WorldOrPlayerControllerLost"));
    if (!Run.Steps.IsValidIndex(Run.Index)) return Finish();
    const FStep& Step=Run.Steps[Run.Index];
    if (!Run.bSetup)
    {
        Run.bSetup=true; Run.StartTime=FPlatformTime::Seconds(); Run.StableTime=0; Run.StableFrames=0;
        if (!Step.Setup()) return Finish(TEXT("SetupFailed"));
        LogStep(TEXT("STEP_BEGIN"),Step);
        return true; // Never accept a setup in the same tick that configured it.
    }
    const double Now=FPlatformTime::Seconds();
    if (Step.Guard && !Step.Guard()) return Finish(TEXT("InvariantViolated"));
    if (Step.Check())
    {
        if (Run.StableFrames++==0) Run.StableTime=Now;
        if (Run.StableFrames>=4 && Now-Run.StableTime>=Step.Hold)
        {
            LogStep(Step.Case.IsEmpty()?TEXT("CHECK"):TEXT("PASS"),Step);
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
