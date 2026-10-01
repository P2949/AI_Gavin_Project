// EncounterEntranceActor.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Interaction/Interactable.h"

#include "EncounterEntranceActor.generated.h"

class UEncounterDefinition;
class FDataValidationContext;
class UBoxComponent;

UCLASS()
class AI_GAVIN_PROJECT_API AEncounterEntranceActor
    : public AActor,
      public IInteractable
{
    GENERATED_BODY()

public:
    AEncounterEntranceActor();

    virtual void Interact_Implementation(
        AActor *Interactor) override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(
        FDataValidationContext &Context) const override;
#endif

protected:
    virtual void BeginPlay() override;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Encounter")
    TObjectPtr<UBoxComponent> InteractionBounds;

    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "Encounter")
    TObjectPtr<UEncounterDefinition> EncounterDefinition;

    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "Encounter")
    TObjectPtr<AActor> ResolvedIndicatorActor;

private:
    void RefreshResolvedPresentation();
};
