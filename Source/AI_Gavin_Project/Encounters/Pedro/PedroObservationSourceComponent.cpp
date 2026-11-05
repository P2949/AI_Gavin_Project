#include "Encounters/Pedro/PedroObservationSourceComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

UPedroObservationSourceComponent::UPedroObservationSourceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UPedroObservationSourceComponent::ReportConfirmedAction(
    EPedroEvidenceKind Kind)
{
    if (!bEvidenceEnabled || Kind == EPedroEvidenceKind::None ||
        (SubjectId.IsNone() && OpportunityId.IsNone()))
    {
        return false;
    }

    AActor* SourceActor = GetOwner();
    if (!IsValid(SourceActor))
    {
        return false;
    }

    FPedroBehaviourEvent Event;
    Event.EventId = FGuid::NewGuid();
    Event.Kind = Kind;
    Event.SubjectActor = SourceActor;
    Event.SubjectId = SubjectId;
    Event.OpportunityId = OpportunityId;
    Event.EventLocation = SourceActor->GetActorLocation();

    if (const UWorld* World = SourceActor->GetWorld())
    {
        Event.WorldTimeSeconds = World->GetTimeSeconds();
    }

    // Reports world truth only; the Director decides what Pedro knows.
    OnWorldEventProduced.Broadcast(Event);
    return true;
}

bool UPedroObservationSourceComponent::ReportOpportunityExposed()
{
    if (!bEvidenceEnabled || OpportunityId.IsNone() || !IsValid(GetOwner()))
    {
        return false;
    }

    // Player exposure is a world fact, not an observer perception event.
    OnOpportunityExposed.Broadcast(OpportunityId);
    return true;
}
