#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Interaction/Interactable.h"

#include "PedroPrototypeInteractableActor.generated.h"

class UStaticMeshComponent;
class UPedroObservationSourceComponent;

/**
 * Pedro-local prototype inspectable object.
 * A confirmed inspection increments its local counter; if explicitly
 * configured, the observation source emits a factual world event.
 * No observer is presumed to have witnessed the interaction.
 */
UCLASS()
class AI_GAVIN_PROJECT_API APedroPrototypeInteractableActor
    : public AActor,
      public IInteractable
{
    GENERATED_BODY()

public:
    APedroPrototypeInteractableActor();

    virtual void Interact_Implementation(AActor* Interactor) override;

    int32 GetSuccessfulInspectionCount() const
    {
        return SuccessfulInspectionCount;
    }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Prototype")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Observation")
    TObjectPtr<UPedroObservationSourceComponent> ObservationSource;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Pedro|Prototype")
    int32 SuccessfulInspectionCount = 0;
};
