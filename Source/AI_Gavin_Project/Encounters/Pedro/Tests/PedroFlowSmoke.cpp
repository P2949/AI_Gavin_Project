// B2b-6f opt-in real-world flow: hub -> Pedro -> hub -> Pedro.
// This fixture is absent from Shipping. It changes only the TEST PLAYER's
// one-off position/aim; no Pedro movement, synthetic knowledge or forced travel.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavMesh/RecastNavMesh.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#include "Player/MainPlayerCharacter.h"
#include "Player/Interaction/PlayerInteractionComponent.h"
#include "World/Encounters/EncounterDefinition.h"
#include "World/Encounters/EncounterEntranceActor.h"
#include "World/Encounters/EncounterProgressSubsystem.h"
#include "Encounters/Pedro/Tests/PedroSmokeVisibility.h"
#include "Encounters/Pedro/PedroEncounterDirector.h"
#include "Encounters/Pedro/PedroObserverCharacter.h"
#include "Encounters/Pedro/PedroObserverAIController.h"
#include "Encounters/Pedro/PedroPlayableIntegrationActor.h"
#include "Encounters/Pedro/PedroPrototypeInteractableActor.h"
#include "Encounters/Pedro/PedroPrototypeReturnVolume.h"
#include "Encounters/Pedro/PedroObservationSourceComponent.h"
#include "Encounters/Pedro/Tests/PedroFlowResolvedProbe.h"

namespace PedroFlowSmoke
{
constexpr const TCHAR* ProductionPedro = TEXT("/Game/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype");
constexpr const TCHAR* ProductionHub = TEXT("/Game/AI_Gavin/World/Hub/Maps/L_Hub_Prototype");
constexpr const TCHAR* IsolatedPedro = TEXT("/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest");
constexpr const TCHAR* IsolatedHub = TEXT("/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Hub_Pedro_AutonomousTest");

enum class EPhase : uint8
{
    InitialHub, AimAtEntrance, WaitPedro, PedroSetup, ChooseInteraction,
    WaitPedroSight, AimAtProp, WaitActionEvaluation, WaitSatisfied, ReturnToHub, WaitHub,
    CheckHub, ReenterFromHub, WaitReentered, VerifyReentered
};

struct FState
{
    bool bStarted = false;
    bool bEarly = false;
    bool bReentry = false;
    FString ExpectedHub, ExpectedPedro;
    EPhase Phase = EPhase::InitialHub;
    double Started = 0, PhaseStarted = 0;
    int32 Frames = 0, CompletedCases = 0;
    int32 ActionIndex = 0; // first, repeat, second distinct, third distinct
    int32 EvidenceBefore = 0, InspectionsBefore = 0;
    TWeakObjectPtr<UGameInstance> Game;
    TWeakObjectPtr<UWorld> PreviousWorld;
    TWeakObjectPtr<UWorld> PedroWorld;
    TWeakObjectPtr<AMainPlayerCharacter> Player;
    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<UPlayerInteractionComponent> Interaction;
    TWeakObjectPtr<UCameraComponent> Camera;
    TWeakObjectPtr<APedroEncounterDirector> Director;
    TWeakObjectPtr<APedroObserverCharacter> Observer;
    TWeakObjectPtr<APedroObserverAIController> AI;
    TWeakObjectPtr<APedroPlayableIntegrationActor> Integration;
    TWeakObjectPtr<APedroPrototypeInteractableActor> PendingProp;
    TWeakObjectPtr<APedroPrototypeInteractableActor> FirstProp;
    TWeakObjectPtr<AEncounterEntranceActor> Entrance;
    TWeakObjectPtr<APedroPrototypeReturnVolume> Return;
    TWeakObjectPtr<UEncounterDefinition> Definition;
    FName EncounterId = NAME_None;
    TSet<FName> AcquiredCore;
    TSet<FGuid> AcceptedEventIds;
    TSet<FName> AttemptedThisAction;
    double InteractionStarted = 0;
    double NextSearchAt = 0;
    TStrongObjectPtr<UPedroFlowResolvedProbe> Probe;
    int32 LastEvidenceCount = 0;
};
FState S;

const TCHAR* ScenarioName() { return S.bEarly ? TEXT("early") : TEXT("completion"); }
void Next(EPhase NewPhase)
{
    S.Phase = NewPhase;
    S.PhaseStarted = FPlatformTime::Seconds();
    S.Frames = 0;
}

bool Done(const TCHAR* Reason = nullptr)
{
    if (Reason)
    {
        UE_LOG(LogTemp, Error, TEXT("PEDRO_FLOW_TEST status=FAIL scenario=%s reason=%s phase=%d"),
            ScenarioName(), Reason, static_cast<int32>(S.Phase));
    }
    else
    {
        const int32 Expected = S.bEarly ? 5 : 12;
        UE_LOG(LogTemp, Display, TEXT("PEDRO_FLOW_TEST status=SUMMARY scenario=%s passed=%d expected=%d result=%s"),
            ScenarioName(), S.CompletedCases, Expected,
            S.CompletedCases == Expected ? TEXT("PASS") : TEXT("FAIL"));
    }
    FPlatformMisc::RequestExit(false);
    return false;
}

void StartNextInteraction()
{
    S.AttemptedThisAction.Reset();
    S.InteractionStarted = FPlatformTime::Seconds();
    S.NextSearchAt = 0;
    Next(EPhase::ChooseInteraction);
}

void Pass(const TCHAR* Case, const FString& Detail = FString())
{
    ++S.CompletedCases;
    UE_LOG(LogTemp, Display, TEXT("PEDRO_FLOW_TEST status=PASS scenario=%s case=%s %s"),
        ScenarioName(), Case, *Detail);
}

// Read only UPROPERTY pointers; the game exposes no test-only mutable API.
template<class T>
T* ReadActorProperty(const UObject* Instance, const TCHAR* Name)
{
    if (!IsValid(Instance)) return nullptr;
    const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(
        Instance->GetClass(), FName(Name));
    return Property ? Cast<T>(Property->GetObjectPropertyValue_InContainer(Instance)) : nullptr;
}

UWorld* RunningGameWorld()
{
    if (!GEngine) return nullptr;
    UWorld* Result = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        UWorld* W = Context.World();
        if (W && W->WorldType == EWorldType::Game && W->HasBegunPlay() &&
            W->GetFirstPlayerController() && W->GetFirstPlayerController()->GetPawn())
        {
            if (Result) return nullptr; // ambiguous game worlds fail by timeout
            Result = W;
        }
    }
    return Result;
}

bool BindRealPlayer(UWorld* World)
{
    if (!IsValid(World)) return false;
    auto* PC = World->GetFirstPlayerController();
    auto* Player = PC ? Cast<AMainPlayerCharacter>(PC->GetPawn()) : nullptr;
    auto* Camera = Player ? Player->FindComponentByClass<UCameraComponent>() : nullptr;
    auto* Interaction = Player ? Player->GetInteractionComponent() : nullptr;
    if (!IsValid(PC) || !IsValid(Player) || !Player->IsPlayerControlled() ||
        !IsValid(Camera) || !IsValid(Interaction)) return false;
    S.Player = Player; S.Controller = PC; S.Camera = Camera; S.Interaction = Interaction;
    return true;
}

bool CameraHitsActor(AActor* Target)
{
    UWorld* W = RunningGameWorld();
    if (!IsValid(W) || !IsValid(Target) || !S.Player.IsValid() || !S.Camera.IsValid())
        return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PedroFlowExactPlayerTrace), false);
    Params.AddIgnoredActor(S.Player.Get());
    const FVector Start = S.Camera->GetComponentLocation();
    FHitResult Hit;
    return W->LineTraceSingleByChannel(Hit, Start,
        Start + S.Camera->GetForwardVector() * 250.0f, ECC_Visibility, Params) &&
        Hit.GetActor() == Target;
}

// Exactly the physical eye/cone and blocking Visibility ray used for Pedro's
// event acquisition. This is a preflight; the Director remains authoritative.
bool ObserverSees(APedroPrototypeInteractableActor* Prop)
{
    return PedroSmokeVisibility::CanSee(RunningGameWorld(), S.Observer.Get(),
        S.Director.Get(), Prop);
}

bool ValidCoreSource(APedroPrototypeInteractableActor* Prop, FName& OutId)
{
    if (!IsValid(Prop) || !S.Director.IsValid()) return false;
    auto* Source = Prop->FindComponentByClass<UPedroObservationSourceComponent>();
    if (!IsValid(Source) || !Source->bEvidenceEnabled || Source->OpportunityId.IsNone() ||
        Source->SubjectId.IsNone()) return false;
    int32 Matches = 0;
    for (const FPedroOpportunityDefinition& Def : S.Director->OpportunityDefinitions)
        if (Def.OpportunityId == Source->OpportunityId && Def.Tier == EPedroOpportunityTier::Core)
            ++Matches;
    if (Matches != 1) return false;
    OutId = Source->OpportunityId;
    return true;
}

// Only test-player scenario placement: actual Recast projection/complete path,
// capsule occupancy and first-hit player camera geometry all have to agree.
bool PreparePosition(AActor* Target, bool bRequirePedroVisibility, bool bRequireNav)
{
    UWorld* W = RunningGameWorld();
    auto* P = S.Player.Get(); auto* Cam = S.Camera.Get(); auto* PC = S.Controller.Get();
    if (!W || !IsValid(Target) || !IsValid(P) || !IsValid(Cam) || !IsValid(PC)) return false;
    auto* Move = P->GetCharacterMovement(); auto* Capsule = P->GetCapsuleComponent();
    if (!IsValid(Move) || !IsValid(Capsule)) return false;
    const FVector CameraOffset = Cam->GetComponentLocation() - P->GetActorLocation();
    const FVector FeetOffset = P->GetActorLocation() - Move->GetFeetLocation();
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(
        Capsule->GetScaledCapsuleRadius(),
        FMath::Max(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()-3.0f));
    FVector Origin, Extent;
    Target->GetActorBounds(false, Origin, Extent);
    const ANavigationData* Data = nullptr;
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
    FNavLocation StartNav;
    if (bRequireNav)
    {
        if (!Nav) return false;
        Data = Nav->GetNavDataForProps(Move->GetNavAgentPropertiesRef(),
            Move->GetFeetLocation(), FVector::ZeroVector);
        if (!IsValid(Cast<ARecastNavMesh>(Data)) ||
            !Nav->ProjectPointToNavigation(Move->GetFeetLocation(), StartNav,
                FVector(85,85,180), Data)) return false;
    }
    const float Radii[] = {120.f, 160.f, 200.f};
    for (float Radius : Radii)
        for (int32 AngleIndex=0; AngleIndex<24; ++AngleIndex)
        {
            const double Radians = (2.0 * PI * AngleIndex) / 24.0;
            const FVector Offset(FMath::Cos(Radians)*Radius, FMath::Sin(Radians)*Radius, 0);
            FVector Candidate;
            if (bRequireNav)
            {
                FNavLocation Destination;
                if (!Nav->ProjectPointToNavigation(Target->GetActorLocation()+Offset,
                        Destination, FVector(70,70,160), Data)) continue;
                if (FVector::Dist2D(Destination.Location,Target->GetActorLocation()+Offset)>70) continue;
                FPathFindingQuery Query(PC, *Data, StartNav.Location, Destination.Location);
                Query.SetAllowPartialPaths(false);
                Query.SetNavAgentProperties(Move->GetNavAgentPropertiesRef());
                const FPathFindingResult Route=Nav->FindPathSync(Move->GetNavAgentPropertiesRef(), Query);
                if (!Route.IsSuccessful() || !Route.Path.IsValid() || Route.Path->IsPartial() ||
                    Route.Path->ContainsAnyCustomLink()) continue;
                Candidate = Destination.Location + FeetOffset;
            }
            else
            {
                // Hub has no required Recast data: preserve the actual pawn's
                // grounded Z and derive XY strictly from the saved entrance.
                Candidate = FVector(Origin.X+Offset.X,Origin.Y+Offset.Y,P->GetActorLocation().Z);
            }
            FCollisionQueryParams Occupancy(SCENE_QUERY_STAT(PedroFlowPlayerCapsule),false);
            Occupancy.AddIgnoredActor(P);
            if (W->OverlapBlockingTestByChannel(Candidate, P->GetActorQuat(),
                    ECC_Pawn, Shape, Occupancy)) continue;
            const FVector Eye = Candidate + CameraOffset;
            if (FVector::Dist(Eye,Origin)>=245.f) continue;
            FCollisionQueryParams PlayerRay(SCENE_QUERY_STAT(PedroFlowPlayerLOS),false);
            PlayerRay.AddIgnoredActor(P);
            FHitResult Hit;
            if (!W->LineTraceSingleByChannel(Hit,Eye,Origin,ECC_Visibility,PlayerRay) ||
                Hit.GetActor()!=Target) continue;
            if (bRequirePedroVisibility && !ObserverSees(Cast<APedroPrototypeInteractableActor>(Target)))
                continue;
            if (!P->SetActorLocation(Candidate,false,nullptr,ETeleportType::TeleportPhysics) ||
                FVector::Dist(P->GetActorLocation(),Candidate)>2.f) continue;
            PC->SetControlRotation((Origin-Eye).Rotation());
            return true;
        }
    return false;
}

bool FindHubEntrance(UWorld* World)
{
    S.Entrance.Reset();
    int32 Matches=0;
    for (TActorIterator<AEncounterEntranceActor> It(World); It; ++It)
    {
        AEncounterEntranceActor* Entry = *It;
        auto* Definition = ReadActorProperty<UEncounterDefinition>(Entry,TEXT("EncounterDefinition"));
        if (!IsValid(Definition) || Definition->GetEncounterId().IsNone() ||
            Definition->GetEncounterMap().ToSoftObjectPath().GetLongPackageName()!=S.ExpectedPedro)
            continue;
        ++Matches; S.Entrance=Entry;
        if (S.EncounterId.IsNone()) S.EncounterId=Definition->GetEncounterId();
        else if (Definition->GetEncounterId()!=S.EncounterId) return false;
        // Refresh the non-owning reference from the *current* hub world.
        // Only the stable EncounterId spans world lifetimes.
        S.Definition=Definition;
    }
    return Matches==1 && S.Entrance.IsValid();
}

bool PreparePedro(UWorld* W, bool bAlreadyResolved)
{
    S.Director.Reset(); S.Observer.Reset(); S.AI.Reset(); S.Integration.Reset(); S.Return.Reset();
    if (!BindRealPlayer(W)) return false;
    APedroEncounterDirector* D=nullptr;
    APedroObserverCharacter* O=nullptr;
    APedroPlayableIntegrationActor* I=nullptr;
    APedroPrototypeReturnVolume* R=nullptr;
    int32 ND=0,NO=0,NI=0,NR=0;
    for(TActorIterator<APedroEncounterDirector> It(W);It;++It){D=*It;++ND;}
    for(TActorIterator<APedroObserverCharacter> It(W);It;++It){O=*It;++NO;}
    for(TActorIterator<APedroPlayableIntegrationActor> It(W);It;++It){I=*It;++NI;}
    for(TActorIterator<APedroPrototypeReturnVolume> It(W);It;++It){R=*It;++NR;}
    if (ND!=1 || NO!=1 || NI!=1 || NR!=1 || !IsValid(D) || !IsValid(O) ||
        !IsValid(I) || !IsValid(R)) return false;
    auto* AI=Cast<APedroObserverAIController>(O->GetController());
    auto* Definition=ReadActorProperty<UEncounterDefinition>(D,TEXT("EncounterDefinition"));
    if (!IsValid(AI) || AI->GetPawn()!=O || !IsValid(Definition) ||
        Definition->GetEncounterId()!=S.EncounterId ||
        !D->bUseDirectObservationAcquisition || D->bUsePrototypeAssumedAcquisition ||
        D->CompletionPolicy!=EPedroCompletionPolicy::EvidenceDriven ||
        D->DirectObservationActor!=O || I->Observer!=O || I->Director!=D ||
        D->OpportunityDefinitions.Num()<4 ||
        D->GetObservationState().Opportunities.GetCoreCount()!=4 ||
        D->GetObservationState().Opportunities.GetRequiredCoreSamples(D->CoreSampleFraction)!=3 ||
        D->GetObservationState().GetEvidenceCount()!=0 ||
        D->GetObservationState().Opportunities.GetSampledCoreCount()!=0 ||
        D->HasFinalClassification()) return false;
    if (bAlreadyResolved)
    {
        if (I->IsPlayableActive() || !D->IsEncounterProgressResolved() ||
            S.Probe->NotificationCount!=1) return false;
    }
    else
    {
        if (!I->IsPlayableActive() || !AI->IsAwarenessRunning() ||
            !AI->IsAttentionRunning() || !AI->IsNavigationRunning() ||
            !AI->IsContextualObservationRunning() || D->IsEncounterProgressResolved()) return false;
    }
    S.Director = D; // Required by ValidCoreSource; still read-only.
    int32 CoreProps=0;
    TSet<FName> IDs;
    for (TActorIterator<APedroPrototypeInteractableActor> It(W);It;++It)
    {
        FName Id;
        if (ValidCoreSource(*It, Id)) { ++CoreProps; IDs.Add(Id); }
    }
    if (CoreProps<3 || IDs.Num()<3) return false;
    auto* Box=R->FindComponentByClass<UBoxComponent>();
    if (!IsValid(Box) || !Box->GetGenerateOverlapEvents() ||
        Box->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Overlap) return false;
    S.Director=D;S.Observer=O;S.AI=AI;S.Integration=I;S.Return=R;S.PedroWorld=W;
    return true;
}

bool PostActionValid()
{
    auto* D=S.Director.Get();auto* Prop=S.PendingProp.Get();
    auto* Source=Prop?Prop->FindComponentByClass<UPedroObservationSourceComponent>():nullptr;
    if (!IsValid(D)||!IsValid(Prop)||!IsValid(Source))return false;
    const auto& State=D->GetObservationState();
    if (State.GetEvidenceCount()!=S.EvidenceBefore+1 ||
        Prop->GetSuccessfulInspectionCount()!=S.InspectionsBefore+1 ||
        State.EvidenceHistory.Num()<1) return false;
    const auto& Record=State.EvidenceHistory.Last();
    if (!Record.Event.EventId.IsValid() || S.AcceptedEventIds.Contains(Record.Event.EventId) ||
        Record.KnowledgeSource!=EPedroKnowledgeSource::DirectObservation ||
        Record.Event.SubjectActor.Get()!=Prop ||
        Record.Event.SubjectId!=Source->SubjectId ||
        Record.Event.OpportunityId!=Source->OpportunityId ||
        Record.Event.Kind!=(S.ActionIndex==1?EPedroEvidenceKind::RepeatedInteraction:
                                             EPedroEvidenceKind::InspectedOptionalObject))return false;
    S.AcceptedEventIds.Add(Record.Event.EventId);
    if (S.ActionIndex!=1)S.AcquiredCore.Add(Source->OpportunityId);
    const int32 RequiredDistinct=S.ActionIndex==0 || S.ActionIndex==1 ? 1 : S.ActionIndex==2?2:3;
    if (State.Opportunities.GetSampledCoreCount()!=RequiredDistinct ||
        S.AcquiredCore.Num()!=RequiredDistinct) return false;
    const FString Data=FString::Printf(TEXT("opportunity=%s event=%s core=%d evidence=%d provenance=DirectObservation"),
        *Source->OpportunityId.ToString(),*Record.Event.EventId.ToString(EGuidFormats::Digits),
        RequiredDistinct, State.GetEvidenceCount());
    switch(S.ActionIndex)
    {
    case 0: Pass(TEXT("Evidence01"),Data);break;
    case 1: Pass(TEXT("RepeatNoExtraCore"),Data);break;
    case 2: Pass(TEXT("Evidence02"),Data);break;
    case 3: Pass(TEXT("Evidence03"),Data);break;
    default:return false;
    }
    return true;
}

// Use the active Director's definition, never a weak UObject inherited from
// a departed hub world.  Every clause has its own failure identity so a
// genuine mismatch can be repaired without weakening the acceptance gate.
bool OutcomeAndProgressCorrect(FString& Failure)
{
    auto* D=S.Director.Get();
    auto* Progress=S.Game.IsValid()?S.Game->GetSubsystem<UEncounterProgressSubsystem>():nullptr;
    if (!IsValid(D)) { Failure=TEXT("DirectorMissing"); return false; }
    if (!IsValid(Progress)) { Failure=TEXT("ProgressMissing"); return false; }
    auto* Definition=ReadActorProperty<UEncounterDefinition>(D,TEXT("EncounterDefinition"));
    if (!IsValid(Definition) || Definition->GetEncounterId()!=S.EncounterId)
    {
        Failure=TEXT("CurrentDirectorDefinitionOrIdMismatch"); return false;
    }
    if (!D->HasFinalClassification()) { Failure=TEXT("NoFinalClassification"); return false; }
    if (!D->HasCommittedContextResolution()) { Failure=TEXT("NoCommittedContextResolution"); return false; }
    if (!Progress->IsEncounterResolved(Definition)) { Failure=TEXT("CurrentEncounterNotResolved"); return false; }
    if (Progress->GetResolvedEncounterCount()!=1)
    {
        Failure=FString::Printf(TEXT("ResolvedCount=%d"),Progress->GetResolvedEncounterCount());
        return false;
    }
    if (!S.Probe.IsValid()) { Failure=TEXT("DelegateProbeMissing"); return false; }
    if (S.Probe->NotificationCount!=1 || S.Probe->LastEncounterId!=S.EncounterId)
    {
        Failure=FString::Printf(TEXT("NotificationCount=%d LastId=%s ExpectedId=%s"),
            S.Probe->NotificationCount,*S.Probe->LastEncounterId.ToString(),*S.EncounterId.ToString());
        return false;
    }
    const FPedroObservationState& Observed=D->GetObservationState();
    // Genuine four accepted actions: three different core responses and a
    // repeat, not merely four dispatched clicks or fabricated evidence.
    if (Observed.GetEvidenceCount()!=4 ||
        Observed.Opportunities.GetSampledCoreCount()!=3 ||
        S.AcceptedEventIds.Num()!=4 || S.AcquiredCore.Num()!=3)
    {
        Failure=FString::Printf(TEXT("EvidenceMismatch acquired=%d core=%d ids=%d distinct=%d"),
            Observed.GetEvidenceCount(),Observed.Opportunities.GetSampledCoreCount(),
            S.AcceptedEventIds.Num(),S.AcquiredCore.Num());
        return false;
    }
    const FPedroClassificationResult& Result=D->GetFinalClassification();
    if (!Result.bHasEvidence) { Failure=TEXT("ClassificationHasNoEvidence"); return false; }
    AActor* Outcomes[3]={ReadActorProperty<AActor>(D,TEXT("GentleOutcomeActor")),
        ReadActorProperty<AActor>(D,TEXT("SeekingOutcomeActor")),
        ReadActorProperty<AActor>(D,TEXT("DisruptiveOutcomeActor"))};
    const int32 Winner=static_cast<int32>(Result.Interpretation);
    if (Winner<0 || Winner>2) { Failure=TEXT("UnknownInterpretation"); return false; }
    for(int32 i=0;i<3;++i)
    {
        if (!IsValid(Outcomes[i]))
        {
            Failure=FString::Printf(TEXT("MissingOutcomeActor index=%d"),i); return false;
        }
    }
    if (Outcomes[0]==Outcomes[1] || Outcomes[0]==Outcomes[2] || Outcomes[1]==Outcomes[2])
    {
        Failure=TEXT("DuplicateOutcomeActors"); return false;
    }
    for(int32 i=0;i<3;++i)
    {
        const bool bExpectedActive=i==Winner;
        if (Outcomes[i]->IsHidden()==bExpectedActive ||
            Outcomes[i]->GetActorEnableCollision()!=bExpectedActive)
        {
            Failure=FString::Printf(
                TEXT("OutcomeActorState index=%d winner=%d hidden=%s collision=%s actor=%s"),
                i,Winner,Outcomes[i]->IsHidden()?TEXT("yes"):TEXT("no"),
                Outcomes[i]->GetActorEnableCollision()?TEXT("yes"):TEXT("no"),
                *GetNameSafe(Outcomes[i]));
            return false;
        }
    }
    Failure.Reset();
    return true;
}

bool EnterPedroThroughHub(UWorld* W)
{
    if (!BindRealPlayer(W) || !FindHubEntrance(W)) return false;
    if (!PreparePosition(S.Entrance.Get(),false,false)) return false;
    Next(EPhase::AimAtEntrance);
    return true;
}

bool TriggerRealReturn()
{
    auto* R=S.Return.Get();auto* P=S.Player.Get();auto* W=RunningGameWorld();
    auto* Box=R?R->FindComponentByClass<UBoxComponent>():nullptr;
    if (!IsValid(R)||!IsValid(P)||!IsValid(Box)||!W ||
        !Box->GetGenerateOverlapEvents()||Box->IsOverlappingActor(P))return false;
    const FVector Destination=Box->GetComponentLocation();
    // This must enter the saved box and invoke its real OnComponentBeginOverlap.
    // No direct call to UGameplayStatics::OpenLevel or the private overlap handler.
    // Keep the departing world identity BEFORE dispatching an overlap that can
    // enqueue level travel. Never dereference the previous world afterwards.
    S.PreviousWorld=W;
    if (!P->SetActorLocation(Destination,false,nullptr,ETeleportType::TeleportPhysics))return false;
    if (FVector::Dist(P->GetActorLocation(),Destination)>2.0f)return false;
    Next(EPhase::WaitHub);
    return true;
}

bool Tick(float)
{
    if (!FParse::Param(FCommandLine::Get(),TEXT("PedroFlowSmoke"))) return false;
    if (!S.bStarted)
    {
        S.bStarted=true;
        S.Started=FPlatformTime::Seconds();
        FString Scenario;
        if (!FParse::Value(FCommandLine::Get(),TEXT("PedroFlowScenario="),Scenario) ||
            (Scenario!=TEXT("completion") && Scenario!=TEXT("early")) ||
            !FParse::Value(FCommandLine::Get(),TEXT("PedroFlowExpectedHub="),S.ExpectedHub) ||
            !FParse::Value(FCommandLine::Get(),TEXT("PedroFlowExpectedPedro="),S.ExpectedPedro) ||
            !((S.ExpectedHub==ProductionHub && S.ExpectedPedro==ProductionPedro) ||
              (S.ExpectedHub==IsolatedHub && S.ExpectedPedro==IsolatedPedro)))
            return Done(TEXT("UnapprovedScenarioOrMapPair"));
        S.bEarly=Scenario==TEXT("early");
        Next(EPhase::InitialHub);
        UE_LOG(LogTemp,Display,TEXT("PEDRO_FLOW_TEST status=BEGIN scenario=%s hub=%s pedro=%s expected=%d"),
            ScenarioName(),*S.ExpectedHub,*S.ExpectedPedro,S.bEarly?5:12);
    }
    if (FPlatformTime::Seconds()-S.Started>180.0)return Done(TEXT("GlobalDeadline"));
    UWorld* W=RunningGameWorld();
    if (!W) return true;
    const FString LoadedMap=W->GetOutermost()->GetName();
    if (S.Game.IsValid() && W->GetGameInstance()!=S.Game.Get())
        return Done(TEXT("LostOriginalGameInstanceAcrossTravel"));
    auto* Progress=W->GetGameInstance()
        ? W->GetGameInstance()->GetSubsystem<UEncounterProgressSubsystem>() : nullptr;
    if (!IsValid(Progress))return Done(TEXT("MissingSessionProgress"));
    if (S.Phase!=EPhase::ChooseInteraction &&
        FPlatformTime::Seconds()-S.PhaseStarted>30.0) return Done(TEXT("StageDeadline"));
    switch(S.Phase)
    {
    case EPhase::InitialHub:
    {
        if (LoadedMap!=S.ExpectedHub)return Done(TEXT("DidNotStartInExactHub"));
        if (Progress->GetResolvedEncounterCount()!=0)return Done(TEXT("RunNotFresh"));
        S.Game=W->GetGameInstance();
        S.Probe.Reset(NewObject<UPedroFlowResolvedProbe>(S.Game.Get()));
        if (!S.Probe.IsValid())return Done(TEXT("CannotObserveResolutionDelegate"));
        Progress->OnEncounterResolved.AddDynamic(S.Probe.Get(),&UPedroFlowResolvedProbe::OnEncounterResolved);
        if (!EnterPedroThroughHub(W))return Done(TEXT("CannotReachSavedHubEntrance"));
        return true;
    }
    case EPhase::AimAtEntrance:
    {
        if (LoadedMap!=S.ExpectedHub || !S.Entrance.IsValid() || !S.Interaction.IsValid())
            return Done(TEXT("HubActorLostBeforeInteraction"));
        if (++S.Frames<2)return true;
        if (!CameraHitsActor(S.Entrance.Get()))return Done(TEXT("HubCameraNotAimedAtEntrance"));
        S.PreviousWorld=W; // Set before the real entrance can enqueue travel.
        if (!S.Interaction->TryInteract())return Done(TEXT("RealHubEntranceDispatchFailed"));
        Next(S.bReentry?EPhase::WaitReentered:EPhase::WaitPedro);
        return true;
    }
    case EPhase::WaitPedro:
    case EPhase::WaitReentered:
    {
        if (LoadedMap==S.ExpectedHub && W==S.PreviousWorld.Get())return true;
        if (LoadedMap!=S.ExpectedPedro || W==S.PreviousWorld.Get())
            return Done(TEXT("HubEntranceDidNotLoadExpectedPedroMap"));
        Next(S.Phase==EPhase::WaitReentered?EPhase::VerifyReentered:EPhase::PedroSetup);
        return true;
    }
    case EPhase::PedroSetup:
    {
        if (LoadedMap!=S.ExpectedPedro)return Done(TEXT("PedroMapChangedUnexpectedly"));
        if (!PreparePedro(W,false))
        {
            if (FPlatformTime::Seconds()-S.PhaseStarted<3.0)return true; // auto-possession startup
            return Done(TEXT("InvalidSavedPedroConfiguration"));
        }
        Pass(TEXT("HubEntry"),TEXT("travel=RealPlayerInteraction"));
        Pass(TEXT("PedroConfiguration"),TEXT("policy=EvidenceDriven core=4 required=3"));
        if (S.bEarly)Next(EPhase::ReturnToHub);
        else StartNextInteraction();
        return true;
    }
    case EPhase::ChooseInteraction:
    {
        if (LoadedMap!=S.ExpectedPedro || !S.Director.IsValid())
            return Done(TEXT("LostPedroBeforeInteraction"));
        if (FPlatformTime::Seconds()-S.InteractionStarted>38.0)
        {
            FVector ObserverEye=FVector::ZeroVector;
            FRotator ObserverFacing=FRotator::ZeroRotator;
            if (S.Observer.IsValid())
                S.Observer->GetActorEyesViewPoint(ObserverEye,ObserverFacing);
            UE_LOG(LogTemp,Error,
                TEXT("PEDRO_FLOW_DIAG scenario=%s check=CoreSearch reason=NoThirdObservedReachableCore action=%d acquiredCore=%d attempted=%d eye=%s yaw=%.2f"),
                ScenarioName(),S.ActionIndex,S.AcquiredCore.Num(),
                S.AttemptedThisAction.Num(),*ObserverEye.ToString(),ObserverFacing.Yaw);
            return Done(TEXT("NoThirdObservedReachableCore"));
        }
        if (FPlatformTime::Seconds()<S.NextSearchAt)return true;
        S.NextSearchAt=FPlatformTime::Seconds()+0.40;
        APedroPrototypeInteractableActor* Chosen=nullptr;
        FName ChosenId;
        // Preserve the genuinely repeated action on exactly the first prop.
        // For a new core, consider currently visible targets first, then
        // initially unseen targets: the player can legitimately move nearer
        // while the autonomous observer turns or walks to investigate.
        if (S.ActionIndex==1)
        {
            Chosen=S.FirstProp.Get();
            if (IsValid(Chosen) && !ValidCoreSource(Chosen,ChosenId))
                return Done(TEXT("RepeatSourceInvalid"));
            if (S.AttemptedThisAction.Contains(ChosenId)) Chosen=nullptr;
            if (Chosen && !PreparePosition(Chosen,false,true)) Chosen=nullptr;
        }
        else
        {
            TArray<APedroPrototypeInteractableActor*> Candidates;
            for(TActorIterator<APedroPrototypeInteractableActor> It(W);It;++It)
            {
                FName Id;
                if (ValidCoreSource(*It,Id) && !S.AcquiredCore.Contains(Id) &&
                    !S.AttemptedThisAction.Contains(Id))Candidates.Add(*It);
            }
            Candidates.Sort([](const APedroPrototypeInteractableActor& A,
                               const APedroPrototypeInteractableActor& B)
                {return A.GetName()<B.GetName();});
            for (const bool bPreviouslyVisible : {true,false})
            {
                for(auto* Target:Candidates)
                {
                    if (ObserverSees(Target)!=bPreviouslyVisible)continue;
                    if (!PreparePosition(Target,false,true))continue;
                    Chosen=Target;
                    ValidCoreSource(Chosen,ChosenId);
                    break;
                }
                if (Chosen)break;
            }
        }
        if (!Chosen)
        {
            // No target currently has a legal Recast-projected player
            // approach. Do not move Pedro, falsify knowledge or retry at 120 Hz.
            // Re-sample naturally as autonomous perception/movement evolves.
            if (S.AttemptedThisAction.Num()>0 &&
                FPlatformTime::Seconds()-S.PhaseStarted>0.8)
                S.AttemptedThisAction.Reset();
            return true;
        }
        S.PendingProp=Chosen;
        S.AttemptedThisAction.Add(ChosenId);
        UE_LOG(LogTemp,Display,
            TEXT("PEDRO_FLOW_SEARCH action=%d opportunity=%s player=ValidRecastCandidate initialLOS=%s"),
            S.ActionIndex,*ChosenId.ToString(),ObserverSees(Chosen)?TEXT("yes"):TEXT("no"));
        Next(EPhase::WaitPedroSight);
        return true;
    }
    case EPhase::WaitPedroSight:
    {
        if (!S.PendingProp.IsValid() || !S.Interaction.IsValid())
            return Done(TEXT("WaitingForDeletedInteractionActor"));
        // Aim and let Pedro's live controller make its own decision; *never*
        // force his pose or generate a fake DirectObservation event.
        if (++S.Frames>=2 && CameraHitsActor(S.PendingProp.Get()) &&
            ObserverSees(S.PendingProp.Get()))
        {
            Next(EPhase::AimAtProp);
            return true;
        }
        if (FPlatformTime::Seconds()-S.PhaseStarted>6.0)
        {
            FName Id;
            ValidCoreSource(S.PendingProp.Get(),Id);
            UE_LOG(LogTemp,Display,
                TEXT("PEDRO_FLOW_SEARCH action=%d opportunity=%s observation=NotYetVisible retry=anotherCandidate"),
                S.ActionIndex,*Id.ToString());
            Next(EPhase::ChooseInteraction);
        }
        return true;
    }
    case EPhase::AimAtProp:
    {
        if (!S.PendingProp.IsValid() || !S.Director.IsValid() || !S.Interaction.IsValid())
            return Done(TEXT("InteractionActorLost"));
        if (++S.Frames<2)return true;
        if (!CameraHitsActor(S.PendingProp.Get()) || !ObserverSees(S.PendingProp.Get()))
        {
            Next(EPhase::WaitPedroSight);return true;
        }
        auto* Prop=S.PendingProp.Get();
        auto* Source=Prop->FindComponentByClass<UPedroObservationSourceComponent>();
        if (!IsValid(Source))return Done(TEXT("MissingSourceAtDispatch"));
        const auto& State=S.Director->GetObservationState();
        S.EvidenceBefore=State.GetEvidenceCount();
        S.InspectionsBefore=Prop->GetSuccessfulInspectionCount();
        if (!S.Interaction->TryInteract())return Done(TEXT("RealTryInteractRejectedVerifiedTrace"));
        if (S.ActionIndex==0)S.FirstProp=Prop;
        Next(EPhase::WaitActionEvaluation);
        return true;
    }
    case EPhase::WaitActionEvaluation:
    {
        if (FPlatformTime::Seconds()-S.PhaseStarted<.20)return true; // next-tick Director completion
        if (!PostActionValid())return Done(TEXT("NotOneRealDirectEvidencePerCore"));
        if (S.ActionIndex<3)
        {
            bool bOutcomePremature=false;
            for(const TCHAR* Name : {TEXT("GentleOutcomeActor"),TEXT("SeekingOutcomeActor"),TEXT("DisruptiveOutcomeActor")})
            {
                AActor* Outcome=ReadActorProperty<AActor>(S.Director.Get(),Name);
                if (!IsValid(Outcome) || !Outcome->IsHidden() || Outcome->GetActorEnableCollision())
                    bOutcomePremature=true;
            }
            if (bOutcomePremature || S.Director->HasFinalClassification() ||
                S.Director->IsEncounterProgressResolved() ||
                Progress->GetResolvedEncounterCount()!=0 || S.Probe->NotificationCount!=0)
                return Done(TEXT("PrematureClassificationOrResolution"));
            if (S.ActionIndex==2)
                Pass(TEXT("NoPrematureCompletion"), TEXT("after_two_distinct=yes progress=0"));
            ++S.ActionIndex;
            StartNextInteraction();
        }
        else
        {
            Next(EPhase::WaitSatisfied);
        }
        return true;
    }
    case EPhase::WaitSatisfied:
    {
        FString Reason;
        if (!OutcomeAndProgressCorrect(Reason))
        {
            if (FPlatformTime::Seconds()-S.PhaseStarted>3.0)
            {
                UE_LOG(LogTemp,Error,
                    TEXT("PEDRO_FLOW_DIAG scenario=%s check=Completion reason=%s"),
                    ScenarioName(),*Reason);
                return Done(TEXT("ClassificationOutcomeOrDelegateMismatch"));
            }
            return true;
        }
        if (!S.AI.IsValid() || (!S.AI->GetContextMemory().bSatisfied ||
                               S.AI->GetContextDecision().Mode!=EPedroContextMode::Satisfied))
        {
            if (FPlatformTime::Seconds()-S.PhaseStarted>5.0)
                return Done(TEXT("ObserverDidNotEnterSatisfied"));
            return true;
        }
        // A delayed Satisfied transition must not emit duplicate resolution cases.
        Pass(TEXT("ExactlyOneResolution"), FString::Printf(
            TEXT("classification=%d callbacks=%d sessionCount=%d"),
            static_cast<int32>(S.Director->GetFinalClassification().Interpretation),
            S.Probe->NotificationCount, Progress->GetResolvedEncounterCount()));
        Pass(TEXT("Satisfied"),TEXT("ai_mode=Satisfied"));
        Next(EPhase::ReturnToHub);
        return true;
    }
    case EPhase::ReturnToHub:
    {
        if (LoadedMap!=S.ExpectedPedro)return Done(TEXT("NotInPedroForRealReturn"));
        if (!TriggerRealReturn())return Done(TEXT("CouldNotTriggerRealReturnOverlap"));
        return true;
    }
    case EPhase::WaitHub:
    {
        if (LoadedMap==S.ExpectedPedro && W==S.PreviousWorld.Get())return true;
        if (LoadedMap!=S.ExpectedHub || W==S.PreviousWorld.Get())
            return Done(TEXT("ReturnVolumeDidNotLoadExpectedHub"));
        Next(EPhase::CheckHub);return true;
    }
    case EPhase::CheckHub:
    {
        if (LoadedMap!=S.ExpectedHub || !BindRealPlayer(W) || !FindHubEntrance(W))
            return Done(TEXT("HubLoadedWithoutPedroEntrance"));
        const int32 ExpectedCount=S.bEarly?0:1;
        if (Progress->GetResolvedEncounterCount()!=ExpectedCount ||
            Progress->IsEncounterResolved(S.Definition.Get())!=(!S.bEarly) ||
            S.Probe->NotificationCount!=ExpectedCount)
            return Done(TEXT("SharedProgressNotPreservedAcrossHubTravel"));
        Pass(S.bEarly?TEXT("EarlyReturn"):TEXT("ReturnToHub"),TEXT("travel=RealOverlap"));
        Pass(S.bEarly?TEXT("HubUnresolved"):TEXT("HubProgressPersisted"),
            FString::Printf(TEXT("sessionCount=%d callbacks=%d"),
                Progress->GetResolvedEncounterCount(),S.Probe->NotificationCount));
        S.bReentry=true;
        if (!PreparePosition(S.Entrance.Get(),false,false))return Done(TEXT("CannotReenterThroughHubEntrance"));
        Next(EPhase::ReenterFromHub);
        return true;
    }
    case EPhase::ReenterFromHub:
    {
        if (++S.Frames<2)return true;
        if (!CameraHitsActor(S.Entrance.Get()))return Done(TEXT("HubReentryCameraMissed"));
        S.PreviousWorld=W; // The interaction may immediately schedule travel.
        if (!S.Interaction->TryInteract())return Done(TEXT("RealHubReentryInteractionFailed"));
        Next(EPhase::WaitReentered);
        return true;
    }
    case EPhase::VerifyReentered:
    {
        if (LoadedMap!=S.ExpectedPedro)return Done(TEXT("ReentryWrongPedroMap"));
        if (!PreparePedro(W,!S.bEarly))
        {
            if (FPlatformTime::Seconds()-S.PhaseStarted<3.0)return true;
            return Done(TEXT("ReentryDirectorOrProgressInvalid"));
        }
        if (Progress->GetResolvedEncounterCount()!=(S.bEarly?0:1) ||
            S.Probe->NotificationCount!=(S.bEarly?0:1) ||
            S.Director->GetObservationState().GetEvidenceCount()!=0)
            return Done(TEXT("ReentryStateLeakedOrDuplicateResolve"));
        Pass(S.bEarly?TEXT("FreshReentry"):TEXT("CompletedReentry"),
            FString::Printf(TEXT("localEvidence=0 sessionCount=%d callbacks=%d active=%s"),
                Progress->GetResolvedEncounterCount(),S.Probe->NotificationCount,
                S.Integration->IsPlayableActive()?TEXT("yes"):TEXT("no")));
        return Done();
    }
    }
    return Done(TEXT("UnreachablePhase"));
}

[[maybe_unused]] const auto FlowTicker =
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
} // namespace PedroFlowSmoke
#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
