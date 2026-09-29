#include "Player/Interaction/PlayerInteractionComponent.h"

#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Interaction/Interactable.h"

UPlayerInteractionComponent::UPlayerInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerInteractionComponent::SetInteractionOriginComponent(
    USceneComponent *NewOriginComponent)
{
    InteractionOriginComponent = NewOriginComponent;
}

bool UPlayerInteractionComponent::TryInteract()
{
    AActor *InteractionTarget =
        FindInteractionTarget();

    if (!InteractionTarget)
    {
        return false;
    }

    IInteractable::Execute_Interact(
        InteractionTarget,
        GetOwner());

    return true;
}

AActor *UPlayerInteractionComponent::FindInteractionTarget() const
{
    if (!IsValid(InteractionOriginComponent) ||
        InteractionDistance <= 0.0f)
    {
        return nullptr;
    }

    UWorld *World = GetWorld();

    if (!World)
    {
        return nullptr;
    }

    const FVector TraceStart =
        InteractionOriginComponent->GetComponentLocation();

    const FVector TraceEnd =
        TraceStart +
        InteractionOriginComponent->GetForwardVector() *
            InteractionDistance;

    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = false;

    if (const AActor *Owner = GetOwner())
    {
        QueryParams.AddIgnoredActor(Owner);
    }

    FHitResult HitResult;

    const bool bHit =
        World->LineTraceSingleByChannel(
            HitResult,
            TraceStart,
            TraceEnd,
            InteractionTraceChannel.GetValue(),
            QueryParams);

    if (!bHit)
    {
        return nullptr;
    }

    AActor *HitActor =
        HitResult.GetActor();

    if (!IsValid(HitActor) ||
        !HitActor->GetClass()->ImplementsInterface(
            UInteractable::StaticClass()))
    {
        return nullptr;
    }

    return HitActor;
}
