#include "Encounters/Pedro/PedroPhysicalObserverWorldValidationHarness.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Encounters/Pedro/PedroDirectObservation.h"
#include "Encounters/Pedro/PedroEncounterDirector.h"
#include "Encounters/Pedro/PedroObservationSourceComponent.h"
#include "Encounters/Pedro/PedroObserverCharacter.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"

namespace
{
constexpr float MaxRangeCm = 1000.0f;
constexpr float HalfFovDegrees = 60.0f;
const FName FixtureSubject(TEXT("PhysicalObserverFixtureProp"));
const FName FixtureOpportunity(TEXT("PhysicalObserverFixtureInspection"));

void SetVisibilityOnly(UStaticMeshComponent* Mesh, UStaticMesh* Cube)
{
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(Cube);
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}
}

APedroPhysicalObserverWorldValidationHarness::
    APedroPhysicalObserverWorldValidationHarness()
{
    PrimaryActorTick.bCanEverTick = false;
}

const TCHAR* APedroPhysicalObserverWorldValidationHarness::CaseName(ECase Case)
{
    switch (Case)
    {
    case ECase::Visible: return TEXT("Visible");
    case ECase::TurnAway: return TEXT("TurnAway");
    case ECase::TurnBack: return TEXT("TurnBack");
    case ECase::MoveOutOfRange: return TEXT("MoveOutOfRange");
    case ECase::MoveIntoRange: return TEXT("MoveIntoRange");
    case ECase::CoverBlocks: return TEXT("CoverBlocks");
    case ECase::NoRetroactive: return TEXT("NoRetroactive");
    case ECase::EyeFollowsPose: return TEXT("EyeFollowsPose");
    default: return TEXT("Unknown");
    }
}

void APedroPhysicalObserverWorldValidationHarness::BeginPlay()
{
    Super::BeginPlay();
    if (!bRunValidationInPIE)
    {
        return;
    }

    UWorld* World = GetWorld();
    // Never run this fixture in a saved or production gameplay level.
    if (!World || World->WorldType != EWorldType::PIE ||
        !World->GetMapName().Contains(TEXT("Untitled")))
    {
        UE_LOG(LogTemp, Error,
            TEXT("PEDRO_PHYSICAL_WORLD_TEST status=FAIL reason=RequiresUnsavedUntitledPIE"));
        return;
    }

    constexpr int32 Total = static_cast<int32>(ECase::Count);
    int32 Passed = 0;
    UE_LOG(LogTemp, Display,
        TEXT("PEDRO_PHYSICAL_WORLD_TEST status=BEGIN expected=%d mode=PIE"), Total);

    for (int32 Index = 0; Index < Total; ++Index)
    {
        const ECase TestCase = static_cast<ECase>(Index);
        const bool bPassed = RunCase(TestCase, Index);
        if (bPassed)
        {
            ++Passed;
            UE_LOG(LogTemp, Display,
                TEXT("PEDRO_PHYSICAL_WORLD_TEST status=PASS case=%s"),
                CaseName(TestCase));
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("PEDRO_PHYSICAL_WORLD_TEST status=FAIL case=%s"),
                CaseName(TestCase));
        }
    }

    UE_LOG(LogTemp, Display,
        TEXT("PEDRO_PHYSICAL_WORLD_TEST status=SUMMARY passed=%d failed=%d total=%d"),
        Passed, Total - Passed, Total);
}

bool APedroPhysicalObserverWorldValidationHarness::RunCase(
    ECase TestCase, int32 CaseIndex)
{
    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return false;
    }

    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!IsValid(Cube))
    {
        return false;
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Each test gets a separate region. Destruction is not assumed to make
    // pending actors disappear from all collision queries immediately.
    const FVector Base(10000.0 + 4000.0 * CaseIndex, 10000.0, 2000.0);
    const FVector NearRoot = Base + FVector(0.0, 0.0, -64.0);
    const FVector FarRoot = NearRoot + FVector(-2000.0, 0.0, 0.0);
    const FVector PropLocation = Base + FVector(300.0, 0.0, 0.0);

    APedroObserverCharacter* Observer = World->SpawnActor<APedroObserverCharacter>(
        NearRoot, FRotator::ZeroRotator, Params);
    AStaticMeshActor* Target = World->SpawnActor<AStaticMeshActor>(
        PropLocation, FRotator::ZeroRotator, Params);
    APedroEncounterDirector* Director = World->SpawnActor<APedroEncounterDirector>(
        Base + FVector(0.0, 0.0, -1000.0), FRotator::ZeroRotator, Params);
    AStaticMeshActor* Wall = nullptr;

    bool bOK = IsValid(Observer) && IsValid(Target) && IsValid(Director) &&
        IsValid(Target->GetStaticMeshComponent());
    UPedroObservationSourceComponent* Source = nullptr;

    if (bOK)
    {
        SetVisibilityOnly(Target->GetStaticMeshComponent(), Cube);

        Director->bUsePrototypeAssumedAcquisition = false;
        Director->bUseDirectObservationAcquisition = true;
        Director->DirectObservationActor = Observer;
        Director->DirectObservationMaxDistance = MaxRangeCm;
        Director->DirectObservationViewHalfAngleDegrees = HalfFovDegrees;

        Source = NewObject<UPedroObservationSourceComponent>(Target);
        bOK = IsValid(Source);
        if (bOK)
        {
            Target->AddInstanceComponent(Source);
            Source->bEvidenceEnabled = true;
            Source->SubjectId = FixtureSubject;
            Source->OpportunityId = FixtureOpportunity;
            Source->RegisterComponent();
            bOK = Director->RegisterObservationSource(Source);
        }
    }

    // The real ACharacter drives its eye transform. We change its pose
    // BEFORE the confirmed action is generated in these cases.
    if (bOK)
    {
        switch (TestCase)
        {
        case ECase::TurnAway:
        case ECase::TurnBack:
        case ECase::NoRetroactive:
            Observer->SetActorRotation(FRotator(0.0, 180.0, 0.0));
            break;
        case ECase::MoveOutOfRange:
        case ECase::MoveIntoRange:
            Observer->SetActorLocation(FarRoot);
            break;
        default:
            break;
        }
    }

    if (bOK && TestCase == ECase::CoverBlocks)
    {
        Wall = World->SpawnActor<AStaticMeshActor>(
            Base + FVector(150.0, 0.0, 0.0), FRotator::ZeroRotator, Params);
        bOK = IsValid(Wall) && IsValid(Wall->GetStaticMeshComponent());
        if (bOK)
        {
            SetVisibilityOnly(Wall->GetStaticMeshComponent(), Cube);
            Wall->SetActorScale3D(FVector(0.2, 3.0, 3.0));
        }
    }

    FPedroBehaviourEvent Probe;
    if (bOK)
    {
        Probe.EventId = FGuid::NewGuid();
        Probe.Kind = EPedroEvidenceKind::InspectedOptionalObject;
        Probe.SubjectActor = Target;
        Probe.SubjectId = FixtureSubject;
        Probe.OpportunityId = FixtureOpportunity;
        Probe.EventLocation = Target->GetActorLocation();
        Probe.WorldTimeSeconds = World->GetTimeSeconds();
    }

    auto SightNow = [&]()
    {
        return PedroDirectObservation::Evaluate(
            World, Observer, Probe, MaxRangeCm, HalfFovDegrees);
    };

    bool bEyeMoved = true;
    if (bOK && TestCase == ECase::TurnBack)
    {
        bOK = SightNow() == EPedroDirectObservationStatus::OutsideViewCone;
        Observer->SetActorRotation(FRotator::ZeroRotator);
    }
    if (bOK && TestCase == ECase::MoveIntoRange)
    {
        bOK = SightNow() == EPedroDirectObservationStatus::TooFar;
        Observer->SetActorLocation(NearRoot);
    }
    if (bOK && TestCase == ECase::EyeFollowsPose)
    {
        FVector BeforeLocation;
        FRotator BeforeRotation;
        Observer->GetActorEyesViewPoint(BeforeLocation, BeforeRotation);
        Observer->SetActorLocation(NearRoot + FVector(0.0, 50.0, 0.0));
        Observer->SetActorRotation(FRotator(0.0, -10.0, 0.0));
        FVector AfterLocation;
        FRotator AfterRotation;
        Observer->GetActorEyesViewPoint(AfterLocation, AfterRotation);
        bEyeMoved = AfterLocation.Equals(
            BeforeLocation + FVector(0.0, 50.0, 0.0), 0.1) &&
            AfterRotation.Equals(FRotator(0.0, -10.0, 0.0), 0.1) &&
            !AfterRotation.Equals(BeforeRotation, 0.1);
        bOK = bEyeMoved;
    }

    EPedroDirectObservationStatus Expected = EPedroDirectObservationStatus::Visible;
    switch (TestCase)
    {
    case ECase::TurnAway:
    case ECase::NoRetroactive:
        Expected = EPedroDirectObservationStatus::OutsideViewCone;
        break;
    case ECase::MoveOutOfRange:
        Expected = EPedroDirectObservationStatus::TooFar;
        break;
    case ECase::CoverBlocks:
        Expected = EPedroDirectObservationStatus::Occluded;
        break;
    default:
        break;
    }

    EPedroDirectObservationStatus Actual = EPedroDirectObservationStatus::MissingWorld;
    int32 FinalCount = -1;
    int32 ExpectedCount = Expected == EPedroDirectObservationStatus::Visible ? 1 : 0;

    if (bOK)
    {
        Actual = SightNow();
        bOK = Actual == Expected &&
            Source->ReportConfirmedAction(EPedroEvidenceKind::InspectedOptionalObject);
    }

    if (bOK)
    {
        const FPedroObservationState& State = Director->GetObservationState();
        FinalCount = State.GetEvidenceCount();
        bOK = FinalCount == ExpectedCount;
        if (bOK && ExpectedCount == 1)
        {
            bOK = State.EvidenceHistory.Num() == 1 &&
                State.EvidenceHistory[0].KnowledgeSource ==
                    EPedroKnowledgeSource::DirectObservation &&
                State.EvidenceHistory[0].Event.OpportunityId == FixtureOpportunity;
        }
    }

    if (bOK && TestCase == ECase::NoRetroactive)
    {
        // Rotating toward an OLD, unseen event must not acquire that event.
        Observer->SetActorRotation(FRotator::ZeroRotator);
        bOK = Director->GetObservationState().GetEvidenceCount() == 0 &&
            SightNow() == EPedroDirectObservationStatus::Visible;
        if (bOK)
        {
            // A NEW successful action can now be acquired as a new occurrence.
            bOK = Source->ReportConfirmedAction(
                EPedroEvidenceKind::InspectedOptionalObject) &&
                Director->GetObservationState().GetEvidenceCount() == 1;
            FinalCount = Director->GetObservationState().GetEvidenceCount();
            ExpectedCount = 1;
        }
    }

    UE_LOG(LogTemp, Display,
        TEXT("PEDRO_PHYSICAL_WORLD_TEST case=%s expectedTrace=%s actualTrace=%s "
             "acquired=%d expectedAcquired=%d eyeMoved=%s valid=%s"),
        CaseName(TestCase),
        PedroDirectObservation::StatusName(Expected),
        PedroDirectObservation::StatusName(Actual),
        FinalCount, ExpectedCount, bEyeMoved ? TEXT("yes") : TEXT("no"),
        bOK ? TEXT("yes") : TEXT("no"));

    if (IsValid(Director)) Director->Destroy();
    if (IsValid(Target)) Target->Destroy();
    if (IsValid(Wall)) Wall->Destroy();
    if (IsValid(Observer)) Observer->Destroy();
    return bOK;
}
