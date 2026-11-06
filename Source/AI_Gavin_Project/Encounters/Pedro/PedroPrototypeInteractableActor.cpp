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

    // The inspection is real; an attempted dispatch alone would not qualify.
    ++SuccessfulInspectionCount;

    if (IsValid(ObservationSource))
    {
        // A completed inspection also demonstrates meaningful exposure.
        // Future refusal/abandonment activities must explicitly expose the
        // choice before they can report a response (including ignoring it).
        ObservationSource->ReportOpportunityExposed();

        const EPedroEvidenceKind Kind =
            SuccessfulInspectionCount == 1
                ? EPedroEvidenceKind::InspectedOptionalObject
                : EPedroEvidenceKind::RepeatedInteraction;

        ObservationSource->ReportConfirmedAction(Kind);
    }
}
