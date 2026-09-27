#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoomActivationVolume.generated.h"

class ARoomLifecycleActor;
class UBoxComponent;
class UPrimitiveComponent;
struct FHitResult;

/**
 * Spatial entry point for a gameplay room.
 *
 * When a player-controlled Pawn enters the activation box, this actor
 * requests activation from its explicitly configured RoomLifecycleActor.
 *
 * The lifecycle remains responsible for validating activation and for
 * making repeated activation idempotent.
 */
UCLASS()
class AI_GAVIN_PROJECT_API ARoomActivationVolume : public AActor
{
    GENERATED_BODY()

public:
    ARoomActivationVolume();

protected:
    virtual void BeginPlay() override;

    /**
     * Editable trigger shape defining the room's activation boundary.
     */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Components")
    TObjectPtr<UBoxComponent> ActivationBox;

    /**
     * Room activated when a player enters this volume.
     */
    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "Room|Activation")
    TObjectPtr<ARoomLifecycleActor> RoomLifecycle;

private:
    UFUNCTION()
    void HandleActivationOverlap(
        UPrimitiveComponent *OverlappedComponent,
        AActor *OtherActor,
        UPrimitiveComponent *OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult &SweepResult);
};
