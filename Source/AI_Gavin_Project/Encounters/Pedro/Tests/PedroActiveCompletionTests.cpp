#include "Encounters/Pedro/PedroActivePresentation.h"
#include "Encounters/Pedro/PedroEncounterDirector.h"
#include "Encounters/Pedro/PedroObservationSourceComponent.h"
#include "Encounters/Pedro/PedroPrototypeInteractableActor.h"
#include "World/Encounters/EncounterDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

struct FPedroActiveDirectorTestAccess
{
    static bool Configure(APedroEncounterDirector* Director)
    {
        return Director->ObservationState.Opportunities.Configure(
            Director->OpportunityDefinitions);
    }

    static void Dispatch(APedroEncounterDirector* Director, AActor* Target)
    {
        Director->HandleInteractionDispatched(Target, Target);
    }

    static void Evaluate(APedroEncounterDirector* Director)
    {
        Director->EvaluateActiveCompletion();
    }

    static bool HasPresented(const APedroEncounterDirector* Director)
    {
        return Director->bHasPresentedOutcome;
    }

    static void SetEncounter(APedroEncounterDirector* Director)
    {
        Director->EncounterDefinition = NewObject<UEncounterDefinition>(Director);
    }

    static void SetTargets(APedroEncounterDirector* Director)
    {
        Director->GentleOutcomeActor = NewObject<AActor>(GetTransientPackage());
        Director->SeekingOutcomeActor = NewObject<AActor>(GetTransientPackage());
        Director->DisruptiveOutcomeActor = NewObject<AActor>(GetTransientPackage());
    }
};

namespace PedroActiveTests
{
struct FThreeActors
{
    AActor* Gentle = nullptr;
    AActor* Seeking = nullptr;
    AActor* Disruptive = nullptr;
};

FThreeActors MakeActors()
{
    FThreeActors Actors;
    Actors.Gentle = NewObject<AActor>(GetTransientPackage());
    Actors.Seeking = NewObject<AActor>(GetTransientPackage());
    Actors.Disruptive = NewObject<AActor>(GetTransientPackage());
    return Actors;
}

FPedroCompletionDecision Ready(EPedroInterpretation Interpretation)
{
    FPedroCompletionDecision Decision;
    Decision.Status = EPedroCompletionStatus::Ready;
    Decision.bHasClassification = true;
    Decision.Classification.bHasEvidence = true;
    Decision.Classification.Interpretation = Interpretation;
    return Decision;
}

APedroEncounterDirector* NewActiveDirector()
{
    APedroEncounterDirector* Director =
        NewObject<APedroEncounterDirector>(GetTransientPackage());
    if (!Director)
    {
        return nullptr;
    }
    Director->CompletionPolicy = EPedroCompletionPolicy::EvidenceDriven;
    Director->bUsePrototypeAssumedAcquisition = true;
    Director->CoreSampleFraction = 0.75f;
    for (const TCHAR* Id :
         { TEXT("Core_A"), TEXT("Core_B"), TEXT("Core_C"), TEXT("Core_D") })
    {
        FPedroOpportunityDefinition Definition;
        Definition.OpportunityId = FName(Id);
        Definition.Tier = EPedroOpportunityTier::Core;
        Director->OpportunityDefinitions.Add(Definition);
    }
    if (!FPedroActiveDirectorTestAccess::Configure(Director))
    {
        return nullptr;
    }
    return Director;
}

UPedroObservationSourceComponent* Source(APedroEncounterDirector* Director,
                                        const TCHAR* Id)
{
    APedroPrototypeInteractableActor* Prop =
        NewObject<APedroPrototypeInteractableActor>(GetTransientPackage());
    if (!Prop)
    {
        return nullptr;
    }
    auto* Component = Prop->FindComponentByClass<UPedroObservationSourceComponent>();
    if (!Component)
    {
        return nullptr;
    }
    Component->bEvidenceEnabled = true;
    Component->SubjectId = FName(Id);
    Component->OpportunityId = FName(Id);
    return Director->RegisterObservationSource(Component) ? Component : nullptr;
}
} // namespace PedroActiveTests

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroActiveGateNotReadyTest,
    "AI_Gavin_Project.Pedro.ActivePresentation.GateNotReady",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroActiveGateNotReadyTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const auto Actors = PedroActiveTests::MakeActors();
    FPedroCompletionDecision NotReady;
    const auto Plan = PedroActivePresentation::Prepare(
        NotReady, Actors.Gentle, Actors.Seeking, Actors.Disruptive);
    TestFalse(TEXT("Not-ready gate cannot produce presentation"), Plan.IsReady());
    TestTrue(TEXT("Gate status preserved"),
        Plan.Status == EPedroPresentationStatus::GateNotReady);
    // A Ready status without a classification remains ineligible.
    FPedroCompletionDecision MissingClassification;
    MissingClassification.Status = EPedroCompletionStatus::Ready;
    const auto NoClassification = PedroActivePresentation::Prepare(
        MissingClassification, Actors.Gentle, Actors.Seeking, Actors.Disruptive);
    TestFalse(TEXT("Ready status alone is not sufficient"),
        NoClassification.IsReady());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroActiveMissingActorTest,
    "AI_Gavin_Project.Pedro.ActivePresentation.MissingActor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroActiveMissingActorTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const auto Actors = PedroActiveTests::MakeActors();
    const auto Plan = PedroActivePresentation::Prepare(
        PedroActiveTests::Ready(EPedroInterpretation::Seeking),
        Actors.Gentle, Actors.Seeking, nullptr);
    TestFalse(TEXT("Even unused third actor is required"), Plan.IsReady());
    TestTrue(TEXT("Missing actor status"),
        Plan.Status == EPedroPresentationStatus::MissingOutcomeActor);
    const auto MissingGentle = PedroActivePresentation::Prepare(
        PedroActiveTests::Ready(EPedroInterpretation::Seeking),
        nullptr, Actors.Seeking, Actors.Disruptive);
    TestTrue(TEXT("Missing Gentle actor is rejected"),
        MissingGentle.Status == EPedroPresentationStatus::MissingOutcomeActor);
    const auto MissingSeeking = PedroActivePresentation::Prepare(
        PedroActiveTests::Ready(EPedroInterpretation::Gentle),
        Actors.Gentle, nullptr, Actors.Disruptive);
    TestTrue(TEXT("Missing Seeking actor is rejected"),
        MissingSeeking.Status == EPedroPresentationStatus::MissingOutcomeActor);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroActiveDuplicateActorTest,
    "AI_Gavin_Project.Pedro.ActivePresentation.DuplicateActor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroActiveDuplicateActorTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const auto Actors = PedroActiveTests::MakeActors();
    const auto Plan = PedroActivePresentation::Prepare(
        PedroActiveTests::Ready(EPedroInterpretation::Seeking),
        Actors.Gentle, Actors.Gentle, Actors.Disruptive);
    TestFalse(TEXT("Duplicate targets rejected"), Plan.IsReady());
    TestTrue(TEXT("Duplicate target status"),
        Plan.Status == EPedroPresentationStatus::DuplicateOutcomeActor);
    const auto GentleDisruptive = PedroActivePresentation::Prepare(
        PedroActiveTests::Ready(EPedroInterpretation::Seeking),
        Actors.Gentle, Actors.Seeking, Actors.Gentle);
    TestTrue(TEXT("Gentle/Disruptive alias is rejected"),
        GentleDisruptive.Status == EPedroPresentationStatus::DuplicateOutcomeActor);
    const auto SeekingDisruptive = PedroActivePresentation::Prepare(
        PedroActiveTests::Ready(EPedroInterpretation::Seeking),
        Actors.Gentle, Actors.Seeking, Actors.Seeking);
    TestTrue(TEXT("Seeking/Disruptive alias is rejected"),
        SeekingDisruptive.Status == EPedroPresentationStatus::DuplicateOutcomeActor);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroActiveGentleMappingTest,
    "AI_Gavin_Project.Pedro.ActivePresentation.Gentle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroActiveGentleMappingTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const auto Actors = PedroActiveTests::MakeActors();
    const auto Plan = PedroActivePresentation::Prepare(
        PedroActiveTests::Ready(EPedroInterpretation::Gentle),
        Actors.Gentle, Actors.Seeking, Actors.Disruptive);
    TestTrue(TEXT("Gentle plan ready"), Plan.IsReady());
    TestTrue(TEXT("Gentle selects only Gentle actor"),
        Plan.SelectedActor == Actors.Gentle);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroActiveSeekingMappingTest,
    "AI_Gavin_Project.Pedro.ActivePresentation.Seeking",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroActiveSeekingMappingTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const auto Actors = PedroActiveTests::MakeActors();
    const auto Plan = PedroActivePresentation::Prepare(
        PedroActiveTests::Ready(EPedroInterpretation::Seeking),
        Actors.Gentle, Actors.Seeking, Actors.Disruptive);
    TestTrue(TEXT("Seeking plan ready"), Plan.IsReady());
    TestTrue(TEXT("Seeking selects only Seeking actor"),
        Plan.SelectedActor == Actors.Seeking);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroActiveDisruptiveMappingTest,
    "AI_Gavin_Project.Pedro.ActivePresentation.Disruptive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroActiveDisruptiveMappingTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const auto Actors = PedroActiveTests::MakeActors();
    const auto Plan = PedroActivePresentation::Prepare(
        PedroActiveTests::Ready(EPedroInterpretation::Disruptive),
        Actors.Gentle, Actors.Seeking, Actors.Disruptive);
    TestTrue(TEXT("Disruptive plan ready"), Plan.IsReady());
    TestTrue(TEXT("Disruptive selects only Disruptive actor"),
        Plan.SelectedActor == Actors.Disruptive);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroActiveLegacyDefaultTest,
    "AI_Gavin_Project.Pedro.DirectorActive.LegacyDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroActiveLegacyDefaultTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    auto* Director = NewObject<APedroEncounterDirector>(GetTransientPackage());
    TestNotNull(TEXT("Director"), Director);
    if (!Director) return false;
    TestTrue(TEXT("Legacy policy remains default"),
        Director->CompletionPolicy == EPedroCompletionPolicy::LegacyInteractionCount);
    AActor* Target = NewObject<AActor>(GetTransientPackage());
    FPedroActiveDirectorTestAccess::Dispatch(Director, Target);
    TestEqual(TEXT("Legacy click still counted"),
        Director->GetObservationState().InteractionCount, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroActiveIgnoresClicksTest,
    "AI_Gavin_Project.Pedro.DirectorActive.IgnoresLegacyClicks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroActiveIgnoresClicksTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    auto* Director = PedroActiveTests::NewActiveDirector();
    TestNotNull(TEXT("Director"), Director);
    if (!Director) return false;
    AActor* Target = NewObject<AActor>(GetTransientPackage());
    for (int32 I = 0; I < 8; ++I)
    {
        FPedroActiveDirectorTestAccess::Dispatch(Director, Target);
    }
    TestEqual(TEXT("Active policy does not consume legacy clicks"),
        Director->GetObservationState().InteractionCount, 0);
    TestFalse(TEXT("Clicks alone cannot present an outcome"),
        FPedroActiveDirectorTestAccess::HasPresented(Director));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroActiveInsufficientEvidenceTest,
    "AI_Gavin_Project.Pedro.DirectorActive.InsufficientEvidence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroActiveInsufficientEvidenceTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    auto* Director = PedroActiveTests::NewActiveDirector();
    TestNotNull(TEXT("Director"), Director);
    if (!Director) return false;
    FPedroActiveDirectorTestAccess::SetTargets(Director);
    FPedroActiveDirectorTestAccess::SetEncounter(Director);
    for (const TCHAR* Id : {TEXT("Core_A"), TEXT("Core_B")})
    {
        auto* Source = PedroActiveTests::Source(Director, Id);
        TestNotNull(TEXT("Source"), Source);
        if (!Source) return false;
        TestTrue(TEXT("Exposure"), Source->ReportOpportunityExposed());
        TestTrue(TEXT("Evidence"),
            Source->ReportConfirmedAction(EPedroEvidenceKind::InspectedOptionalObject));
    }
    TestEqual(TEXT("Two samples"),
        Director->GetObservationState().Opportunities.GetSampledCoreCount(), 2);
    TestFalse(TEXT("Not yet resolved"),
        FPedroActiveDirectorTestAccess::HasPresented(Director));
    TestFalse(TEXT("No final classification"), Director->HasFinalClassification());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroActiveMissingWorldTest,
    "AI_Gavin_Project.Pedro.DirectorActive.MissingWorldFailsClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroActiveMissingWorldTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    auto* Director = PedroActiveTests::NewActiveDirector();
    TestNotNull(TEXT("Director"), Director);
    if (!Director) return false;
    FPedroActiveDirectorTestAccess::SetTargets(Director);
    FPedroActiveDirectorTestAccess::SetEncounter(Director);
    for (const TCHAR* Id : {TEXT("Core_A"), TEXT("Core_B"), TEXT("Core_C")})
    {
        auto* Source = PedroActiveTests::Source(Director, Id);
        TestNotNull(TEXT("Source"), Source);
        if (!Source) return false;
        TestTrue(TEXT("Exposure"), Source->ReportOpportunityExposed());
        TestTrue(TEXT("Evidence"),
            Source->ReportConfirmedAction(EPedroEvidenceKind::InspectedOptionalObject));
    }
    TestTrue(TEXT("Gate would be ready"),
        PedroCompletionGate::Evaluate(
            Director->GetObservationState(), Director->CoreSampleFraction)
            .CanConclude());
    FPedroActiveDirectorTestAccess::Evaluate(Director);
    TestFalse(TEXT("Without a real World no presentation"),
        FPedroActiveDirectorTestAccess::HasPresented(Director));
    TestFalse(TEXT("No interpretation committed without a World"),
        Director->HasFinalClassification());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
