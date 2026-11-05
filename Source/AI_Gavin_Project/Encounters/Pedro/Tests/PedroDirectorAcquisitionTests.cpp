#include "Encounters/Pedro/PedroEncounterDirector.h"
#include "Encounters/Pedro/PedroObservationSourceComponent.h"
#include "Encounters/Pedro/PedroPrototypeInteractableActor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
UPedroObservationSourceComponent* ConfigureSource(
    APedroPrototypeInteractableActor* Prop, FName SubjectId)
{
    UPedroObservationSourceComponent* Source =
        Prop->FindComponentByClass<UPedroObservationSourceComponent>();

    if (Source)
    {
        Source->bEvidenceEnabled = true;
        Source->SubjectId = SubjectId;
        Source->OpportunityId = FName(TEXT("Inspect_A"));
    }
    return Source;
}

FPedroBehaviourEvent MakeWorldEvent(FName SubjectId)
{
    FPedroBehaviourEvent Event;
    Event.EventId = FGuid::NewGuid();
    Event.Kind = EPedroEvidenceKind::DiscoveredSecret;
    Event.SubjectId = SubjectId;
    Event.OpportunityId = FName(TEXT("Secret_A"));
    Event.WorldTimeSeconds = 5.0f;
    return Event;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroDirectorConfirmedEventTest,
    "AI_Gavin_Project.Pedro.Director.ConfirmedEvent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroDirectorConfirmedEventTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);

    APedroEncounterDirector* Director =
        NewObject<APedroEncounterDirector>(GetTransientPackage());
    APedroPrototypeInteractableActor* Prop =
        NewObject<APedroPrototypeInteractableActor>(GetTransientPackage());
    AActor* Interactor = NewObject<AActor>(GetTransientPackage());
    UPedroObservationSourceComponent* Source =
        ConfigureSource(Prop, FName(TEXT("PrototypeProp_A")));

    TestNotNull(TEXT("Prototype has an observation source"), Source);
    if (!Source)
    {
        return false;
    }

    Director->bUsePrototypeAssumedAcquisition = true;

    TestTrue(TEXT("First source registration works"),
        Director->RegisterObservationSource(Source));
    TestFalse(TEXT("Duplicate registration is ignored"),
        Director->RegisterObservationSource(Source));

    Prop->Interact_Implementation(Interactor);
    Prop->Interact_Implementation(Interactor);

    const FPedroObservationState& State = Director->GetObservationState();
    TestEqual(TEXT("Two completed interactions are acquired"),
        State.GetEvidenceCount(), 2);
    TestEqual(TEXT("Inspection raised Curiosity"), State.TraitScores.Curiosity, 1);
    TestEqual(TEXT("Repeated interaction raised Repetition"),
        State.TraitScores.Repetition, 1);

    if (State.EvidenceHistory.Num() == 2)
    {
        TestTrue(TEXT("First record explicitly says awareness was assumed"),
            State.EvidenceHistory[0].KnowledgeSource ==
                EPedroKnowledgeSource::PrototypeAssumed);
        TestTrue(TEXT("Second record explicitly says awareness was assumed"),
            State.EvidenceHistory[1].KnowledgeSource ==
                EPedroKnowledgeSource::PrototypeAssumed);
        TestTrue(TEXT("Event identities remain distinct"),
            State.EvidenceHistory[0].Event.EventId !=
                State.EvidenceHistory[1].Event.EventId);
        TestEqual(TEXT("Subject identity retained in acquired record"),
            State.EvidenceHistory[0].Event.SubjectId,
            FName(TEXT("PrototypeProp_A")));
    }

    // For this old prototype, an unbound actor still performs inspections,
    // but without a Director registration no knowledge is acquired.
    APedroPrototypeInteractableActor* Unbound =
        NewObject<APedroPrototypeInteractableActor>(GetTransientPackage());
    UPedroObservationSourceComponent* OtherSource =
        ConfigureSource(Unbound, FName(TEXT("UnboundProp")));
    TestNotNull(TEXT("Unbound actor has a source"), OtherSource);
    Unbound->Interact_Implementation(Interactor);
    TestEqual(TEXT("Unbound actor did not change Director knowledge"),
        State.GetEvidenceCount(), 2);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroDirectorAcquisitionLifecycleTest,
    "AI_Gavin_Project.Pedro.Director.AcquisitionLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroDirectorAcquisitionLifecycleTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);

    APedroEncounterDirector* Director =
        NewObject<APedroEncounterDirector>(GetTransientPackage());
    APedroPrototypeInteractableActor* Prop =
        NewObject<APedroPrototypeInteractableActor>(GetTransientPackage());
    UPedroObservationSourceComponent* Source =
        ConfigureSource(Prop, FName(TEXT("Source_A")));

    TestNotNull(TEXT("Prototype has an observation source"), Source);
    if (!Source)
    {
        return false;
    }

    TestFalse(TEXT("Assumed acquisition is disabled by default"),
        Director->bUsePrototypeAssumedAcquisition);
    TestTrue(TEXT("Registration works with acquisition disabled"),
        Director->RegisterObservationSource(Source));

    FPedroBehaviourEvent Event = MakeWorldEvent(FName(TEXT("SecretObject_A")));
    Source->OnWorldEventProduced.Broadcast(Event);
    TestEqual(TEXT("Disabled acquisition leaves knowledge empty"),
        Director->GetObservationState().GetEvidenceCount(), 0);

    Director->bUsePrototypeAssumedAcquisition = true;
    Source->OnWorldEventProduced.Broadcast(Event);
    TestEqual(TEXT("Enabled acquisition accepts the original event"),
        Director->GetObservationState().GetEvidenceCount(), 1);
    TestEqual(TEXT("Original event ID is preserved"),
        Director->GetObservationState().EvidenceHistory[0].Event.EventId,
        Event.EventId);

    Source->OnWorldEventProduced.Broadcast(Event);
    TestEqual(TEXT("Rebroadcast of the same occurrence is rejected"),
        Director->GetObservationState().GetEvidenceCount(), 1);
    TestEqual(TEXT("Duplicate never increments the secret counter"),
        Director->GetObservationState().SecretsDiscoveredCount, 1);

    // Simulate resolution without requiring the hub/progress subsystem in a
    // unit test. The same bHasPresentedOutcome guard is used in production.
    Director->bHasPresentedOutcome = true;
    Source->OnWorldEventProduced.Broadcast(
        MakeWorldEvent(FName(TEXT("SecretObject_B"))));
    TestEqual(TEXT("Events after resolution cannot change knowledge"),
        Director->GetObservationState().GetEvidenceCount(), 1);

    Director->bHasPresentedOutcome = false;
    Director->UnregisterObservationSources();
    Source->OnWorldEventProduced.Broadcast(
        MakeWorldEvent(FName(TEXT("SecretObject_C"))));
    TestEqual(TEXT("Unsubscription stops event acquisition"),
        Director->GetObservationState().GetEvidenceCount(), 1);

    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
