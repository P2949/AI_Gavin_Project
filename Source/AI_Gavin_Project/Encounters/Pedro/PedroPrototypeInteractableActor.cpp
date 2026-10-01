#include "Encounters/Pedro/PedroPrototypeInteractableActor.h"

#include "Components/StaticMeshComponent.h"

APedroPrototypeInteractableActor::
    APedroPrototypeInteractableActor()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComponent =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("MeshComponent"));

    SetRootComponent(MeshComponent);

    MeshComponent->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly);

    MeshComponent->SetCollisionResponseToAllChannels(
        ECR_Ignore);

    MeshComponent->SetCollisionResponseToChannel(
        ECC_Visibility,
        ECR_Block);
}

void APedroPrototypeInteractableActor::
    Interact_Implementation(
        AActor *Interactor)
{
    static_cast<void>(Interactor);
}
