#include "Encounters/Pedro/PedroDirectObservation.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

const TCHAR* PedroDirectObservation::StatusName(
    EPedroDirectObservationStatus Status)
{
    switch (Status)
    {
    case EPedroDirectObservationStatus::Visible: return TEXT("Visible");
    case EPedroDirectObservationStatus::MissingWorld: return TEXT("MissingWorld");
    case EPedroDirectObservationStatus::MissingObserver: return TEXT("MissingObserver");
    case EPedroDirectObservationStatus::ObserverWorldMismatch: return TEXT("ObserverWorldMismatch");
    case EPedroDirectObservationStatus::InvalidEvent: return TEXT("InvalidEvent");
    case EPedroDirectObservationStatus::InvalidSettings: return TEXT("InvalidSettings");
    case EPedroDirectObservationStatus::InvalidGeometry: return TEXT("InvalidGeometry");
    case EPedroDirectObservationStatus::TooFar: return TEXT("TooFar");
    case EPedroDirectObservationStatus::OutsideViewCone: return TEXT("OutsideViewCone");
    case EPedroDirectObservationStatus::Occluded: return TEXT("Occluded");
    }
    return TEXT("Unknown");
}

EPedroDirectObservationStatus PedroDirectObservation::CheckGeometry(
    const FVector& EyeLocation,
    const FVector& EyeForward,
    const FVector& EventLocation,
    float MaxDistance,
    float ViewHalfAngleDegrees)
{
    if (!FMath::IsFinite(MaxDistance) || MaxDistance <= 0.0f ||
        !FMath::IsFinite(ViewHalfAngleDegrees) ||
        ViewHalfAngleDegrees < 0.0f || ViewHalfAngleDegrees > 180.0f)
    {
        return EPedroDirectObservationStatus::InvalidSettings;
    }
    if (EyeLocation.ContainsNaN() || EyeForward.ContainsNaN() ||
        EventLocation.ContainsNaN() || EyeForward.IsNearlyZero())
    {
        return EPedroDirectObservationStatus::InvalidGeometry;
    }

    const FVector ToEvent = EventLocation - EyeLocation;
    const double DistanceSquared = ToEvent.SizeSquared();
    const double MaximumSquared = static_cast<double>(MaxDistance) * MaxDistance;
    if (!FMath::IsFinite(DistanceSquared))
    {
        return EPedroDirectObservationStatus::InvalidGeometry;
    }
    if (DistanceSquared > MaximumSquared)
    {
        return EPedroDirectObservationStatus::TooFar;
    }

    // An event at the eye position requires no angle or trace segment.
    if (DistanceSquared > 1e-12)
    {
        const FVector Forward = EyeForward.GetSafeNormal();
        const FVector Direction = ToEvent.GetSafeNormal();
        if (Forward.IsNearlyZero() || Direction.IsNearlyZero())
        {
            return EPedroDirectObservationStatus::InvalidGeometry;
        }
        const double Dot = FVector::DotProduct(Forward, Direction);
        const double MinDot = FMath::Cos(FMath::DegreesToRadians(
            static_cast<double>(ViewHalfAngleDegrees)));
        if (Dot < MinDot - 1e-6)
        {
            return EPedroDirectObservationStatus::OutsideViewCone;
        }
    }
    return EPedroDirectObservationStatus::Visible;
}

EPedroDirectObservationStatus PedroDirectObservation::Evaluate(
    const UWorld* World,
    const AActor* Observer,
    const FPedroBehaviourEvent& Event,
    float MaxDistance,
    float ViewHalfAngleDegrees)
{
    if (!IsValid(World))
    {
        return EPedroDirectObservationStatus::MissingWorld;
    }
    if (!IsValid(Observer))
    {
        return EPedroDirectObservationStatus::MissingObserver;
    }
    if (Observer->GetWorld() != World)
    {
        return EPedroDirectObservationStatus::ObserverWorldMismatch;
    }
    if (!Event.EventId.IsValid() || Event.Kind == EPedroEvidenceKind::None ||
        (Event.SubjectId.IsNone() && Event.OpportunityId.IsNone()))
    {
        return EPedroDirectObservationStatus::InvalidEvent;
    }

    FVector EyeLocation;
    FRotator EyeRotation;
    Observer->GetActorEyesViewPoint(EyeLocation, EyeRotation);

    const EPedroDirectObservationStatus Geometry = CheckGeometry(
        EyeLocation,
        EyeRotation.Vector(),
        Event.EventLocation,
        MaxDistance,
        ViewHalfAngleDegrees);
    if (Geometry != EPedroDirectObservationStatus::Visible)
    {
        return Geometry;
    }

    // There is no occluding segment if the event is at the observer's eye.
    if (EyeLocation.Equals(Event.EventLocation, 0.01f))
    {
        return EPedroDirectObservationStatus::Visible;
    }

    FHitResult Hit;
    // Use simple Visibility collision (the same kind configured on fixture cubes).
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PedroDirectObservation), false);
    QueryParams.AddIgnoredActor(Observer);

    const bool bHit = World->LineTraceSingleByChannel(
        Hit, EyeLocation, Event.EventLocation, ECC_Visibility, QueryParams);

    // A target's own visible collision surface is not an obstruction.
    const AActor* SubjectActor = Event.SubjectActor.Get();
    if (bHit && (!IsValid(SubjectActor) || Hit.GetActor() != SubjectActor))
    {
        return EPedroDirectObservationStatus::Occluded;
    }
    return EPedroDirectObservationStatus::Visible;
}
