#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Interaction/Interactable.h"

#include "PedroPrototypeInteractableActor.generated.h"

class UStaticMeshComponent;

/**
 * Minimal Pedro-local interaction target used to exercise observation logic.
 *
 * The actor deliberately performs no gameplay action when interacted with.
 * Pedro observes the player's decision through the shared interaction
 * dispatch event.
 */
UCLASS()
class AI_GAVIN_PROJECT_API APedroPrototypeInteractableActor
    : public AActor,
      public IInteractable
{
    GENERATED_BODY()

public:
    APedroPrototypeInteractableActor();

    virtual void Interact_Implementation(
        AActor *Interactor) override;

protected:
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Pedro|Prototype")
    TObjectPtr<UStaticMeshComponent> MeshComponent;
};
