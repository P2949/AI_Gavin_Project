#include "Encounters/Pedro/PedroActiveWorldValidationHarness.h"

#include "Encounters/Pedro/PedroEncounterDirector.h"
#include "Encounters/Pedro/PedroObservationSourceComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "World/Encounters/EncounterProgressSubsystem.h"

APedroActiveWorldValidationHarness::APedroActiveWorldValidationHarness()
{
    PrimaryActorTick.bCanEverTick = false;
}

const TCHAR* APedroActiveWorldValidationHarness::CaseName() const
{
    switch (static_cast<ECase>(CaseIndex))
    {
    case ECase::Seeking: return TEXT("Seeking");
    case ECase::Gentle: return TEXT("Gentle");
    case ECase::Disruptive: return TEXT("Disruptive");
    case ECase::Reentrant: return TEXT("Reentrant");
    case ECase::MissingOutcome: return TEXT("MissingOutcome");
    case ECase::AlreadyResolved: return TEXT("AlreadyResolved");
    case ECase::InsufficientSamples: return TEXT("InsufficientSamples");
    case ECase::EmptyEncounterId: return TEXT("EmptyEncounterId");
    default: return TEXT("Unknown");
    }
}

bool APedroActiveWorldValidationHarness::IsSuccessCase() const
{
    const ECase Case = static_cast<ECase>(CaseIndex);
    return Case == ECase::Seeking || Case == ECase::Gentle ||
           Case == ECase::Disruptive || Case == ECase::Reentrant;
}

EPedroEvidenceKind APedroActiveWorldValidationHarness::CaseEvidenceKind() const
{
    switch (static_cast<ECase>(CaseIndex))
    {
    case ECase::Gentle: return EPedroEvidenceKind::HelpedNPC;
    case ECase::Disruptive: return EPedroEvidenceKind::AttackedNPC;
    default: return EPedroEvidenceKind::InspectedOptionalObject;
    }
}

void APedroActiveWorldValidationHarness::BeginPlay()
{
    Super::BeginPlay();
    if (!bRunValidationInPIE)
    {
        return;
    }
    UWorld* World = GetWorld();
    if (!World || World->WorldType != EWorldType::PIE)
    {
        UE_LOG(LogTemp, Error,
            TEXT("PEDRO_WORLD_TEST status=FAIL scenario=Setup reason=NotPIE"));
        return;
    }
    // Refuse production encounter maps, even if the fixture is enabled by mistake.
    const FString MapName = World->GetMapName();
    if (MapName.Contains(TEXT("L_Pedro_Prototype")) ||
        MapName.Contains(TEXT("L_Hub_Prototype")))
    {
        UE_LOG(LogTemp, Error,
            TEXT("PEDRO_WORLD_TEST status=FAIL scenario=Setup reason=ProductionMap"));
        return;
    }
    UGameInstance* GameInstance = World->GetGameInstance();
    Progress = GameInstance
        ? GameInstance->GetSubsystem<UEncounterProgressSubsystem>() : nullptr;
    if (!IsValid(Progress))
    {
        UE_LOG(LogTemp, Error,
            TEXT("PEDRO_WORLD_TEST status=FAIL scenario=Setup reason=NoProgressSubsystem"));
        return;
    }
    Progress->OnEncounterResolved.AddDynamic(
        this, &APedroActiveWorldValidationHarness::OnEncounterResolved);
    UE_LOG(LogTemp, Display,
        TEXT("PEDRO_WORLD_TEST status=BEGIN expected=8 mode=PIE actualLOS=no"));
    World->GetTimerManager().SetTimerForNextTick(
        this, &APedroActiveWorldValidationHarness::StartNextCase);
}

void APedroActiveWorldValidationHarness::AssertCondition(
    bool bCondition, const TCHAR* Description)
{
    if (!bCondition)
    {
        ++CurrentFailures;
        UE_LOG(LogTemp, Error,
            TEXT("PEDRO_WORLD_TEST status=ASSERT_FAIL scenario=%s check=%s"),
            CaseName(), Description);
    }
}

void APedroActiveWorldValidationHarness::StartNextCase()
{
    if (CaseIndex >= static_cast<int32>(ECase::Count))
    {
        UE_LOG(LogTemp, Display,
            TEXT("PEDRO_WORLD_TEST status=SUMMARY passed=%d failed=%d total=%d"),
            PassedCases, FailedCases, PassedCases + FailedCases);
        return;
    }

    CurrentFailures = 0;
    ResolutionBroadcasts = 0;
    bCallbackObservedResolvedState = true;
    const ECase Case = static_cast<ECase>(CaseIndex);
    UWorld* World = GetWorld();
    if (!IsValid(World) || !IsValid(Progress))
    {
        AssertCondition(false, TEXT("RuntimeDependencies"));
        VerifyCurrentCase();
        return;
    }

    Definition = NewObject<UPedroWorldValidationEncounterDefinition>(this);
    AssertCondition(IsValid(Definition), TEXT("DefinitionCreated"));
    if (!IsValid(Definition))
    {
        VerifyCurrentCase();
        return;
    }
    if (Case != ECase::EmptyEncounterId)
    {
        const FString Identifier = FString::Printf(
            TEXT("PedroFixture_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
        Definition->SetValidationId(FName(*Identifier));
    }

    const FActorSpawnParameters Parameters = []
    {
        FActorSpawnParameters P;
        P.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return P;
    }();

    // Three genuine spawned actors in the PIE world. The engine's built-in
    // cube is only a debug visualization; no project assets are authored.
    UStaticMesh* Cube = LoadObject<UStaticMesh>(
        nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    AssertCondition(IsValid(Cube), TEXT("EngineCubeLoaded"));
    const float Offset = 500.f * CaseIndex;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        AStaticMeshActor* Target = World->SpawnActor<AStaticMeshActor>(
            AStaticMeshActor::StaticClass(),
            FVector(Offset + 120.f * Index, 300.f, 80.f),
            FRotator::ZeroRotator, Parameters);
        AssertCondition(IsValid(Target), TEXT("OutcomeActorSpawned"));
        if (IsValid(Target))
        {
            if (UStaticMeshComponent* Mesh = Target->GetStaticMeshComponent())
            {
                Mesh->SetMobility(EComponentMobility::Movable);
                Mesh->SetStaticMesh(Cube);
            }
            Target->SetActorScale3D(FVector(0.6f));
            // Even a deliberately unassigned target must start hidden.
            Target->SetActorHiddenInGame(true);
            Target->SetActorEnableCollision(false);
            SpawnedActors.Add(Target);
            if (Index == 0) GentleActor = Target;
            if (Index == 1) SeekingActor = Target;
            if (Index == 2) DisruptiveActor = Target;
        }
    }
    if (CurrentFailures != 0)
    {
        VerifyCurrentCase();
        return;
    }

    // Configure before BeginPlay by using deferred actor spawning.
    const FTransform DirectorTransform(
        FRotator::ZeroRotator, FVector(Offset, 0.f, 0.f));
    Director = World->SpawnActorDeferred<APedroEncounterDirector>(
        APedroEncounterDirector::StaticClass(), DirectorTransform,
        this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    AssertCondition(IsValid(Director), TEXT("DirectorSpawnedDeferred"));
    if (!IsValid(Director))
    {
        VerifyCurrentCase();
        return;
    }

    Director->CompletionPolicy = EPedroCompletionPolicy::EvidenceDriven;
    Director->bUsePrototypeAssumedAcquisition = true;
    Director->bEnableCompletionGateShadowEvaluation = true;
    Director->CoreSampleFraction = 0.75f;
    Director->EncounterDefinition = Definition.Get();
    Director->GentleOutcomeActor = GentleActor.Get();
    Director->SeekingOutcomeActor = SeekingActor.Get();
    Director->DisruptiveOutcomeActor =
        Case == ECase::MissingOutcome ? nullptr : DisruptiveActor.Get();
    for (const TCHAR* Id :
         { TEXT("Core_A"), TEXT("Core_B"), TEXT("Core_C"), TEXT("Core_D") })
    {
        FPedroOpportunityDefinition Item;
        Item.OpportunityId = FName(Id);
        Item.Tier = EPedroOpportunityTier::Core;
        Director->OpportunityDefinitions.Add(Item);
    }
    UGameplayStatics::FinishSpawningActor(Director, DirectorTransform);
    AssertCondition(IsValid(Director), TEXT("DirectorFinishedSpawning"));
    if (CurrentFailures != 0)
    {
        VerifyCurrentCase();
        return;
    }

    // Pre-resolve only this fixture's unique ID. Never reset session state.
    if (Case == ECase::AlreadyResolved)
    {
        AssertCondition(Progress->ResolveEncounter(Definition),
            TEXT("PreResolutionAccepted"));
    }
    BroadcastsBeforeEvents = ResolutionBroadcasts;
    ResolvedCountBeforeEvents = Progress->GetResolvedEncounterCount();

    if (Case == ECase::InsufficientSamples)
    {
        for (int32 I = 0; I < 3; ++I)
        {
            EmitResponse(FName(TEXT("Core_A")), CaseEvidenceKind());
        }
    }
    else
    {
        EmitResponse(FName(TEXT("Core_A")), CaseEvidenceKind());
        EmitResponse(FName(TEXT("Core_B")), CaseEvidenceKind());
        EmitResponse(FName(TEXT("Core_C")), CaseEvidenceKind());
    }

    AssertCondition(!Director->HasFinalClassification(),
        TEXT("NotFinalizedBeforeNextTick"));
    World->GetTimerManager().SetTimerForNextTick(
        this, &APedroActiveWorldValidationHarness::WaitOneTick);
}

void APedroActiveWorldValidationHarness::EmitResponse(
    FName OpportunityId, EPedroEvidenceKind Kind)
{
    UWorld* World = GetWorld();
    if (!IsValid(World) || !IsValid(Director))
    {
        AssertCondition(false, TEXT("EmitDependencies"));
        return;
    }
    UPedroObservationSourceComponent* Source = nullptr;
    // Reuse the first source to test repeated actions on the SAME core.
    if (OpportunityId == FName(TEXT("Core_A")) && IsValid(FirstSource))
    {
        Source = FirstSource.Get();
    }
    else
    {
        AActor* SourceOwner = World->SpawnActor<AActor>(
            AActor::StaticClass(), FVector(0.f, 700.f, 50.f),
            FRotator::ZeroRotator);
        AssertCondition(IsValid(SourceOwner), TEXT("SourceActorSpawned"));
        if (!IsValid(SourceOwner)) return;
        SpawnedActors.Add(SourceOwner);
        Source = NewObject<UPedroObservationSourceComponent>(SourceOwner);
        Source->bEvidenceEnabled = true;
        Source->SubjectId = OpportunityId;
        Source->OpportunityId = OpportunityId;
        SourceOwner->AddInstanceComponent(Source);
        Source->RegisterComponent();
        AssertCondition(Director->RegisterObservationSource(Source),
            TEXT("SourceRegistered"));
        if (OpportunityId == FName(TEXT("Core_A"))) FirstSource = Source;
    }
    AssertCondition(Source->ReportOpportunityExposed(), TEXT("ExposureReported"));
    AssertCondition(Source->ReportConfirmedAction(Kind), TEXT("ActionReported"));
}

void APedroActiveWorldValidationHarness::WaitOneTick()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimerForNextTick(
            this, &APedroActiveWorldValidationHarness::VerifyCurrentCase);
    }
}

void APedroActiveWorldValidationHarness::OnEncounterResolved(
    UEncounterDefinition* ResolvedDefinition)
{
    if (!IsValid(Definition) || ResolvedDefinition != Definition) return;
    ++ResolutionBroadcasts;
    bCallbackObservedResolvedState &=
        IsValid(Progress) && Progress->IsEncounterResolved(Definition);
    if (static_cast<ECase>(CaseIndex) == ECase::Reentrant &&
        IsValid(Director) && IsValid(FirstSource))
    {
        // Same-stack reentry while ResolveEncounter is broadcasting.
        // Director must already be committed/locked and ignore new evidence.
        Director->EvaluateActiveCompletion();
        FirstSource->ReportConfirmedAction(EPedroEvidenceKind::RepeatedInteraction);
    }
}

void APedroActiveWorldValidationHarness::VerifyCurrentCase()
{
    if (IsValid(Director) && IsValid(Progress) && IsValid(Definition))
    {
        const bool bSuccess = IsSuccessCase();
        const int32 ExpectedNewResolutions = bSuccess ? 1 : 0;
        const bool bResolved = Progress->IsEncounterResolved(Definition);
        AssertCondition(bResolved ==
            (bSuccess || static_cast<ECase>(CaseIndex) == ECase::AlreadyResolved),
            TEXT("ProgressResolutionState"));
        AssertCondition(Progress->GetResolvedEncounterCount() ==
            ResolvedCountBeforeEvents + ExpectedNewResolutions,
            TEXT("ExactlyOnceProgressCount"));
        AssertCondition(ResolutionBroadcasts - BroadcastsBeforeEvents ==
            ExpectedNewResolutions, TEXT("ExactlyOnceBroadcast"));
        AssertCondition(bCallbackObservedResolvedState,
            TEXT("StateCommittedBeforeBroadcast"));
        AssertCondition(Director->HasFinalClassification() == bSuccess,
            TEXT("ClassificationCommittedOnlyOnSuccess"));
        AssertCondition(Director->bHasPresentedOutcome == bSuccess,
            TEXT("PresentedOnlyOnSuccess"));

        EPedroInterpretation Interpretation = EPedroInterpretation::Seeking;
        switch (static_cast<ECase>(CaseIndex))
        {
        case ECase::Gentle: Interpretation = EPedroInterpretation::Gentle; break;
        case ECase::Disruptive: Interpretation = EPedroInterpretation::Disruptive; break;
        default: break;
        }
        if (bSuccess)
        {
            AssertCondition(Director->GetFinalClassification().Interpretation ==
                Interpretation, TEXT("FrozenInterpretation"));
        }
        const AActor* Targets[] = {GentleActor.Get(), SeekingActor.Get(), DisruptiveActor.Get()};
        for (int32 I = 0; I < 3; ++I)
        {
            if (!IsValid(Targets[I])) continue;
            const bool bSelected =
                bSuccess && static_cast<int32>(Interpretation) == I;
            AssertCondition(Targets[I]->IsHidden() != bSelected,
                TEXT("OnlySelectedActorVisible"));
            AssertCondition(Targets[I]->GetActorEnableCollision() == bSelected,
                TEXT("OnlySelectedActorCollidable"));
        }
        if (bSuccess && IsValid(FirstSource))
        {
            const int32 Before = Director->GetObservationState().GetEvidenceCount();
            FirstSource->ReportConfirmedAction(EPedroEvidenceKind::RepeatedInteraction);
            Director->EvaluateActiveCompletion();
            AssertCondition(Director->GetObservationState().GetEvidenceCount() == Before,
                TEXT("PostCompletionEvidenceIgnored"));
            AssertCondition(ResolutionBroadcasts - BroadcastsBeforeEvents == 1,
                TEXT("PostCompletionNoSecondBroadcast"));
        }
        if (static_cast<ECase>(CaseIndex) == ECase::InsufficientSamples)
        {
            AssertCondition(
                Director->GetObservationState().Opportunities.GetSampledCoreCount() == 1,
                TEXT("RepetitionNotMultipleSamples"));
        }
    }
    else
    {
        AssertCondition(false, TEXT("FixtureCreated"));
    }

    if (CurrentFailures == 0)
    {
        ++PassedCases;
        UE_LOG(LogTemp, Display,
            TEXT("PEDRO_WORLD_TEST status=PASS scenario=%s"), CaseName());
    }
    else
    {
        ++FailedCases;
        UE_LOG(LogTemp, Error,
            TEXT("PEDRO_WORLD_TEST status=FAIL scenario=%s failures=%d"),
            CaseName(), CurrentFailures);
    }
    TearDownCurrentCase();
    ++CaseIndex;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimerForNextTick(
            this, &APedroActiveWorldValidationHarness::StartNextCase);
    }
}

void APedroActiveWorldValidationHarness::TearDownCurrentCase()
{
    if (IsValid(Director)) Director->Destroy();
    Director = nullptr;
    FirstSource = nullptr;
    for (const TObjectPtr<AActor>& Actor : SpawnedActors)
    {
        if (IsValid(Actor.Get())) Actor->Destroy();
    }
    SpawnedActors.Reset();
    GentleActor = nullptr;
    SeekingActor = nullptr;
    DisruptiveActor = nullptr;
    Definition = nullptr;
}

void APedroActiveWorldValidationHarness::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(Progress))
    {
        Progress->OnEncounterResolved.RemoveDynamic(
            this, &APedroActiveWorldValidationHarness::OnEncounterResolved);
    }
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearAllTimersForObject(this);
    }
    TearDownCurrentCase();
    Super::EndPlay(EndPlayReason);
}
