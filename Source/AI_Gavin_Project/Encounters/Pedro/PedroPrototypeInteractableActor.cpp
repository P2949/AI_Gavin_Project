#include "Encounters/Pedro/PedroPrototypeInteractableActor.h"

#include "Components/StaticMeshComponent.h"
#include "Encounters/Pedro/PedroObservationSourceComponent.h"

APedroPrototypeInteractableActor::APedroPrototypeInteractableActor()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("MeshComponent"));
    SetRootComponent(MeshComponent);

    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    ObservationSource = CreateDefaultSubobject<UPedroObservationSourceComponent>(
        TEXT("ObservationSource"));
}

void APedroPrototypeInteractableActor::Interact_Implementation(
    AActor* Interactor)
{
    if (!IsValid(Interactor))
    {
        return;
    }

    // This state transition confirms that the prototype inspection happened.
    // OnInteractionDispatched runs earlier and does not prove success.
    ++SuccessfulInspectionCount;

    if (IsValid(ObservationSource))
    {
        const EPedroEvidenceKind Kind =
            SuccessfulInspectionCount == 1
                ? EPedroEvidenceKind::InspectedOptionalObject
                : EPedroEvidenceKind::RepeatedInteraction;

        ObservationSource->ReportConfirmedAction(Kind);
    }
}
