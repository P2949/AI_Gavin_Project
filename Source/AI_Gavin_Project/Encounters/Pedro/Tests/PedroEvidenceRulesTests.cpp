#include "Encounters/Pedro/PedroObservationState.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
FPedroEvidenceRecord MakeEvidence(
    EPedroEvidenceKind Kind,
    FName SubjectId,
    FName OpportunityId = NAME_None)
{
    FPedroEvidenceRecord Evidence;
    Evidence.Event.EventId = FGuid::NewGuid();
    Evidence.Event.Kind = Kind;
    Evidence.Event.SubjectId = SubjectId;
    Evidence.Event.OpportunityId = OpportunityId;
    Evidence.KnowledgeSource = EPedroKnowledgeSource::DirectObservation;
    return Evidence;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroEvidenceAcceptanceTest,
    "AI_Gavin_Project.Pedro.Evidence.Acceptance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroEvidenceAcceptanceTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;

    FPedroEvidenceRecord Invalid = MakeEvidence(
        EPedroEvidenceKind::HelpedNPC, FName(TEXT("NPC_A")));
    Invalid.Event.EventId = FGuid{};
    TestFalse(TEXT("Missing event ID is rejected"), State.RecordEvidence(Invalid));

    Invalid = MakeEvidence(EPedroEvidenceKind::None, FName(TEXT("NPC_A")));
    TestFalse(TEXT("None evidence kind is rejected"), State.RecordEvidence(Invalid));

    Invalid = MakeEvidence(EPedroEvidenceKind::HelpedNPC, NAME_None);
    TestFalse(TEXT("Events need a stable subject or opportunity ID"),
        State.RecordEvidence(Invalid));

    Invalid = MakeEvidence(EPedroEvidenceKind::HelpedNPC, FName(TEXT("NPC_A")));
    Invalid.KnowledgeSource = EPedroKnowledgeSource::Reported;
    TestFalse(TEXT("Unimplemented reported knowledge is rejected"),
        State.RecordEvidence(Invalid));

    FPedroEvidenceRecord Help = MakeEvidence(
        EPedroEvidenceKind::HelpedNPC, FName(TEXT("NPC_A")), FName(TEXT("Request_A")));
    TestTrue(TEXT("Directly observed fact is accepted"), State.RecordEvidence(Help));
    TestFalse(TEXT("Same event ID is rejected when repeated"), State.RecordEvidence(Help));
    TestEqual(TEXT("Only one evidence record exists"), State.GetEvidenceCount(), 1);
    TestEqual(TEXT("Duplicate event did not double-score Care"), State.TraitScores.Care, 1);
    TestEqual(TEXT("Duplicate event did not double-increment help count"),
        State.HelpedNPCCount, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroEvidenceMeaningTest,
    "AI_Gavin_Project.Pedro.Evidence.Meaning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroEvidenceMeaningTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    TestTrue(TEXT("Help recorded"), State.RecordEvidence(MakeEvidence(
        EPedroEvidenceKind::HelpedNPC, FName(TEXT("NPC_A")), FName(TEXT("Request_A")))));
    TestTrue(TEXT("Owned property destroyed"), State.RecordEvidence(MakeEvidence(
        EPedroEvidenceKind::DestroyedOwnedProperty, FName(TEXT("OwnedVase")))));
    TestTrue(TEXT("Unowned property destroyed"), State.RecordEvidence(MakeEvidence(
        EPedroEvidenceKind::DestroyedUnownedProperty, FName(TEXT("UnownedVase")))));
    TestTrue(TEXT("Cruel item choice recorded"), State.RecordEvidence(MakeEvidence(
        EPedroEvidenceKind::KeptRequestedItemCruelly, FName(TEXT("RequestedItem")))));
    TestTrue(TEXT("Observer hit recorded"), State.RecordEvidence(MakeEvidence(
        EPedroEvidenceKind::AttackedObserver, FName(TEXT("Pedro")))));
    TestTrue(TEXT("Secret discovered"), State.RecordEvidence(MakeEvidence(
        EPedroEvidenceKind::DiscoveredSecret, FName(TEXT("SecretOne")))));

    TestEqual(TEXT("One helpful action"), State.HelpedNPCCount, 1);
    TestEqual(TEXT("One owned property destroyed"), State.OwnedPropertyDestroyedCount, 1);
    TestEqual(TEXT("One unowned property destroyed"), State.UnownedPropertyDestroyedCount, 1);
    TestEqual(TEXT("One cruel action"), State.CruelActionCount, 1);
    TestEqual(TEXT("One observer hit"), State.ObserverAttackCount, 1);
    TestEqual(TEXT("One secret"), State.SecretsDiscoveredCount, 1);
    TestEqual(TEXT("Both objects and cruelty contribute to disruption"),
        State.TraitScores.Disruption, 3);
    TestEqual(TEXT("Cruelty offsets helping in Care dimension"),
        State.TraitScores.Care, 0);
    TestEqual(TEXT("Actual observer hit contributes to Aggression"),
        State.TraitScores.Aggression, 1);
    TestEqual(TEXT("Secret contributes to Curiosity"), State.TraitScores.Curiosity, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroEvidenceRepetitionTest,
    "AI_Gavin_Project.Pedro.Evidence.Repetition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroEvidenceRepetitionTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;

    for (int32 Index = 0; Index < 20; ++Index)
    {
        TestTrue(TEXT("Unique repeated interaction is recorded"),
            State.RecordEvidence(MakeEvidence(
                EPedroEvidenceKind::RepeatedInteraction, FName(TEXT("Lamp")))));
    }

    TestEqual(TEXT("All repetitions are kept in history"), State.GetEvidenceCount(), 20);
    TestEqual(TEXT("Lamp repetition influence is capped at three"),
        State.TraitScores.Repetition, 3);

    State.RecordEvidence(MakeEvidence(
        EPedroEvidenceKind::RepeatedInteraction, FName(TEXT("OtherLamp"))));
    TestEqual(TEXT("A different subject can contribute"),
        State.TraitScores.Repetition, 4);

    State.RecordEvidence(MakeEvidence(
        EPedroEvidenceKind::HelpedNPC, FName(TEXT("NPC_A")), FName(TEXT("Request_A"))));
    State.RecordEvidence(MakeEvidence(
        EPedroEvidenceKind::HelpedNPC, FName(TEXT("NPC_A")), FName(TEXT("Request_B"))));
    TestEqual(TEXT("Two different requests from one NPC contribute separately"),
        State.TraitScores.Care, 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroEvidenceResetTest,
    "AI_Gavin_Project.Pedro.Evidence.Reset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroEvidenceResetTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPedroObservationState State;
    State.InteractionCount = 3;
    State.UniqueInteractionCount = 2;
    State.RecordEvidence(MakeEvidence(
        EPedroEvidenceKind::AttackedNPC, FName(TEXT("NPC_A"))));
    State.RecordEvidence(MakeEvidence(
        EPedroEvidenceKind::RepeatedInteraction, FName(TEXT("Lamp"))));

    State.Reset();
    TestEqual(TEXT("Evidence cleared"), State.GetEvidenceCount(), 0);
    TestEqual(TEXT("Aggression reset"), State.TraitScores.Aggression, 0);
    TestEqual(TEXT("Repetition reset"), State.TraitScores.Repetition, 0);
    TestEqual(TEXT("NPC attack count reset"), State.NPCAttackCount, 0);
    TestEqual(TEXT("Legacy interaction count reset"), State.InteractionCount, 0);
    TestEqual(TEXT("Legacy unique count reset"), State.UniqueInteractionCount, 0);

    TestTrue(TEXT("Evidence can be recorded again after reset"),
        State.RecordEvidence(MakeEvidence(
            EPedroEvidenceKind::RepeatedInteraction, FName(TEXT("Lamp")))));
    TestEqual(TEXT("Repetition influence restarts at one"),
        State.TraitScores.Repetition, 1);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
