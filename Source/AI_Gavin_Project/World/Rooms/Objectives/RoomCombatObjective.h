#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/Rooms/RoomLifecycleTypes.h"
#include "RoomCombatObjective.generated.h"

class ARoomLifecycleActor;
class FDataValidationContext;
class UHealthComponent;

/**
 * Completes a room when all explicitly configured combat targets are dead.
 *
 * The objective depends only on shared gameplay capabilities:
 * UHealthComponent provides death state/events and IRoomResettable ensures
 * each required target can participate correctly in room retries.
 *
 * Enemy-specific classes are deliberately not part of this contract.
 */
UCLASS()
class AI_GAVIN_PROJECT_API ARoomCombatObjective : public AActor
{
    GENERATED_BODY()

public:
    ARoomCombatObjective();

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(
        FDataValidationContext &Context) const override;
#endif

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason) override;

    /**
     * Room whose completion this objective controls.
     */
    UPROPERTY(
        EditInstanceOnly,
        Category = "Room|Objective")
    TObjectPtr<ARoomLifecycleActor> RoomLifecycle;

    /**
     * Actors that must all be dead before the room can complete.
     *
     * Every unique target must contain a UHealthComponent and implement
     * IRoomResettable so player-death retries restore objective state.
     */
    UPROPERTY(
        EditInstanceOnly,
        Category = "Room|Objective")
    TArray<TObjectPtr<AActor>> RequiredTargets;

private:
    bool InitializeObjective();
    void UnbindObjective();
    void QueueCompletionEvaluation();
    void HandleDeferredCompletionEvaluation();
    void EvaluateCompletion();

    UFUNCTION()
    void HandleRequiredTargetDeath(AActor *DamageCauser);

    UFUNCTION()
    void HandleRoomStateChanged(
        ERoomLifecycleState OldState,
        ERoomLifecycleState NewState);

    TArray<TWeakObjectPtr<UHealthComponent>>
        RequiredHealthComponents;

    bool bInitialized = false;
    bool bCompletionEvaluationPending = false;
};
