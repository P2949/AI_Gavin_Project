#include "Encounters/Pedro/PedroClassification.h"
#include "Encounters/Pedro/PedroObservationState.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
FPedroEvidenceRecord MakeEvidence(
    EPedroEvidenceKind Kind,
    const TCHAR* Subject,
    const TCHAR* Opportunity = TEXT(""))
{
    FPedroEvidenceRecord Evidence;
    Evidence.Event.EventId = FGuid::NewGuid();
    Evidence.Event.Kind = Kind;
    Evidence.Event.SubjectId = FName(Subject);
    Evidence.Event.OpportunityId = FName(Opportunity);
    Evidence.KnowledgeSource = EPedroKnowledgeSource::DirectObservation;
    return Evidence;
}

bool Record(
    FPedroObservationState& State,
    EPedroEvidenceKind Kind,
    const TCHAR* Subject,
    const TCHAR* Opportunity = TEXT(""))
{
    return State.RecordEvidence(MakeEvidence(Kind, Subject, Opportunity));
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroClassificationEmptyTest,
    "AI_Gavin_Project.Pedro.Classification.Empty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroClassificationEmptyTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const FPedroObservationState State;
    const FPedroClassificationResult Result = PedroClassification::Evaluate(State);
    TestFalse(TEXT("No evidence is explicitly reported"), Result.bHasEvidence);
    TestTrue(TEXT("Empty case has documented Seeking fallback"),
        Result.Interpretation == EPedroInterpretation::Seeking);
    TestTrue(TEXT("Gentle remains eligible"), Result.bGentleEligible);
    TestEqual(TEXT("Gentle score is zero"), Result.GentleScore, 0);
    TestEqual(TEXT("Seeking score is zero"), Result.SeekingScore, 0);
    TestEqual(TEXT("Disruptive score is zero"), Result.DisruptiveScore, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroClassificationGentleTest,
    "AI_Gavin_Project.Pedro.Classification.Gentle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroClassificationGentleTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("First real help"), Record(State,
        EPedroEvidenceKind::HelpedNPC, TEXT("NPC_A"), TEXT("Request_A")));
    TestTrue(TEXT("Independent help"), Record(State,
        EPedroEvidenceKind::HelpedNPC, TEXT("NPC_A"), TEXT("Request_B")));
    TestTrue(TEXT("Patient solution"), Record(State,
        EPedroEvidenceKind::SolvedPuzzleMethodically, TEXT("Puzzle_A")));
    const auto Result = PedroClassification::Evaluate(State);
    TestTrue(TEXT("Gentle selected"),
        Result.Interpretation == EPedroInterpretation::Gentle);
    TestTrue(TEXT("Gentle eligible"), Result.bGentleEligible);
    TestTrue(TEXT("Evidence exists"), Result.bHasEvidence);
    TestEqual(TEXT("Two help events contribute independently"),
        State.TraitScores.Care, 2);
    TestEqual(TEXT("Expected provisional Gentle score"),
        Result.GentleScore, 5);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroClassificationSeekingTest,
    "AI_Gavin_Project.Pedro.Classification.Seeking",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroClassificationSeekingTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Inspect object"), Record(State,
        EPedroEvidenceKind::InspectedOptionalObject, TEXT("Prop_A")));
    TestTrue(TEXT("Discover secret"), Record(State,
        EPedroEvidenceKind::DiscoveredSecret, TEXT("Secret_A")));
    TestTrue(TEXT("Experiment with puzzle"), Record(State,
        EPedroEvidenceKind::SolvedPuzzleByTrialAndError, TEXT("Puzzle_A")));
    TestTrue(TEXT("Repeat interaction"), Record(State,
        EPedroEvidenceKind::RepeatedInteraction, TEXT("Prop_A")));
    const auto Result = PedroClassification::Evaluate(State);
    TestTrue(TEXT("Seeking selected"),
        Result.Interpretation == EPedroInterpretation::Seeking);
    TestEqual(TEXT("Expected Seeking score"), Result.SeekingScore, 6);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroClassificationDisruptiveTest,
    "AI_Gavin_Project.Pedro.Classification.Disruptive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroClassificationDisruptiveTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Owned property broken"), Record(State,
        EPedroEvidenceKind::DestroyedOwnedProperty, TEXT("OwnedVase")));
    TestTrue(TEXT("NPC attacked"), Record(State,
        EPedroEvidenceKind::AttackedNPC, TEXT("NPC_A")));
    const auto Result = PedroClassification::Evaluate(State);
    TestFalse(TEXT("Harm makes Gentle ineligible"), Result.bGentleEligible);
    TestTrue(TEXT("Disruptive selected"),
        Result.Interpretation == EPedroInterpretation::Disruptive);
    TestEqual(TEXT("Expected Disruptive score"), Result.DisruptiveScore, 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroClassificationDisqualifiersTest,
    "AI_Gavin_Project.Pedro.Classification.Disqualifiers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroClassificationDisqualifiersTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const EPedroEvidenceKind Disqualifiers[] = {
        EPedroEvidenceKind::AttackedNPC,
        EPedroEvidenceKind::AttackedObserver,
        EPedroEvidenceKind::KeptRequestedItemCruelly,
        EPedroEvidenceKind::DestroyedOwnedProperty
    };
    for (const EPedroEvidenceKind Kind : Disqualifiers)
    {
        FPedroObservationState State;
        TestTrue(TEXT("First help"), Record(State,
            EPedroEvidenceKind::HelpedNPC, TEXT("NPC_A"), TEXT("Request_A")));
        TestTrue(TEXT("Second help"), Record(State,
            EPedroEvidenceKind::HelpedNPC, TEXT("NPC_B"), TEXT("Request_B")));
        TestTrue(TEXT("Disqualifying event accepted"),
            Record(State, Kind, TEXT("Subject_D")));
        const auto Result = PedroClassification::Evaluate(State);
        TestFalse(TEXT("Gentle disqualified regardless of prior help"),
            Result.bGentleEligible);
        TestTrue(TEXT("Gentle is never selected when disqualified"),
            Result.Interpretation != EPedroInterpretation::Gentle);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroClassificationUnownedPropertyTest,
    "AI_Gavin_Project.Pedro.Classification.UnownedProperty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroClassificationUnownedPropertyTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("NPC helped"), Record(State,
        EPedroEvidenceKind::HelpedNPC, TEXT("NPC_A")));
    TestTrue(TEXT("Unowned property broken"), Record(State,
        EPedroEvidenceKind::DestroyedUnownedProperty, TEXT("Debris")));
    const auto Result = PedroClassification::Evaluate(State);
    TestTrue(TEXT("Unowned destruction does not disqualify Gentle"),
        Result.bGentleEligible);
    TestTrue(TEXT("Help can outweigh unowned disruption"),
        Result.Interpretation == EPedroInterpretation::Gentle);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroClassificationRepetitionTest,
    "AI_Gavin_Project.Pedro.Classification.RepetitionCap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroClassificationRepetitionTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    for (int32 Index = 0; Index < 20; ++Index)
    {
        TestTrue(TEXT("Distinct repeat event accepted"), Record(State,
            EPedroEvidenceKind::RepeatedInteraction, TEXT("Prop_A")));
    }
    TestEqual(TEXT("All events retained"), State.GetEvidenceCount(), 20);
    TestEqual(TEXT("Single source contributes at most three repetition points"),
        State.TraitScores.Repetition, 3);
    auto Result = PedroClassification::Evaluate(State);
    TestEqual(TEXT("Classifier uses capped points"), Result.SeekingScore, 3);
    TestTrue(TEXT("Seeking selected"),
        Result.Interpretation == EPedroInterpretation::Seeking);
    TestTrue(TEXT("Independent source accepted"), Record(State,
        EPedroEvidenceKind::RepeatedInteraction, TEXT("Prop_B")));
    Result = PedroClassification::Evaluate(State);
    TestEqual(TEXT("Second source contributes another point"),
        Result.SeekingScore, 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroClassificationDuplicateTest,
    "AI_Gavin_Project.Pedro.Classification.Duplicate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroClassificationDuplicateTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    const FPedroEvidenceRecord Evidence = MakeEvidence(
        EPedroEvidenceKind::HelpedNPC, TEXT("NPC_A"));
    TestTrue(TEXT("Original event accepted"), State.RecordEvidence(Evidence));
    const auto Before = PedroClassification::Evaluate(State);
    TestFalse(TEXT("Same event ID rejected"), State.RecordEvidence(Evidence));
    const auto After = PedroClassification::Evaluate(State);
    TestEqual(TEXT("One evidence event"), State.GetEvidenceCount(), 1);
    TestEqual(TEXT("Gentle score unchanged"), After.GentleScore, Before.GentleScore);
    TestEqual(TEXT("Seeking score unchanged"), After.SeekingScore, Before.SeekingScore);
    TestEqual(TEXT("Disruptive score unchanged"),
        After.DisruptiveScore, Before.DisruptiveScore);
    TestTrue(TEXT("Interpretation unchanged"),
        After.Interpretation == Before.Interpretation);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroClassificationTieTest,
    "AI_Gavin_Project.Pedro.Classification.Ties",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroClassificationTieTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Help"), Record(State,
        EPedroEvidenceKind::HelpedNPC, TEXT("NPC_A")));
    TestTrue(TEXT("Inspect"), Record(State,
        EPedroEvidenceKind::InspectedOptionalObject, TEXT("Prop_A")));
    auto Result = PedroClassification::Evaluate(State);
    TestEqual(TEXT("Scores tied"), Result.GentleScore, Result.SeekingScore);
    TestTrue(TEXT("Seeking wins Gentle/Seeking tie"),
        Result.Interpretation == EPedroInterpretation::Seeking);

    State.Reset();
    TestTrue(TEXT("Help"), Record(State,
        EPedroEvidenceKind::HelpedNPC, TEXT("NPC_A")));
    TestTrue(TEXT("Patient solution"), Record(State,
        EPedroEvidenceKind::SolvedPuzzleMethodically, TEXT("Puzzle")));
    TestTrue(TEXT("Break unowned A"), Record(State,
        EPedroEvidenceKind::DestroyedUnownedProperty, TEXT("Unowned_A")));
    TestTrue(TEXT("Break unowned B"), Record(State,
        EPedroEvidenceKind::DestroyedUnownedProperty, TEXT("Unowned_B")));
    Result = PedroClassification::Evaluate(State);
    TestTrue(TEXT("Still Gentle-eligible"), Result.bGentleEligible);
    TestEqual(TEXT("Gentle and Disruptive scores tied"),
        Result.GentleScore, Result.DisruptiveScore);
    TestTrue(TEXT("Gentle wins Gentle/Disruptive tie"),
        Result.Interpretation == EPedroInterpretation::Gentle);

    State.Reset();
    TestTrue(TEXT("Inspect"), Record(State,
        EPedroEvidenceKind::InspectedOptionalObject, TEXT("Prop_A")));
    TestTrue(TEXT("Break unowned"), Record(State,
        EPedroEvidenceKind::DestroyedUnownedProperty, TEXT("Debris")));
    Result = PedroClassification::Evaluate(State);
    TestEqual(TEXT("Seeking and Disruptive scores tied"),
        Result.SeekingScore, Result.DisruptiveScore);
    TestTrue(TEXT("Seeking wins Seeking/Disruptive tie"),
        Result.Interpretation == EPedroInterpretation::Seeking);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroClassificationMixedAndResetTest,
    "AI_Gavin_Project.Pedro.Classification.MixedAndReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroClassificationMixedAndResetTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Help"), Record(State,
        EPedroEvidenceKind::HelpedNPC, TEXT("NPC_A")));
    TestTrue(TEXT("Inspect"), Record(State,
        EPedroEvidenceKind::InspectedOptionalObject, TEXT("Prop_A")));
    TestTrue(TEXT("Break unowned"), Record(State,
        EPedroEvidenceKind::DestroyedUnownedProperty, TEXT("Debris")));
    TestTrue(TEXT("Patient solution"), Record(State,
        EPedroEvidenceKind::SolvedPuzzleMethodically, TEXT("Puzzle_A")));
    TestTrue(TEXT("Attack observer"), Record(State,
        EPedroEvidenceKind::AttackedObserver, TEXT("Pedro")));
    const auto Before = PedroClassification::Evaluate(State);
    TestFalse(TEXT("Observer attack disqualifies Gentle"),
        Before.bGentleEligible);
    TestTrue(TEXT("Disruptive wins mixed case"),
        Before.Interpretation == EPedroInterpretation::Disruptive);
    State.Reset();
    const auto After = PedroClassification::Evaluate(State);
    TestFalse(TEXT("Reset removes evidence"), After.bHasEvidence);
    TestTrue(TEXT("Reset restores Gentle eligibility"), After.bGentleEligible);
    TestTrue(TEXT("Reset restores deterministic fallback"),
        After.Interpretation == EPedroInterpretation::Seeking);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroClassificationProvenanceTest,
    "AI_Gavin_Project.Pedro.Classification.Provenance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroClassificationProvenanceTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    auto Invalid = MakeEvidence(EPedroEvidenceKind::HelpedNPC, TEXT("NPC_A"));
    Invalid.KnowledgeSource = EPedroKnowledgeSource::Reported;
    TestFalse(TEXT("Unimplemented gossip is rejected"), State.RecordEvidence(Invalid));
    Invalid.KnowledgeSource = EPedroKnowledgeSource::Inferred;
    TestFalse(TEXT("Unimplemented inference is rejected"), State.RecordEvidence(Invalid));
    auto Result = PedroClassification::Evaluate(State);
    TestFalse(TEXT("Rejected events cannot count as acquired knowledge"),
        Result.bHasEvidence);
    TestEqual(TEXT("Rejected events cannot contribute score"),
        Result.GentleScore, 0);

    Invalid.KnowledgeSource = EPedroKnowledgeSource::PrototypeAssumed;
    TestTrue(TEXT("Explicit provisional provenance is accepted"),
        State.RecordEvidence(Invalid));
    Result = PedroClassification::Evaluate(State);
    TestTrue(TEXT("Acquired event now present"), Result.bHasEvidence);
    TestTrue(TEXT("Accepted help is interpreted Gentle"),
        Result.Interpretation == EPedroInterpretation::Gentle);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
