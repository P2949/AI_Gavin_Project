#include "Encounters/Pedro/PedroObservationState.h"
#include "Encounters/Pedro/PedroClassification.h"
#include "Encounters/Pedro/PedroCompletionGate.h"
#include "Encounters/Pedro/PedroObserverCharacter.h"
#include "Encounters/Pedro/PedroPlayableIntegrationActor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace PedroPlayableUnit
{
FPedroEvidenceRecord Contact()
{
    FPedroEvidenceRecord R;
    R.Event.EventId = FGuid::NewGuid();
    R.Event.Kind = EPedroEvidenceKind::AttackedObserver;
    R.Event.SubjectActor = NewObject<APedroObserverCharacter>(GetTransientPackage());
    R.Event.SubjectId = FName(TEXT("PedroObserver"));
    R.Event.OpportunityId = NAME_None;
    R.KnowledgeSource = EPedroKnowledgeSource::DirectContact;
    return R;
}
}

#define PEDRO_PLAYABLE_UNIT(Name) \
    IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPedroPlayable##Name##Test, \
        "AI_Gavin_Project.Pedro.Playable." #Name, \
        EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
    bool FPedroPlayable##Name##Test::RunTest(const FString& Parameters)

PEDRO_PLAYABLE_UNIT(ContactAccepted)
{
    FPedroObservationState S;
    const auto R = PedroPlayableUnit::Contact();
    return TestTrue(TEXT("Verified firsthand observer hit accepted"), S.RecordEvidence(R)) &&
        TestEqual(TEXT("One observer attack"), S.ObserverAttackCount, 1) &&
        TestEqual(TEXT("One acquired record"), S.GetEvidenceCount(), 1);
}

PEDRO_PLAYABLE_UNIT(ContactWrongKind)
{
    FPedroObservationState S;
    auto R = PedroPlayableUnit::Contact();
    R.Event.Kind = EPedroEvidenceKind::HelpedNPC;
    return TestFalse(TEXT("Cannot use physical-contact provenance for another kind"),
        S.RecordEvidence(R));
}

PEDRO_PLAYABLE_UNIT(ContactMissingObserver)
{
    FPedroObservationState S;
    auto R = PedroPlayableUnit::Contact();
    R.Event.SubjectActor.Reset();
    const bool Missing = !S.RecordEvidence(R);
    R = PedroPlayableUnit::Contact();
    R.Event.SubjectId = FName(TEXT("OtherActor"));
    const bool Wrong = !S.RecordEvidence(R);
    return TestTrue(TEXT("Contact must identify a live Pedro observer"), Missing && Wrong);
}

PEDRO_PLAYABLE_UNIT(ContactDuplicate)
{
    FPedroObservationState S;
    const auto R = PedroPlayableUnit::Contact();
    const bool First = S.RecordEvidence(R);
    const bool Again = S.RecordEvidence(R);
    return TestTrue(TEXT("Same physical occurrence not counted twice"), First && !Again) &&
        TestEqual(TEXT("Only one observer attack"), S.ObserverAttackCount, 1);
}

PEDRO_PLAYABLE_UNIT(ContactNoCoreSample)
{
    FPedroObservationState S;
    TArray<FPedroOpportunityDefinition> Defs;
    for (const TCHAR* Id : {TEXT("A"),TEXT("B"),TEXT("C"),TEXT("D")})
    {
        FPedroOpportunityDefinition D;
        D.OpportunityId = FName(Id);
        Defs.Add(D);
    }
    const bool Configured = S.Opportunities.Configure(Defs);
    const bool Accepted = S.RecordEvidence(PedroPlayableUnit::Contact());
    const auto Decision = PedroCompletionGate::Evaluate(S, 0.75f);
    return TestTrue(TEXT("Configured and accepted"), Configured && Accepted) &&
        TestEqual(TEXT("Observer contact does not fake a core sample"),
            S.Opportunities.GetSampledCoreCount(), 0) &&
        TestFalse(TEXT("Insufficient core sampling does not complete"), Decision.CanConclude());
}

PEDRO_PLAYABLE_UNIT(ContactClassificationDisqualification)
{
    FPedroObservationState S;
    const bool Accepted = S.RecordEvidence(PedroPlayableUnit::Contact());
    const auto C = PedroClassification::Evaluate(S);
    return TestTrue(TEXT("Contact accepted"), Accepted) &&
        TestFalse(TEXT("Attacking Pedro disqualifies Gentle"), C.bGentleEligible) &&
        TestEqual(TEXT("First aggression contribution"), S.TraitScores.Aggression, 1);
}

PEDRO_PLAYABLE_UNIT(ContactLegacyProvenance)
{
    FPedroObservationState S;
    auto R = PedroPlayableUnit::Contact();
    R.KnowledgeSource = EPedroKnowledgeSource::Reported;
    const bool Reported = !S.RecordEvidence(R);
    R.KnowledgeSource = EPedroKnowledgeSource::Inferred;
    const bool Inferred = !S.RecordEvidence(R);
    R.KnowledgeSource = EPedroKnowledgeSource::DirectObservation;
    R.Event.Kind = EPedroEvidenceKind::InspectedOptionalObject;
    R.Event.SubjectId = FName(TEXT("Object"));
    const bool ExistingDirect = S.RecordEvidence(R);
    return TestTrue(TEXT("Unsupported sources still fail; visual behaviour unchanged"),
        Reported && Inferred && ExistingDirect);
}
#undef PEDRO_PLAYABLE_UNIT
#endif
