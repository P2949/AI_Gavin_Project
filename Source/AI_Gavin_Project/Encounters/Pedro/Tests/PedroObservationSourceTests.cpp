#include "Encounters/Pedro/PedroObservationSourceComponent.h"
#include "Encounters/Pedro/PedroPrototypeInteractableActor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroSourceDisabledAndIdentityTest,
    "AI_Gavin_Project.Pedro.Source.Configuration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroSourceDisabledAndIdentityTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);

    APedroPrototypeInteractableActor* Prop =
        NewObject<APedroPrototypeInteractableActor>(GetTransientPackage());
    UPedroObservationSourceComponent* Source =
        Prop->FindComponentByClass<UPedroObservationSourceComponent>();
    TestNotNull(TEXT("Prototype has an observation source"), Source);
    if (!Source)
    {
        return false;
    }

    int32 Emissions = 0;
    Source->OnWorldEventProduced.AddLambda(
        [&Emissions](const FPedroBehaviourEvent&) { ++Emissions; });

    TestFalse(TEXT("Disabled source emits nothing"),
        Source->ReportConfirmedAction(EPedroEvidenceKind::HelpedNPC));

    Source->bEvidenceEnabled = true;
    TestFalse(TEXT("Missing authored identity emits nothing"),
        Source->ReportConfirmedAction(EPedroEvidenceKind::HelpedNPC));

    Source->SubjectId = FName(TEXT("PrototypeProp_A"));
    TestFalse(TEXT("None kind emits nothing"),
        Source->ReportConfirmedAction(EPedroEvidenceKind::None));
    TestEqual(TEXT("No invalid event was broadcast"), Emissions, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroSourceEmissionsTest,
    "AI_Gavin_Project.Pedro.Source.Emission",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPedroSourceEmissionsTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);

    APedroPrototypeInteractableActor* Prop =
        NewObject<APedroPrototypeInteractableActor>(GetTransientPackage());
    AActor* Interactor = NewObject<AActor>(GetTransientPackage());
    UPedroObservationSourceComponent* Source =
        Prop->FindComponentByClass<UPedroObservationSourceComponent>();
    TestNotNull(TEXT("Prototype has an observation source"), Source);
    if (!Source)
    {
        return false;
    }

    Source->bEvidenceEnabled = true;
    Source->SubjectId = FName(TEXT("OptionalProp_A"));
    Source->OpportunityId = FName(TEXT("InspectProp_A"));

    TArray<FPedroBehaviourEvent> ProducedEvents;
    Source->OnWorldEventProduced.AddLambda(
        [&ProducedEvents](const FPedroBehaviourEvent& Event)
        {
            ProducedEvents.Add(Event);
        });

    // Invalid interaction cannot create a confirmed inspection.
    Prop->Interact_Implementation(nullptr);
    TestEqual(TEXT("Invalid interactor did not inspect"),
        Prop->GetSuccessfulInspectionCount(), 0);

    Prop->Interact_Implementation(Interactor);
    Prop->Interact_Implementation(Interactor);
    TestEqual(TEXT("Two inspections completed"),
        Prop->GetSuccessfulInspectionCount(), 2);
    TestEqual(TEXT("Two distinct world events emitted"),
        ProducedEvents.Num(), 2);

    if (ProducedEvents.Num() == 2)
    {
        TestTrue(TEXT("First occurrence has unique ID"),
            ProducedEvents[0].EventId.IsValid());
        TestTrue(TEXT("Second occurrence has unique ID"),
            ProducedEvents[1].EventId.IsValid());
        TestTrue(TEXT("Repeated actions have different event IDs"),
            ProducedEvents[0].EventId != ProducedEvents[1].EventId);
        TestTrue(TEXT("First action is an actual inspection"),
            ProducedEvents[0].Kind ==
            EPedroEvidenceKind::InspectedOptionalObject);
        TestTrue(TEXT("Subsequent action is a repetition"),
            ProducedEvents[1].Kind ==
            EPedroEvidenceKind::RepeatedInteraction);
        TestEqual(TEXT("Subject ID is retained"),
            ProducedEvents[0].SubjectId,
            FName(TEXT("OptionalProp_A")));
        TestEqual(TEXT("Opportunity ID is retained"),
            ProducedEvents[1].OpportunityId,
            FName(TEXT("InspectProp_A")));
        TestTrue(TEXT("Source actor is non-owning"),
            ProducedEvents[0].SubjectActor.Get() == Prop);
    }

    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
