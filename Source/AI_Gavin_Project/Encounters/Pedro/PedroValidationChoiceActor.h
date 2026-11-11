#pragma once

#include "CoreMinimal.h"
#include "Encounters/Pedro/PedroObservationTypes.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"

#include "PedroValidationChoiceActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UPedroObservationSourceComponent;

/**
 * Isolated PIE validation control, not an NPC, combat action or production
 * gameplay actor. Clicking a clearly labelled cube emits a SIMULATED confirmed
 * HelpedNPC or AttackedNPC event for a specified existing core opportunity.
 * No perception / LOS check is performed. Disabled outside ChoiceTest maps.
 */
UCLASS(NotBlueprintable)
class AI_GAVIN_PROJECT_API APedroValidationChoiceActor : public AActor,
    public IInteractable
{
    GENERATED_BODY()

public:
    APedroValidationChoiceActor();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Pedro|Validation")
    FName OpportunityId = NAME_None;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Pedro|Validation")
    EPedroEvidenceKind ActionKind = EPedroEvidenceKind::HelpedNPC;

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Interact_Implementation(AActor* Interactor) override;

protected:
    virtual void BeginPlay() override;

private:
    void ConfigureSource();
    bool IsPermittedWorld() const;

    UPROPERTY(VisibleAnywhere, Category="Pedro|Validation")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category="Pedro|Validation")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, Category="Pedro|Validation")
    TObjectPtr<UTextRenderComponent> Label;

    UPROPERTY(VisibleAnywhere, Category="Pedro|Validation")
    TObjectPtr<UPedroObservationSourceComponent> ObservationSource;

    bool bAlreadyUsed = false;
};
