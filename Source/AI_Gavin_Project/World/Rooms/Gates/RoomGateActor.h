#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/Rooms/RoomLifecycleTypes.h"
#include "RoomGateActor.generated.h"

class ARoomLifecycleActor;
class UBoxComponent;
class UStaticMeshComponent;

/**
 * Physical access gate driven entirely by room lifecycle state.
 *
 * The gate derives whether it should be open from the current room state
 * instead of maintaining historical open/closed gameplay state.
 *
 * A separate collision box keeps gameplay access independent from the
 * optional presentation mesh.
 */
UCLASS()
class AI_GAVIN_PROJECT_API ARoomGateActor : public AActor
{
    GENERATED_BODY()

public:
    ARoomGateActor();

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason) override;

    /**
     * Authoritative physical blocker for the gate.
     */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Components")
    TObjectPtr<UBoxComponent> GateCollision;

    /**
     * Optional presentation mesh.
     *
     * Collision is intentionally disabled on this component. GateCollision
     * remains the authoritative physical boundary.
     */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Components")
    TObjectPtr<UStaticMeshComponent> GateVisual;

    /**
     * Room whose lifecycle drives this gate.
     */
    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "Room|Gate")
    TObjectPtr<ARoomLifecycleActor> RoomLifecycle;

    /**
     * Controls the gate's Inactive-state policy.
     *
     * true  = entrance-style gate: open before room activation.
     * false = exit-style gate: closed until room completion.
     *
     * All gates close while the room is Active and open once Completed.
     */
    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "Room|Gate")
    bool bOpenWhileInactive = true;

private:
    UFUNCTION()
    void HandleRoomStateChanged(
        ERoomLifecycleState OldState,
        ERoomLifecycleState NewState);

    void ApplyRoomState(ERoomLifecycleState State);
    bool ShouldBeOpen(ERoomLifecycleState State) const;
};
