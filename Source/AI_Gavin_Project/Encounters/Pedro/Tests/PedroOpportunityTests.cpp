#include "Encounters/Pedro/PedroEncounterDirector.h"
#include "Encounters/Pedro/PedroObservationSourceComponent.h"
#include "Encounters/Pedro/PedroPrototypeInteractableActor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
FPedroOpportunityDefinition MakeOpportunityDefinitionForTest(
    const TCHAR* Id, EPedroOpportunityTier Tier)
{
    FPedroOpportunityDefinition Definition;
    Definition.OpportunityId = FName(Id);
    Definition.Tier = Tier;
    return Definition;
}

TArray<FPedroOpportunityDefinition> MakeFourCoreOneBonusForTest()
{
    TArray<FPedroOpportunityDefinition> Definitions;
    Definitions.Add(MakeOpportunityDefinitionForTest(
        TEXT("Core_A"), EPedroOpportunityTier::Core));
    Definitions.Add(MakeOpportunityDefinitionForTest(
        TEXT("Core_B"), EPedroOpportunityTier::Core));
    Definitions.Add(MakeOpportunityDefinitionForTest(
        TEXT("Core_C"), EPedroOpportunityTier::Core));
    Definitions.Add(MakeOpportunityDefinitionForTest(
        TEXT("Core_D"), EPedroOpportunityTier::Core));
    Definitions.Add(MakeOpportunityDefinitionForTest(
        TEXT("Bonus_X"), EPedroOpportunityTier::Bonus));
    return Definitions;
}

FPedroEvidenceRecord MakeOpportunityEvidenceForTest(
    FName OpportunityId,
    EPedroKnowledgeSource Source = EPedroKnowledgeSource::PrototypeAssumed)
{
    FPedroEvidenceRecord Evidence;
    Evidence.Event.EventId = FGuid::NewGuid();
    Evidence.Event.Kind = EPedroEvidenceKind::InspectedOptionalObject;
    Evidence.Event.SubjectId = FName(TEXT("Prop_Test"));
    Evidence.Event.OpportunityId = OpportunityId;
    Evidence.KnowledgeSource = Source;
    return Evidence;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroOpportunityDefinitionTest,
    "AI_Gavin_Project.Pedro.Opportunity.Definitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroOpportunityDefinitionTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroOpportunityState Progress;

    TestFalse(TEXT("No core definitions never reaches the threshold"),
        Progress.HasSufficientCoreSamples());
    TestEqual(TEXT("No configured core requires zero samples"),
        Progress.GetRequiredCoreSamples(), 0);

    const TArray<FPedroOpportunityDefinition> Valid = MakeFourCoreOneBonusForTest();
    TestTrue(TEXT("Valid authored opportunity list is accepted"),
        Progress.Configure(Valid));
    TestEqual(TEXT("Four core opportunities"), Progress.GetCoreCount(), 4);
    TestEqual(TEXT("One bonus opportunity"), Progress.GetBonusCount(), 1);
    TestEqual(TEXT("75 percent of four requires three"),
        Progress.GetRequiredCoreSamples(), 3);
    TestEqual(TEXT("100 percent of four requires four"),
        Progress.GetRequiredCoreSamples(1.0f), 4);
    TestEqual(TEXT("50 percent of four requires two"),
        Progress.GetRequiredCoreSamples(0.5f), 2);

    TArray<FPedroOpportunityDefinition> Invalid = Valid;
    Invalid.Add(MakeOpportunityDefinitionForTest(
        TEXT("Core_A"), EPedroOpportunityTier::Bonus));
    TestFalse(TEXT("Duplicate opportunity ID rejected"),
        Progress.Configure(Invalid));
    TestEqual(TEXT("Failed setup did not modify existing core count"),
        Progress.GetCoreCount(), 4);
    TestEqual(TEXT("Failed setup did not modify existing bonus count"),
        Progress.GetBonusCount(), 1);

    Invalid.Reset();
    Invalid.Add(MakeOpportunityDefinitionForTest(
        TEXT(""), EPedroOpportunityTier::Core));
    TestFalse(TEXT("None identity rejected"), Progress.Configure(Invalid));
    TestEqual(TEXT("Failed empty-ID setup is atomic"),
        Progress.Entries.Num(), 5);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroOpportunitySamplingTest,
    "AI_Gavin_Project.Pedro.Opportunity.Sampling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroOpportunitySamplingTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroOpportunityState Progress;
    TestTrue(TEXT("Setup succeeds"),
        Progress.Configure(MakeFourCoreOneBonusForTest()));

    TestFalse(TEXT("Unknown exposure ignored"),
        Progress.MarkPlayerExposed(FName(TEXT("Unknown"))));
    TestFalse(TEXT("Unknown evidence ignored"),
        Progress.NoteAcquiredResponse(FName(TEXT("Unknown"))));
    TestTrue(TEXT("World exposure recorded"),
        Progress.MarkPlayerExposed(FName(TEXT("Core_A"))));
    TestFalse(TEXT("Exposure alone does not sample a core opportunity"),
        Progress.HasSufficientCoreSamples());
    TestEqual(TEXT("Exposed without acquired evidence is not sampled"),
        Progress.GetSampledCoreCount(), 0);

    TestTrue(TEXT("Acquired response to exposed opportunity"),
        Progress.NoteAcquiredResponse(FName(TEXT("Core_A"))));
    TestFalse(TEXT("Repeated exposure cannot count twice"),
        Progress.MarkPlayerExposed(FName(TEXT("Core_A"))));
    TestFalse(TEXT("Repeated response cannot count twice"),
        Progress.NoteAcquiredResponse(FName(TEXT("Core_A"))));
    TestEqual(TEXT("One core sampled despite repeated reports"),
        Progress.GetSampledCoreCount(), 1);

    TestTrue(TEXT("Second core evidence may arrive before exposure"),
        Progress.NoteAcquiredResponse(FName(TEXT("Core_B"))));
    TestEqual(TEXT("Evidence without exposure is not a sample"),
        Progress.GetSampledCoreCount(), 1);
    TestTrue(TEXT("Later exposure completes second core sample"),
        Progress.MarkPlayerExposed(FName(TEXT("Core_B"))));
    TestEqual(TEXT("Two core samples"), Progress.GetSampledCoreCount(), 2);
    TestFalse(TEXT("75 percent not yet reached"),
        Progress.HasSufficientCoreSamples());

    Progress.MarkPlayerExposed(FName(TEXT("Bonus_X")));
    Progress.NoteAcquiredResponse(FName(TEXT("Bonus_X")));
    TestEqual(TEXT("One bonus sampled"), Progress.GetSampledBonusCount(), 1);
    TestEqual(TEXT("Bonus does not count toward core"),
        Progress.GetSampledCoreCount(), 2);

    Progress.MarkPlayerExposed(FName(TEXT("Core_C")));
    Progress.NoteAcquiredResponse(FName(TEXT("Core_C")));
    TestEqual(TEXT("Three core sampled"), Progress.GetSampledCoreCount(), 3);
    TestTrue(TEXT("75 percent now reached"),
        Progress.HasSufficientCoreSamples());
    TestFalse(TEXT("100 percent not yet reached"),
        Progress.HasSufficientCoreSamples(1.0f));

    Progress.Reset();
    TestEqual(TEXT("Reset removes authored opportunity instances"),
        Progress.Entries.Num(), 0);
    TestFalse(TEXT("Reset does not imply completion"),
        Progress.HasSufficientCoreSamples());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroOpportunityEvidenceGateTest,
    "AI_Gavin_Project.Pedro.Opportunity.EvidenceGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroOpportunityEvidenceGateTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Setup succeeds"),
        State.Opportunities.Configure(MakeFourCoreOneBonusForTest()));

    const FName CoreA(TEXT("Core_A"));
    State.Opportunities.MarkPlayerExposed(CoreA);
    FPedroEvidenceRecord Rejected = MakeOpportunityEvidenceForTest(
        CoreA, EPedroKnowledgeSource::Reported);
    TestFalse(TEXT("Unsupported gossip cannot sample an opportunity"),
        State.RecordEvidence(Rejected));
    TestEqual(TEXT("Unsupported knowledge left core unsampled"),
        State.Opportunities.GetSampledCoreCount(), 0);

    FPedroEvidenceRecord Valid = MakeOpportunityEvidenceForTest(CoreA);
    TestTrue(TEXT("Accepted knowledge samples exposed core"),
        State.RecordEvidence(Valid));
    TestFalse(TEXT("Duplicate occurrence rejected"),
        State.RecordEvidence(Valid));
    TestEqual(TEXT("One accepted event"), State.GetEvidenceCount(), 1);
    TestEqual(TEXT("One sampled opportunity"),
        State.Opportunities.GetSampledCoreCount(), 1);

    const FName CoreB(TEXT("Core_B"));
    TestTrue(TEXT("Accepted evidence before world exposure"),
        State.RecordEvidence(MakeOpportunityEvidenceForTest(CoreB)));
    TestEqual(TEXT("Unexposed core is not sampled"),
        State.Opportunities.GetSampledCoreCount(), 1);
    State.Opportunities.MarkPlayerExposed(CoreB);
    TestEqual(TEXT("Later exposure makes acquired core sampled"),
        State.Opportunities.GetSampledCoreCount(), 2);

    TestTrue(TEXT("Unconfigured events remain valid evidence"),
        State.RecordEvidence(MakeOpportunityEvidenceForTest(
            FName(TEXT("NotAuthored")))));
    TestEqual(TEXT("Unconfigured event does not inflate opportunity count"),
        State.Opportunities.GetSampledCoreCount(), 2);

    State.Reset();
    TestEqual(TEXT("Reset clears evidence"), State.GetEvidenceCount(), 0);
    TestEqual(TEXT("Reset clears opportunity definitions"),
        State.Opportunities.Entries.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroDirectorOpportunityIntegrationTest,
    "AI_Gavin_Project.Pedro.Opportunity.DirectorIntegration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroDirectorOpportunityIntegrationTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);

    APedroEncounterDirector* Director =
        NewObject<APedroEncounterDirector>(GetTransientPackage());
    APedroPrototypeInteractableActor* Prop =
        NewObject<APedroPrototypeInteractableActor>(GetTransientPackage());
    AActor* Interactor = NewObject<AActor>(GetTransientPackage());
    UPedroObservationSourceComponent* Source =
        Prop->FindComponentByClass<UPedroObservationSourceComponent>();
    TestNotNull(TEXT("Prototype source exists"), Source);
    if (!Source)
    {
        return false;
    }

    Director->OpportunityDefinitions = MakeFourCoreOneBonusForTest();
    TestTrue(TEXT("Director configured with authored opportunities"),
        Director->ObservationState.Opportunities.Configure(
            Director->OpportunityDefinitions));
    TestTrue(TEXT("Director subscribes once"),
        Director->RegisterObservationSource(Source));

    Source->SubjectId = FName(TEXT("Prop_A"));
    Source->OpportunityId = FName(TEXT("Core_A"));
    TestFalse(TEXT("Disabled source cannot report exposure"),
        Source->ReportOpportunityExposed());
    TestEqual(TEXT("Disabled source did not change world exposure"),
        Director->GetObservationState().Opportunities.GetSampledCoreCount(), 0);

    Source->bEvidenceEnabled = true;
    TestTrue(TEXT("Enabled source reports meaningful exposure"),
        Source->ReportOpportunityExposed());
    const FPedroOpportunityProgressEntry* Entry =
        Director->GetObservationState().Opportunities.Find(FName(TEXT("Core_A")));
    TestTrue(TEXT("Director received exposure"), Entry && Entry->bPlayerExposed);
    TestEqual(TEXT("Exposure without acquired response is not sampled"),
        Director->GetObservationState().Opportunities.GetSampledCoreCount(), 0);

    Prop->Interact_Implementation(Interactor);
    TestEqual(TEXT("Disabled Director acquisition still records no evidence"),
        Director->GetObservationState().GetEvidenceCount(), 0);
    TestEqual(TEXT("No observer evidence means no sample"),
        Director->GetObservationState().Opportunities.GetSampledCoreCount(), 0);

    Director->bUsePrototypeAssumedAcquisition = true;
    Prop->Interact_Implementation(Interactor);
    TestEqual(TEXT("Director acquired one confirmed action"),
        Director->GetObservationState().GetEvidenceCount(), 1);
    TestEqual(TEXT("Confirmed and exposed core becomes sampled"),
        Director->GetObservationState().Opportunities.GetSampledCoreCount(), 1);
    TestEqual(TEXT("Confirmed repetition contributes to interpretation"),
        Director->GetObservationState().TraitScores.Repetition, 1);

    Prop->Interact_Implementation(Interactor);
    TestEqual(TEXT("Another confirmed action remains in evidence history"),
        Director->GetObservationState().GetEvidenceCount(), 2);
    TestEqual(TEXT("Repeated activity cannot sample one core twice"),
        Director->GetObservationState().Opportunities.GetSampledCoreCount(), 1);

    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
