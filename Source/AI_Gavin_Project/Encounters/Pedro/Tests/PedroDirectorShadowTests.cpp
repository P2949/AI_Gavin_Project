#include "Encounters/Pedro/PedroEncounterDirector.h"
#include "Encounters/Pedro/PedroObservationSourceComponent.h"
#include "Encounters/Pedro/PedroPrototypeInteractableActor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

// Narrow test-only access to initialize observation state without running
// BeginPlay on a worldless transient actor.
struct FPedroDirectorShadowTestAccess
{
    static bool Configure(APedroEncounterDirector* Director)
    {
        return Director->ObservationState.Opportunities.Configure(
            Director->OpportunityDefinitions);
    }
};

namespace PedroDirectorShadowTests
{
TArray<FPedroOpportunityDefinition> FourCoreDefinitions()
{
    TArray<FPedroOpportunityDefinition> Definitions;
    for (const TCHAR* Id :
         { TEXT("Core_A"), TEXT("Core_B"), TEXT("Core_C"), TEXT("Core_D") })
    {
        FPedroOpportunityDefinition Definition;
        Definition.OpportunityId = FName(Id);
        Definition.Tier = EPedroOpportunityTier::Core;
        Definitions.Add(Definition);
    }
    return Definitions;
}

APedroEncounterDirector* NewDirector()
{
    APedroEncounterDirector* Director =
        NewObject<APedroEncounterDirector>(GetTransientPackage());
    if (!Director)
    {
        return nullptr;
    }
    Director->OpportunityDefinitions = FourCoreDefinitions();
    Director->bUsePrototypeAssumedAcquisition = true;
    if (!FPedroDirectorShadowTestAccess::Configure(Director))
    {
        return nullptr;
    }
    return Director;
}

UPedroObservationSourceComponent* NewSource(
    APedroEncounterDirector* Director, const TCHAR* Id)
{
    if (!Director)
    {
        return nullptr;
    }
    APedroPrototypeInteractableActor* Prop =
        NewObject<APedroPrototypeInteractableActor>(GetTransientPackage());
    if (!Prop)
    {
        return nullptr;
    }
    UPedroObservationSourceComponent* Source =
        Prop->FindComponentByClass<UPedroObservationSourceComponent>();
    if (!Source)
    {
        return nullptr;
    }
    Source->bEvidenceEnabled = true;
    Source->SubjectId = FName(Id);
    Source->OpportunityId = FName(Id);
    return Director->RegisterObservationSource(Source) ? Source : nullptr;
}

bool Expose(UPedroObservationSourceComponent* Source)
{
    return Source && Source->ReportOpportunityExposed();
}

bool Acquire(UPedroObservationSourceComponent* Source)
{
    return Source &&
        Source->ReportConfirmedAction(EPedroEvidenceKind::InspectedOptionalObject);
}
} // namespace PedroDirectorShadowTests

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroDirectorShadowAcquisitionRequiredTest,
    "AI_Gavin_Project.Pedro.DirectorShadow.AcquisitionRequired",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroDirectorShadowAcquisitionRequiredTest::RunTest(
    const FString& Parameters)
{
    static_cast<void>(Parameters);
    APedroEncounterDirector* Director = PedroDirectorShadowTests::NewDirector();
    TestNotNull(TEXT("Director created"), Director);
    if (!Director) return false;
    Director->bUsePrototypeAssumedAcquisition = false;

    auto* Source = PedroDirectorShadowTests::NewSource(Director, TEXT("Core_A"));
    TestNotNull(TEXT("Registered source"), Source);
    if (!Source) return false;

    TestTrue(TEXT("Exposure event emitted"), PedroDirectorShadowTests::Expose(Source));
    TestTrue(TEXT("World fact emitted"), PedroDirectorShadowTests::Acquire(Source));
    TestTrue(TEXT("Shadow evaluation ran on exposure"),
        Director->HasShadowCompletionDecision());
    TestTrue(TEXT("Without acquisition, no evidence"),
        Director->GetShadowCompletionDecision().Status ==
            EPedroCompletionStatus::NoAcquiredEvidence);
    TestEqual(TEXT("World truth is not assumed observer knowledge"),
        Director->GetObservationState().GetEvidenceCount(), 0);
    TestFalse(TEXT("No conclusion"),
        Director->GetShadowCompletionDecision().CanConclude());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroDirectorShadowExposureFirstTest,
    "AI_Gavin_Project.Pedro.DirectorShadow.ExposureThenEvidence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroDirectorShadowExposureFirstTest::RunTest(
    const FString& Parameters)
{
    static_cast<void>(Parameters);
    APedroEncounterDirector* Director = PedroDirectorShadowTests::NewDirector();
    TestNotNull(TEXT("Director created"), Director);
    if (!Director) return false;

    for (const TCHAR* Id : { TEXT("Core_A"), TEXT("Core_B") })
    {
        auto* Source = PedroDirectorShadowTests::NewSource(Director, Id);
        TestNotNull(TEXT("Source created"), Source);
        if (!Source) return false;
        TestTrue(TEXT("Exposed"), PedroDirectorShadowTests::Expose(Source));
        TestTrue(TEXT("Acquired"), PedroDirectorShadowTests::Acquire(Source));
    }
    TestTrue(TEXT("Shadow snapshot exists"), Director->HasShadowCompletionDecision());
    TestEqual(TEXT("Two core sampled"),
        Director->GetShadowCompletionDecision().SampledCoreCount, 2);
    TestFalse(TEXT("Two of four are not sufficient"),
        Director->GetShadowCompletionDecision().CanConclude());

    auto* Last = PedroDirectorShadowTests::NewSource(Director, TEXT("Core_C"));
    TestNotNull(TEXT("Third source"), Last);
    if (!Last) return false;
    TestTrue(TEXT("Third exposure"), PedroDirectorShadowTests::Expose(Last));
    TestFalse(TEXT("Exposure without acquired response is not sufficient"),
        Director->GetShadowCompletionDecision().CanConclude());
    TestTrue(TEXT("Third acquisition"), PedroDirectorShadowTests::Acquire(Last));
    const auto& Decision = Director->GetShadowCompletionDecision();
    TestTrue(TEXT("Shadow gate is ready at three of four"), Decision.CanConclude());
    TestEqual(TEXT("Three core sampled"), Decision.SampledCoreCount, 3);
    TestEqual(TEXT("Three core required"), Decision.RequiredCoreSamples, 3);
    TestTrue(TEXT("Inspection evidence is classified as Seeking"),
        Decision.Classification.Interpretation == EPedroInterpretation::Seeking);
    TestEqual(TEXT("Shadow never emits legacy clicks"),
        Director->GetObservationState().InteractionCount, 0);
    TestEqual(TEXT("Only accepted evidence counted"),
        Director->GetObservationState().GetEvidenceCount(), 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroDirectorShadowEvidenceFirstTest,
    "AI_Gavin_Project.Pedro.DirectorShadow.EvidenceThenExposure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroDirectorShadowEvidenceFirstTest::RunTest(
    const FString& Parameters)
{
    static_cast<void>(Parameters);
    APedroEncounterDirector* Director = PedroDirectorShadowTests::NewDirector();
    TestNotNull(TEXT("Director created"), Director);
    if (!Director) return false;

    TArray<UPedroObservationSourceComponent*> Sources;
    for (const TCHAR* Id : { TEXT("Core_A"), TEXT("Core_B"), TEXT("Core_C") })
    {
        auto* Source = PedroDirectorShadowTests::NewSource(Director, Id);
        TestNotNull(TEXT("Source created"), Source);
        if (!Source) return false;
        Sources.Add(Source);
        TestTrue(TEXT("Acquired before exposure"), PedroDirectorShadowTests::Acquire(Source));
    }
    TestTrue(TEXT("All evidence acquired but no choices exposed"),
        Director->GetShadowCompletionDecision().Status ==
            EPedroCompletionStatus::InsufficientCoreSamples);
    TestEqual(TEXT("Zero sampled despite evidence"),
        Director->GetShadowCompletionDecision().SampledCoreCount, 0);
    TestTrue(TEXT("First exposure"), PedroDirectorShadowTests::Expose(Sources[0]));
    TestTrue(TEXT("Second exposure"), PedroDirectorShadowTests::Expose(Sources[1]));
    TestFalse(TEXT("Still below gate"),
        Director->GetShadowCompletionDecision().CanConclude());
    TestTrue(TEXT("Third exposure"), PedroDirectorShadowTests::Expose(Sources[2]));
    TestTrue(TEXT("Ready after late exposure"),
        Director->GetShadowCompletionDecision().CanConclude());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroDirectorShadowDisabledTest,
    "AI_Gavin_Project.Pedro.DirectorShadow.Disabled",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroDirectorShadowDisabledTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    APedroEncounterDirector* Director = PedroDirectorShadowTests::NewDirector();
    TestNotNull(TEXT("Director created"), Director);
    if (!Director) return false;
    Director->bEnableCompletionGateShadowEvaluation = false;
    auto* Source = PedroDirectorShadowTests::NewSource(Director, TEXT("Core_A"));
    TestNotNull(TEXT("Source created"), Source);
    if (!Source) return false;
    TestTrue(TEXT("Exposed"), PedroDirectorShadowTests::Expose(Source));
    TestTrue(TEXT("Acquired"), PedroDirectorShadowTests::Acquire(Source));
    TestFalse(TEXT("Disabled shadow does not create snapshot"),
        Director->HasShadowCompletionDecision());
    TestEqual(TEXT("Acquisition still records accepted evidence"),
        Director->GetObservationState().GetEvidenceCount(), 1);
    TestEqual(TEXT("Opportunity progress is unchanged by diagnostic switch"),
        Director->GetObservationState().Opportunities.GetSampledCoreCount(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroDirectorShadowDuplicateLifecycleTest,
    "AI_Gavin_Project.Pedro.DirectorShadow.DuplicatesAndPostOutcome",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroDirectorShadowDuplicateLifecycleTest::RunTest(
    const FString& Parameters)
{
    static_cast<void>(Parameters);
    APedroEncounterDirector* Director = PedroDirectorShadowTests::NewDirector();
    TestNotNull(TEXT("Director created"), Director);
    if (!Director) return false;
    auto* Source = PedroDirectorShadowTests::NewSource(Director, TEXT("Core_A"));
    TestNotNull(TEXT("Source created"), Source);
    if (!Source) return false;
    TestTrue(TEXT("Exposed"), PedroDirectorShadowTests::Expose(Source));

    FPedroBehaviourEvent Event;
    Event.EventId = FGuid::NewGuid();
    Event.Kind = EPedroEvidenceKind::InspectedOptionalObject;
    Event.SubjectId = FName(TEXT("Source_A"));
    Event.OpportunityId = FName(TEXT("Core_A"));
    Source->OnWorldEventProduced.Broadcast(Event);
    TestEqual(TEXT("Original fact acquired"),
        Director->GetObservationState().GetEvidenceCount(), 1);
    Source->OnWorldEventProduced.Broadcast(Event);
    TestEqual(TEXT("Duplicate event ID rejected"),
        Director->GetObservationState().GetEvidenceCount(), 1);
    TestEqual(TEXT("Duplicate cannot add samples"),
        Director->GetShadowCompletionDecision().SampledCoreCount, 1);
    TestEqual(TEXT("Duplicate cannot add evidence"),
        Director->GetShadowCompletionDecision().AcquiredEvidenceCount, 1);

    Director->bHasPresentedOutcome = true;
    Event.EventId = FGuid::NewGuid();
    Source->OnWorldEventProduced.Broadcast(Event);
    Source->OnOpportunityExposed.Broadcast(FName(TEXT("Core_B")));
    TestEqual(TEXT("Post-outcome fact is ignored"),
        Director->GetObservationState().GetEvidenceCount(), 1);
    TestEqual(TEXT("Post-outcome diagnostics remain unchanged"),
        Director->GetShadowCompletionDecision().AcquiredEvidenceCount, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroDirectorShadowLegacyTest,
    "AI_Gavin_Project.Pedro.DirectorShadow.LegacyRemainsAuthoritative",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroDirectorShadowLegacyTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    APedroEncounterDirector* Director = PedroDirectorShadowTests::NewDirector();
    TestNotNull(TEXT("Director created"), Director);
    if (!Director) return false;

    for (const TCHAR* Id : { TEXT("Core_A"), TEXT("Core_B"), TEXT("Core_C") })
    {
        auto* Source = PedroDirectorShadowTests::NewSource(Director, Id);
        TestNotNull(TEXT("Source created"), Source);
        if (!Source) return false;
        TestTrue(TEXT("Exposure"), PedroDirectorShadowTests::Expose(Source));
        TestTrue(TEXT("Acquisition"), PedroDirectorShadowTests::Acquire(Source));
    }
    TestTrue(TEXT("Shadow says ready"),
        Director->GetShadowCompletionDecision().CanConclude());
    TestFalse(TEXT("Shadow readiness cannot present outcome"),
        Director->bHasPresentedOutcome);
    TestEqual(TEXT("Shadow never increments legacy count"),
        Director->GetObservationState().InteractionCount, 0);

    // Existing worldless Director legacy path finalizes on its threshold.
    // It intentionally doesn't require real presentation actors in this unit
    // test; PIE validates the real outcome actors and shared progression.
    Director->InteractionsBeforeOutcome = 2;
    Director->UniqueInteractionsForExploratoryOutcome = 2;
    AActor* Target = NewObject<AActor>(GetTransientPackage());
    TestNotNull(TEXT("Interaction target"), Target);
    if (!Target) return false;
    Director->HandleInteractionDispatched(Target, Target);
    TestFalse(TEXT("First legacy click does not conclude"),
        Director->bHasPresentedOutcome);
    Director->HandleInteractionDispatched(Target, Target);
    TestTrue(TEXT("Second legacy click concludes"),
        Director->bHasPresentedOutcome);
    TestEqual(TEXT("Legacy threshold recorded"),
        Director->GetObservationState().InteractionCount, 2);
    Director->HandleInteractionDispatched(Target, Target);
    TestEqual(TEXT("Post-outcome dispatch ignored"),
        Director->GetObservationState().InteractionCount, 2);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
