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

    // This event reports world truth only. Perception and evidence acquisition
    // belong to the Director/observer, in a separate implementation step.
    OnWorldEventProduced.Broadcast(Event);
    return true;
}
