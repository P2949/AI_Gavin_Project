#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "PedroPrototypeReturnVolume.generated.h"

class FDataValidationContext;
class UBoxComponent;
class UPrimitiveComponent;
class UWorld;
struct FHitResult;

/**
 * Pedro-local prototype exit used by the world-composition spike.
 *
 * A player-controlled Pawn entering the volume travels to the configured
 * return map. This intentionally does not use IInteractable so leaving the
 * prototype does not become Pedro observation evidence.
 */
UCLASS()
class AI_GAVIN_PROJECT_API APedroPrototypeReturnVolume
    : public AActor
{
    GENERATED_BODY()

public:
    APedroPrototypeReturnVolume();

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(
        FDataValidationContext &Context) const override;
#endif

protected:
    virtual void BeginPlay() override;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Pedro|Prototype")
    TObjectPtr<UBoxComponent> ReturnVolume;

    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "Pedro|Prototype")
    TSoftObjectPtr<UWorld> ReturnMap;

private:
    UFUNCTION()
    void HandleReturnOverlap(
        UPrimitiveComponent *OverlappedComponent,
        AActor *OtherActor,
        UPrimitiveComponent *OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult &SweepResult);
};
