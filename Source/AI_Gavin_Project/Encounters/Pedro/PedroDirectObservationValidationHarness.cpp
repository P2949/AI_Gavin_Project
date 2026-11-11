#include "Encounters/Pedro/PedroDirectObservationValidationHarness.h"

#include "Components/StaticMeshComponent.h"
#include "Encounters/Pedro/PedroDirectObservation.h"
#include "Encounters/Pedro/PedroEncounterDirector.h"
#include "Encounters/Pedro/PedroObservationSourceComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"

APedroDirectObservationValidationHarness::APedroDirectObservationValidationHarness()
{
    PrimaryActorTick.bCanEverTick = false;
}

const TCHAR* APedroDirectObservationValidationHarness::Name(ECase Case)
{
    switch (Case)
    {
    case ECase::Visible: return TEXT("Visible");
    case ECase::Occluded: return TEXT("Occluded");
    case ECase::TooFar: return TEXT("TooFar");
    case ECase::Behind: return TEXT("Behind");
    case ECase::MissingObserver: return TEXT("MissingObserver");
    case ECase::ConflictingPolicies: return TEXT("ConflictingPolicies");
    case ECase::PrototypeCompatibility: return TEXT("PrototypeCompatibility");
    case ECase::InvalidSettings: return TEXT("InvalidSettings");
    default: return TEXT("Unknown");
    }
}

void APedroDirectObservationValidationHarness::BeginPlay()
{
    Super::BeginPlay();
    if (!bRunValidationInPIE)
    {
        return;
    }

    const UWorld* World = GetWorld();
    // An UNSAVED newly-created Editor level has an Untitled* world name.
    // Do not loosen this guard to test saved/production levels.
    if (!World || World->WorldType != EWorldType::PIE ||
        !World->GetMapName().Contains(TEXT("Untitled")))
    {
        UE_LOG(LogTemp, Error,
            TEXT("PEDRO_DIRECT_WORLD_TEST status=FAIL reason=RequiresUnsavedUntitledPIE"));
        return;
    }

    constexpr int32 Total = static_cast<int32>(ECase::Count);
    int32 Passed = 0;
    UE_LOG(LogTemp, Display,
        TEXT("PEDRO_DIRECT_WORLD_TEST status=BEGIN expected=%d mode=PIE"), Total);

    for (int32 Index = 0; Index < Total; ++Index)
    {
        const ECase Case = static_cast<ECase>(Index);
        if (RunCase(Case))
        {
            ++Passed;
            UE_LOG(LogTemp, Display,
                TEXT("PEDRO_DIRECT_WORLD_TEST status=PASS case=%s"), Name(Case));
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("PEDRO_DIRECT_WORLD_TEST status=FAIL case=%s"), Name(Case));
        }
    }
    UE_LOG(LogTemp, Display,
        TEXT("PEDRO_DIRECT_WORLD_TEST status=SUMMARY passed=%d failed=%d total=%d"),
        Passed, Total - Passed, Total);
}

bool APedroDirectObservationValidationHarness::RunCase(ECase Case)
{
    UWorld* World = GetWorld();
    if (!World)
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

    // All test actors are spawned and destroyed in this same PIE BeginPlay.
    // A spawned APedroEncounterDirector is transient to this case.
    ATargetPoint* Observer = nullptr;
    if (Case != ECase::MissingObserver)
    {
        Observer = World->SpawnActor<ATargetPoint>(
            FVector(0, 0, 100), FRotator::ZeroRotator, Params);
    }

    const FVector TargetLocation = Case == ECase::Behind
        ? FVector(-300, 0, 100) : FVector(300, 0, 100);
    AStaticMeshActor* Target = World->SpawnActor<AStaticMeshActor>(
        TargetLocation, FRotator::ZeroRotator, Params);
    AStaticMeshActor* Wall = nullptr;
    APedroEncounterDirector* Director = World->SpawnActor<APedroEncounterDirector>(
        FVector(0, 0, -1000), FRotator::ZeroRotator, Params);

    bool bValid = IsValid(Target) && IsValid(Director) &&
        (Case == ECase::MissingObserver || IsValid(Observer));

    if (bValid)
    {
        UStaticMeshComponent* TargetMesh = Target->GetStaticMeshComponent();
        bValid = IsValid(TargetMesh);
        if (bValid)
        {
            TargetMesh->SetMobility(EComponentMobility::Movable);
            TargetMesh->SetStaticMesh(Cube);
            TargetMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            TargetMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
            TargetMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        }
    }

    if (bValid && Case == ECase::Occluded)
    {
        Wall = World->SpawnActor<AStaticMeshActor>(
            FVector(150, 0, 100), FRotator::ZeroRotator, Params);
        bValid = IsValid(Wall) && IsValid(Wall->GetStaticMeshComponent());
        if (bValid)
        {
            UStaticMeshComponent* WallMesh = Wall->GetStaticMeshComponent();
            WallMesh->SetMobility(EComponentMobility::Movable);
            WallMesh->SetStaticMesh(Cube);
            Wall->SetActorScale3D(FVector(0.2, 3, 3));
            WallMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            WallMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
            WallMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        }
    }

    if (bValid)
    {
        Director->bUsePrototypeAssumedAcquisition =
            Case == ECase::ConflictingPolicies ||
            Case == ECase::PrototypeCompatibility;
        Director->bUseDirectObservationAcquisition =
            Case != ECase::PrototypeCompatibility;
        Director->DirectObservationActor = Observer;
        Director->DirectObservationMaxDistance =
            Case == ECase::TooFar ? 100.0f :
            Case == ECase::InvalidSettings ? 0.0f : 1000.0f;
        Director->DirectObservationViewHalfAngleDegrees = 60.0f;

        UPedroObservationSourceComponent* Source =
            NewObject<UPedroObservationSourceComponent>(Target);
        bValid = IsValid(Source);
        if (bValid)
        {
            Target->AddInstanceComponent(Source);
            Source->bEvidenceEnabled = true;
            Source->SubjectId = FName(TEXT("DirectFixtureProp"));
            Source->OpportunityId = FName(TEXT("DirectFixtureOpportunity"));
            Source->RegisterComponent();
            bValid = Director->RegisterObservationSource(Source);
        }

        if (bValid)
        {
            FPedroBehaviourEvent Sample;
            Sample.EventId = FGuid::NewGuid();
            Sample.Kind = EPedroEvidenceKind::InspectedOptionalObject;
            Sample.SubjectActor = Target;
            Sample.SubjectId = Source->SubjectId;
            Sample.OpportunityId = Source->OpportunityId;
            Sample.EventLocation = Target->GetActorLocation();

            const EPedroDirectObservationStatus GeometryAndTrace =
                PedroDirectObservation::Evaluate(World, Observer, Sample,
                    Director->DirectObservationMaxDistance,
                    Director->DirectObservationViewHalfAngleDegrees);

            EPedroDirectObservationStatus Expected =
                EPedroDirectObservationStatus::Visible;
            switch (Case)
            {
            case ECase::Occluded:
                Expected = EPedroDirectObservationStatus::Occluded;
                break;
            case ECase::TooFar:
                Expected = EPedroDirectObservationStatus::TooFar;
                break;
            case ECase::Behind:
                Expected = EPedroDirectObservationStatus::OutsideViewCone;
                break;
            case ECase::MissingObserver:
                Expected = EPedroDirectObservationStatus::MissingObserver;
                break;
            case ECase::InvalidSettings:
                Expected = EPedroDirectObservationStatus::InvalidSettings;
                break;
            default: break;
            }

            bValid = GeometryAndTrace == Expected &&
                Source->ReportConfirmedAction(
                    EPedroEvidenceKind::InspectedOptionalObject);

            const FPedroObservationState& State = Director->GetObservationState();
            const bool bShouldAcquire = Case == ECase::Visible ||
                Case == ECase::PrototypeCompatibility;
            const int32 ExpectedCount = bShouldAcquire ? 1 : 0;
            bValid = bValid && State.GetEvidenceCount() == ExpectedCount;
            if (bValid && bShouldAcquire)
            {
                const EPedroKnowledgeSource ExpectedSource =
                    Case == ECase::PrototypeCompatibility
                    ? EPedroKnowledgeSource::PrototypeAssumed
                    : EPedroKnowledgeSource::DirectObservation;
                bValid = State.EvidenceHistory[0].KnowledgeSource == ExpectedSource &&
                    State.EvidenceHistory[0].Event.SubjectId == Source->SubjectId;
            }
            UE_LOG(LogTemp, Display,
                TEXT("PEDRO_DIRECT_WORLD_TEST case=%s expectedTrace=%s actualTrace=%s "
                     "acquired=%d expectedAcquired=%d provenanceOK=%s"),
                Name(Case),
                PedroDirectObservation::StatusName(Expected),
                PedroDirectObservation::StatusName(GeometryAndTrace),
                State.GetEvidenceCount(), ExpectedCount,
                bValid ? TEXT("yes") : TEXT("no"));
        }
    }

    if (IsValid(Director)) Director->Destroy();
    if (IsValid(Target)) Target->Destroy();
    if (IsValid(Wall)) Wall->Destroy();
    if (IsValid(Observer)) Observer->Destroy();
    return bValid;
}
