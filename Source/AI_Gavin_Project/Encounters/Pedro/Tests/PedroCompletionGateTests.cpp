#include "Encounters/Pedro/PedroCompletionGate.h"
#include "Encounters/Pedro/PedroObservationState.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

#include <initializer_list>
#include <limits>

namespace PedroCompletionGateTests
{
FPedroOpportunityDefinition Definition(
    const TCHAR* Id,
    EPedroOpportunityTier Tier = EPedroOpportunityTier::Core)
{
    FPedroOpportunityDefinition Value;
    Value.OpportunityId = FName(Id);
    Value.Tier = Tier;
    return Value;
}

bool Configure(
    FPedroObservationState& State,
    int32 CoreCount = 4,
    bool bIncludeBonus = false)
{
    const TCHAR* CoreNames[] = {
        TEXT("Core_A"), TEXT("Core_B"), TEXT("Core_C"), TEXT("Core_D")
    };
    if (CoreCount < 0 || CoreCount > static_cast<int32>(UE_ARRAY_COUNT(CoreNames)))
    {
        return false;
    }

    TArray<FPedroOpportunityDefinition> Definitions;
    for (int32 Index = 0; Index < CoreCount; ++Index)
    {
        Definitions.Add(Definition(CoreNames[Index]));
    }
    if (bIncludeBonus)
    {
        Definitions.Add(Definition(
            TEXT("Bonus_X"), EPedroOpportunityTier::Bonus));
    }
    return State.Opportunities.Configure(Definitions);
}

FPedroEvidenceRecord MakeEvidence(
    const TCHAR* OpportunityId,
    EPedroEvidenceKind Kind = EPedroEvidenceKind::InspectedOptionalObject,
    EPedroKnowledgeSource Knowledge = EPedroKnowledgeSource::PrototypeAssumed)
{
    FPedroEvidenceRecord Evidence;
    Evidence.Event.EventId = FGuid::NewGuid();
    Evidence.Event.Kind = Kind;
    Evidence.Event.SubjectId = FName(TEXT("Subject_Test"));
    Evidence.Event.OpportunityId = FName(OpportunityId);
    Evidence.KnowledgeSource = Knowledge;
    return Evidence;
}

bool Record(
    FPedroObservationState& State,
    const TCHAR* OpportunityId,
    EPedroEvidenceKind Kind = EPedroEvidenceKind::InspectedOptionalObject)
{
    return State.RecordEvidence(MakeEvidence(OpportunityId, Kind));
}

bool Expose(FPedroObservationState& State, const TCHAR* OpportunityId)
{
    return State.Opportunities.MarkPlayerExposed(FName(OpportunityId));
}

bool Sample(
    FPedroObservationState& State,
    const TCHAR* OpportunityId,
    EPedroEvidenceKind Kind = EPedroEvidenceKind::InspectedOptionalObject)
{
    return Expose(State, OpportunityId) && Record(State, OpportunityId, Kind);
}
} // namespace PedroCompletionGateTests

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateEmptyTest,
    "AI_Gavin_Project.Pedro.CompletionGate.Empty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateEmptyTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const FPedroObservationState State;
    const auto Result = PedroCompletionGate::Evaluate(State, 0.75f);
    TestTrue(TEXT("No core definitions is reported"),
        Result.Status == EPedroCompletionStatus::NoCoreOpportunities);
    TestFalse(TEXT("Cannot conclude"), Result.CanConclude());
    TestFalse(TEXT("No classification returned"), Result.bHasClassification);
    TestEqual(TEXT("Zero required core samples"), Result.RequiredCoreSamples, 0);
    TestEqual(TEXT("Zero accepted evidence"), Result.AcquiredEvidenceCount, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateBonusOnlyTest,
    "AI_Gavin_Project.Pedro.CompletionGate.BonusOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateBonusOnlyTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Bonus-only configuration"),
        PedroCompletionGateTests::Configure(State, 0, true));
    TestTrue(TEXT("Bonus is sampled"),
        PedroCompletionGateTests::Sample(State, TEXT("Bonus_X")));
    const auto Result = PedroCompletionGate::Evaluate(State, 0.75f);
    TestTrue(TEXT("No core still blocks conclusion"),
        Result.Status == EPedroCompletionStatus::NoCoreOpportunities);
    TestEqual(TEXT("Bonus count visible"), Result.SampledBonusCount, 1);
    TestEqual(TEXT("Accepted evidence visible"), Result.AcquiredEvidenceCount, 1);
    TestFalse(TEXT("Cannot conclude"), Result.CanConclude());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateNoEvidenceTest,
    "AI_Gavin_Project.Pedro.CompletionGate.NoEvidence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateNoEvidenceTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Four core defined"), PedroCompletionGateTests::Configure(State));

    // Artificial inconsistent low-level flags may not bypass the evidence gate.
    for (const TCHAR* Id : { TEXT("Core_A"), TEXT("Core_B"), TEXT("Core_C") })
    {
        TestTrue(TEXT("Exposure set"), PedroCompletionGateTests::Expose(State, Id));
        TestTrue(TEXT("Low-level acquired flag set"),
            State.Opportunities.NoteAcquiredResponse(FName(Id)));
    }

    const auto Result = PedroCompletionGate::Evaluate(State, 0.75f);
    TestEqual(TEXT("Three sampled flags"), Result.SampledCoreCount, 3);
    TestTrue(TEXT("No accepted evidence blocks"),
        Result.Status == EPedroCompletionStatus::NoAcquiredEvidence);
    TestFalse(TEXT("No classification"), Result.bHasClassification);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateExposureOnlyTest,
    "AI_Gavin_Project.Pedro.CompletionGate.ExposureOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateExposureOnlyTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Configured"), PedroCompletionGateTests::Configure(State));
    for (const TCHAR* Id : { TEXT("Core_A"), TEXT("Core_B"), TEXT("Core_C") })
    {
        TestTrue(TEXT("Exposure"), PedroCompletionGateTests::Expose(State, Id));
    }
    const auto Result = PedroCompletionGate::Evaluate(State, 0.75f);
    TestEqual(TEXT("No core sampled"), Result.SampledCoreCount, 0);
    TestTrue(TEXT("No knowledge blocks"),
        Result.Status == EPedroCompletionStatus::NoAcquiredEvidence);
    TestFalse(TEXT("Cannot conclude"), Result.CanConclude());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateEvidenceOnlyTest,
    "AI_Gavin_Project.Pedro.CompletionGate.EvidenceOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateEvidenceOnlyTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Configured"), PedroCompletionGateTests::Configure(State));
    for (const TCHAR* Id : { TEXT("Core_A"), TEXT("Core_B"), TEXT("Core_C") })
    {
        TestTrue(TEXT("Evidence accepted"), PedroCompletionGateTests::Record(State, Id));
    }
    const auto Result = PedroCompletionGate::Evaluate(State, 0.75f);
    TestEqual(TEXT("Three acquired records"), Result.AcquiredEvidenceCount, 3);
    TestEqual(TEXT("No sampled opportunities"), Result.SampledCoreCount, 0);
    TestTrue(TEXT("Missing exposure blocks"),
        Result.Status == EPedroCompletionStatus::InsufficientCoreSamples);
    TestFalse(TEXT("Cannot conclude"), Result.CanConclude());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateBelowThresholdTest,
    "AI_Gavin_Project.Pedro.CompletionGate.BelowThreshold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateBelowThresholdTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Configured"), PedroCompletionGateTests::Configure(State));
    TestTrue(TEXT("Core A sampled"), PedroCompletionGateTests::Sample(State, TEXT("Core_A")));
    TestTrue(TEXT("Core B sampled"), PedroCompletionGateTests::Sample(State, TEXT("Core_B")));
    const auto Result = PedroCompletionGate::Evaluate(State, 0.75f);
    TestEqual(TEXT("Core total"), Result.CoreCount, 4);
    TestEqual(TEXT("Required 75 percent"), Result.RequiredCoreSamples, 3);
    TestEqual(TEXT("Sampled core count"), Result.SampledCoreCount, 2);
    TestTrue(TEXT("Threshold not met"),
        Result.Status == EPedroCompletionStatus::InsufficientCoreSamples);
    TestFalse(TEXT("Classifier withheld"), Result.bHasClassification);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateThresholdTest,
    "AI_Gavin_Project.Pedro.CompletionGate.ExactThreshold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateThresholdTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Configured"), PedroCompletionGateTests::Configure(State));
    TestTrue(TEXT("Core A sampled"), PedroCompletionGateTests::Sample(State, TEXT("Core_A")));
    TestTrue(TEXT("Core B sampled"), PedroCompletionGateTests::Sample(State, TEXT("Core_B")));
    TestTrue(TEXT("Core C sampled"), PedroCompletionGateTests::Sample(State, TEXT("Core_C")));
    const auto Result = PedroCompletionGate::Evaluate(State, 0.75f);
    TestTrue(TEXT("Ready at 3/4"), Result.CanConclude());
    TestTrue(TEXT("Status is Ready"), Result.Status == EPedroCompletionStatus::Ready);
    TestTrue(TEXT("Classifier has evidence"), Result.Classification.bHasEvidence);
    TestTrue(TEXT("Curiosity yields Seeking"),
        Result.Classification.Interpretation == EPedroInterpretation::Seeking);
    TestEqual(TEXT("Three core samples"), Result.SampledCoreCount, 3);
    TestEqual(TEXT("Fourth core not required"), Result.RequiredCoreSamples, 3);
    const auto AboveMax = PedroCompletionGate::Evaluate(State, 2.0f);
    TestEqual(TEXT("Finite fractions above one clamp to all four core"),
        AboveMax.RequiredCoreSamples, 4);
    TestFalse(TEXT("Upper clamp does not incorrectly conclude at three of four"),
        AboveMax.CanConclude());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateFourCoreTest,
    "AI_Gavin_Project.Pedro.CompletionGate.AllFourCore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateFourCoreTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Configured"), PedroCompletionGateTests::Configure(State));
    for (const TCHAR* Id : { TEXT("Core_A"), TEXT("Core_B"), TEXT("Core_C"), TEXT("Core_D") })
    {
        TestTrue(TEXT("Each core sampled"), PedroCompletionGateTests::Sample(State, Id));
    }
    const auto Result = PedroCompletionGate::Evaluate(State, 0.75f);
    TestEqual(TEXT("All four sampled"), Result.SampledCoreCount, 4);
    TestTrue(TEXT("Can conclude"), Result.CanConclude());
    const auto FullFraction = PedroCompletionGate::Evaluate(State, 1.0f);
    TestEqual(TEXT("At 100 percent require four"), FullFraction.RequiredCoreSamples, 4);
    TestTrue(TEXT("100 percent satisfied"), FullFraction.CanConclude());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateBonusDoesNotCountTest,
    "AI_Gavin_Project.Pedro.CompletionGate.BonusDoesNotCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateBonusDoesNotCountTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Configured"), PedroCompletionGateTests::Configure(State, 4, true));
    TestTrue(TEXT("Core A"), PedroCompletionGateTests::Sample(State, TEXT("Core_A")));
    TestTrue(TEXT("Core B"), PedroCompletionGateTests::Sample(State, TEXT("Core_B")));
    TestTrue(TEXT("Bonus"), PedroCompletionGateTests::Sample(State, TEXT("Bonus_X")));
    const auto Result = PedroCompletionGate::Evaluate(State, 0.75f);
    TestEqual(TEXT("Only two core samples"), Result.SampledCoreCount, 2);
    TestEqual(TEXT("One bonus sampled"), Result.SampledBonusCount, 1);
    TestTrue(TEXT("Bonus cannot finish encounter"),
        Result.Status == EPedroCompletionStatus::InsufficientCoreSamples);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateExposureLastTest,
    "AI_Gavin_Project.Pedro.CompletionGate.ExposureLast",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateExposureLastTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Configured"), PedroCompletionGateTests::Configure(State));
    TestTrue(TEXT("A acquired"), PedroCompletionGateTests::Record(State, TEXT("Core_A")));
    TestTrue(TEXT("B acquired"), PedroCompletionGateTests::Record(State, TEXT("Core_B")));
    TestTrue(TEXT("C acquired"), PedroCompletionGateTests::Record(State, TEXT("Core_C")));
    TestTrue(TEXT("A exposed"), PedroCompletionGateTests::Expose(State, TEXT("Core_A")));
    TestTrue(TEXT("B exposed"), PedroCompletionGateTests::Expose(State, TEXT("Core_B")));
    TestFalse(TEXT("Not ready before C exposure"),
        PedroCompletionGate::Evaluate(State, 0.75f).CanConclude());
    TestTrue(TEXT("C exposed"), PedroCompletionGateTests::Expose(State, TEXT("Core_C")));
    TestTrue(TEXT("Ready after later exposure"),
        PedroCompletionGate::Evaluate(State, 0.75f).CanConclude());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateAcquisitionLastTest,
    "AI_Gavin_Project.Pedro.CompletionGate.AcquisitionLast",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateAcquisitionLastTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Configured"), PedroCompletionGateTests::Configure(State));
    TestTrue(TEXT("A exposed"), PedroCompletionGateTests::Expose(State, TEXT("Core_A")));
    TestTrue(TEXT("B exposed"), PedroCompletionGateTests::Expose(State, TEXT("Core_B")));
    TestTrue(TEXT("C exposed"), PedroCompletionGateTests::Expose(State, TEXT("Core_C")));
    TestTrue(TEXT("A acquired"), PedroCompletionGateTests::Record(State, TEXT("Core_A")));
    TestTrue(TEXT("B acquired"), PedroCompletionGateTests::Record(State, TEXT("Core_B")));
    TestFalse(TEXT("Not ready before C acquisition"),
        PedroCompletionGate::Evaluate(State, 0.75f).CanConclude());
    TestTrue(TEXT("C acquired"), PedroCompletionGateTests::Record(State, TEXT("Core_C")));
    TestTrue(TEXT("Ready after later acquisition"),
        PedroCompletionGate::Evaluate(State, 0.75f).CanConclude());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateRepetitionTest,
    "AI_Gavin_Project.Pedro.CompletionGate.RepetitionAndDuplicate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateRepetitionTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Configured"), PedroCompletionGateTests::Configure(State));
    TestTrue(TEXT("Core A exposed"), PedroCompletionGateTests::Expose(State, TEXT("Core_A")));
    const auto First = PedroCompletionGateTests::MakeEvidence(TEXT("Core_A"));
    TestTrue(TEXT("First event accepted"), State.RecordEvidence(First));
    TestFalse(TEXT("Duplicate ID rejected"), State.RecordEvidence(First));
    for (int32 Index = 0; Index < 20; ++Index)
    {
        TestTrue(TEXT("Distinct repeat event accepted"),
            PedroCompletionGateTests::Record(
                State, TEXT("Core_A"), EPedroEvidenceKind::RepeatedInteraction));
    }
    const auto Result = PedroCompletionGate::Evaluate(State, 0.75f);
    TestEqual(TEXT("One core counted after spam"), Result.SampledCoreCount, 1);
    TestEqual(TEXT("21 unique accepted records"), Result.AcquiredEvidenceCount, 21);
    TestEqual(TEXT("Repetition trait capped"), State.TraitScores.Repetition, 3);
    TestFalse(TEXT("Cannot conclude on one core"), Result.CanConclude());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateProvenanceTest,
    "AI_Gavin_Project.Pedro.CompletionGate.Provenance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateProvenanceTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Configured"), PedroCompletionGateTests::Configure(State));
    for (const TCHAR* Id : { TEXT("Core_A"), TEXT("Core_B"), TEXT("Core_C") })
    {
        TestTrue(TEXT("Exposed"), PedroCompletionGateTests::Expose(State, Id));
        const auto Rejected = PedroCompletionGateTests::MakeEvidence(
            Id, EPedroEvidenceKind::HelpedNPC, EPedroKnowledgeSource::Reported);
        TestFalse(TEXT("Unsupported Reported evidence rejected"),
            State.RecordEvidence(Rejected));
    }
    const auto Result = PedroCompletionGate::Evaluate(State, 0.75f);
    TestEqual(TEXT("No accepted records"), Result.AcquiredEvidenceCount, 0);
    TestTrue(TEXT("Rejected provenance cannot conclude"),
        Result.Status == EPedroCompletionStatus::NoAcquiredEvidence);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateFractionTest,
    "AI_Gavin_Project.Pedro.CompletionGate.FractionRounding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateFractionTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Three core configured"), PedroCompletionGateTests::Configure(State, 3));
    TestTrue(TEXT("Core A"), PedroCompletionGateTests::Sample(State, TEXT("Core_A")));
    TestTrue(TEXT("Core B"), PedroCompletionGateTests::Sample(State, TEXT("Core_B")));
    auto Result = PedroCompletionGate::Evaluate(State, 0.75f);
    TestEqual(TEXT("75 percent of 3 rounds to 3"), Result.RequiredCoreSamples, 3);
    TestFalse(TEXT("Two of three cannot conclude"), Result.CanConclude());
    const auto BelowMin = PedroCompletionGate::Evaluate(State, 0.0f);
    TestEqual(TEXT("Finite zero fraction clamps to at least one core"),
        BelowMin.RequiredCoreSamples, 1);
    TestTrue(TEXT("Lower clamp allows two already sampled cores"),
        BelowMin.CanConclude());
    const auto AboveMax = PedroCompletionGate::Evaluate(State, 2.0f);
    TestEqual(TEXT("Finite fractions above one require all three core"),
        AboveMax.RequiredCoreSamples, 3);
    TestFalse(TEXT("Two of three cannot bypass upper clamp"),
        AboveMax.CanConclude());
    TestTrue(TEXT("Core C"), PedroCompletionGateTests::Sample(State, TEXT("Core_C")));
    Result = PedroCompletionGate::Evaluate(State, 0.75f);
    TestTrue(TEXT("Three of three can conclude"), Result.CanConclude());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateResetTest,
    "AI_Gavin_Project.Pedro.CompletionGate.Reset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateResetTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Configured"), PedroCompletionGateTests::Configure(State));
    TestTrue(TEXT("Core A"), PedroCompletionGateTests::Sample(State, TEXT("Core_A")));
    TestTrue(TEXT("Core B"), PedroCompletionGateTests::Sample(State, TEXT("Core_B")));
    TestTrue(TEXT("Core C"), PedroCompletionGateTests::Sample(State, TEXT("Core_C")));
    TestTrue(TEXT("Initially ready"), PedroCompletionGate::Evaluate(State, 0.75f).CanConclude());

    State.Reset();
    TestTrue(TEXT("Reconfigure after reset"), PedroCompletionGateTests::Configure(State));
    const auto Result = PedroCompletionGate::Evaluate(State, 0.75f);
    TestTrue(TEXT("Reset blocks fresh run"),
        Result.Status == EPedroCompletionStatus::NoAcquiredEvidence);
    TestEqual(TEXT("No stale evidence"), Result.AcquiredEvidenceCount, 0);
    TestEqual(TEXT("No stale core samples"), Result.SampledCoreCount, 0);
    TestFalse(TEXT("No interpretation from previous run"), Result.bHasClassification);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateClassificationTest,
    "AI_Gavin_Project.Pedro.CompletionGate.ClassificationForwarding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateClassificationTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    {
        FPedroObservationState State;
        TestTrue(TEXT("Gentle setup"), PedroCompletionGateTests::Configure(State));
        for (const TCHAR* Id : { TEXT("Core_A"), TEXT("Core_B"), TEXT("Core_C") })
        {
            TestTrue(TEXT("Help sampled"), PedroCompletionGateTests::Sample(
                State, Id, EPedroEvidenceKind::HelpedNPC));
        }
        const auto Result = PedroCompletionGate::Evaluate(State, 0.75f);
        TestTrue(TEXT("Gentle has ready candidate"), Result.CanConclude());
        TestTrue(TEXT("Gentle forwarded"),
            Result.Classification.Interpretation == EPedroInterpretation::Gentle);
    }
    {
        FPedroObservationState State;
        TestTrue(TEXT("Disruptive setup"), PedroCompletionGateTests::Configure(State));
        TestTrue(TEXT("Owned destruction sampled"), PedroCompletionGateTests::Sample(
            State, TEXT("Core_A"), EPedroEvidenceKind::DestroyedOwnedProperty));
        TestTrue(TEXT("NPC attack sampled"), PedroCompletionGateTests::Sample(
            State, TEXT("Core_B"), EPedroEvidenceKind::AttackedNPC));
        TestTrue(TEXT("Unowned destruction sampled"), PedroCompletionGateTests::Sample(
            State, TEXT("Core_C"), EPedroEvidenceKind::DestroyedUnownedProperty));
        const auto Result = PedroCompletionGate::Evaluate(State, 0.75f);
        TestTrue(TEXT("Disruptive has ready candidate"), Result.CanConclude());
        TestFalse(TEXT("Gentle disqualified"), Result.Classification.bGentleEligible);
        TestTrue(TEXT("Disruptive forwarded"),
            Result.Classification.Interpretation == EPedroInterpretation::Disruptive);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateInconsistentTest,
    "AI_Gavin_Project.Pedro.CompletionGate.InconsistentSample",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateInconsistentTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Configured"), PedroCompletionGateTests::Configure(State));
    TestTrue(TEXT("Unassociated knowledge accepted"),
        PedroCompletionGateTests::Record(State, TEXT("UnknownOpportunity")));
    for (const TCHAR* Id : { TEXT("Core_A"), TEXT("Core_B"), TEXT("Core_C") })
    {
        TestTrue(TEXT("Expose"), PedroCompletionGateTests::Expose(State, Id));
        TestTrue(TEXT("Artificial low-level acquired flag"),
            State.Opportunities.NoteAcquiredResponse(FName(Id)));
    }
    const auto Result = PedroCompletionGate::Evaluate(State, 0.75f);
    TestTrue(TEXT("Missing matching evidence detected"),
        Result.Status == EPedroCompletionStatus::InconsistentSampleEvidence);
    TestFalse(TEXT("Inconsistent state cannot finish"), Result.CanConclude());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroCompletionGateInvalidFractionTest,
    "AI_Gavin_Project.Pedro.CompletionGate.NonFiniteFraction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroCompletionGateInvalidFractionTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Configured"), PedroCompletionGateTests::Configure(State));
    TestTrue(TEXT("Core A"), PedroCompletionGateTests::Sample(State, TEXT("Core_A")));
    TestTrue(TEXT("Core B"), PedroCompletionGateTests::Sample(State, TEXT("Core_B")));
    TestTrue(TEXT("Core C"), PedroCompletionGateTests::Sample(State, TEXT("Core_C")));
    const float InvalidFractions[] = {
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()
    };
    for (const float InvalidFraction : InvalidFractions)
    {
        const auto Result = PedroCompletionGate::Evaluate(State, InvalidFraction);
        TestTrue(TEXT("NaN or infinity is rejected"),
            Result.Status == EPedroCompletionStatus::InvalidSampleFraction);
        TestFalse(TEXT("Non-finite fraction cannot conclude"), Result.CanConclude());
        TestFalse(TEXT("Non-finite fraction produces no classification"),
            Result.bHasClassification);
    }
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
