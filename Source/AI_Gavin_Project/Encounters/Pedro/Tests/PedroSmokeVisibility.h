#pragma once

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "Encounters/Pedro/PedroDirectObservation.h"
#include "Encounters/Pedro/PedroEncounterDirector.h"
#include "Encounters/Pedro/PedroObserverCharacter.h"

namespace PedroSmokeVisibility
{
// Read-only scenario preflight. The Director still acquires the real event.
inline bool CanSee(UWorld* World, APedroObserverCharacter* Observer,
    const APedroEncounterDirector* Director, AActor* Target)
{
    if (!IsValid(World) || !IsValid(Observer) || !IsValid(Director) || !IsValid(Target))
        return false;
    FVector Eye;
    FRotator Facing;
    Observer->GetActorEyesViewPoint(Eye, Facing);
    if (PedroDirectObservation::CheckGeometry(Eye, Facing.Vector(),
        Target->GetActorLocation(), Director->DirectObservationMaxDistance,
        Director->DirectObservationViewHalfAngleDegrees) != EPedroDirectObservationStatus::Visible)
        return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PedroSmokeObserverLOS), false);
    Params.AddIgnoredActor(Observer);
    FHitResult Hit;
    return !World->LineTraceSingleByChannel(Hit, Eye, Target->GetActorLocation(),
        ECC_Visibility, Params) || Hit.GetActor() == Target;
}
}
#endif
