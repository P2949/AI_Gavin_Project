#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/Rooms/RoomLifecycleTypes.h"
#include "RoomLifecycleActor.generated.h"

class APawn;
class UHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnRoomLifecycleStateChanged,
    ERoomLifecycleState,
    OldState,
    ERoomLifecycleState,
    NewState);

/**
 * Coordinates reset lifecycle for one room.
 *
 * The room owns reset timing, membership, and reset transforms.
 * Individual participants remain responsible for restoring their own
 * internal gameplay state through IRoomResettable.
 */
UCLASS()
class AI_GAVIN_PROJECT_API ARoomLifecycleActor : public AActor
{
    GENERATED_BODY()

public:
    ARoomLifecycleActor();

    /**
     * Makes this room responsible for the supplied player.
     *
     * Activation snapshots the player's reset transform unless an
     * explicit PlayerResetPoint is configured.
     */
    bool ActivateRoom(APawn *Player);

    /**
     * Stops observing the currently active player.
     */
    void DeactivateRoom();

    /**
     * Registers a resettable actor and snapshots its current transform.
     *
     * Registering the same actor more than once is harmless and keeps
     * the original reset transform.
     */
    bool RegisterResetParticipant(AActor *Participant);

    /**
     * Removes a previously registered participant.
     */
    bool UnregisterResetParticipant(AActor *Participant);

    /**
     * Queues a room reset for the next game tick.
     *
     * Repeated requests before the reset executes are collapsed into
     * one reset.
     */
    void RequestRoomReset();

    /**
     * Permanently completes this room instance.
     *
     * Completion is accepted only while the room is Active.
     * The reason for completion belongs to the caller.
     */
    UFUNCTION(BlueprintCallable, Category = "Room|Lifecycle")
    bool CompleteRoom();

    UFUNCTION(BlueprintPure, Category = "Room|Lifecycle")
    ERoomLifecycleState GetRoomState() const
    {
        return RoomState;
    }

    UFUNCTION(BlueprintPure, Category = "Room|Lifecycle")
    bool IsRoomActive() const
    {
        return RoomState == ERoomLifecycleState::Active;
    }

    UFUNCTION(BlueprintPure, Category = "Room|Lifecycle")
    bool IsRoomCompleted() const
    {
        return RoomState == ERoomLifecycleState::Completed;
    }

    /**
     * Broadcast whenever the high-level lifecycle state changes.
     */
    UPROPERTY(BlueprintAssignable, Category = "Room|Lifecycle")
    FOnRoomLifecycleStateChanged OnRoomStateChanged;

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason) override;

    /**
     * Optional convenience for deliberately configured single-room
     * or test setups.
     *
     * Multi-room gameplay should activate the appropriate room
     * explicitly when the player enters it.
     */
    UPROPERTY(
        EditInstanceOnly,
        Category = "Room|Lifecycle")
    bool bAutoActivateOnBeginPlay = false;

    /**
     * Optional authored location to which the active player resets.
     *
     * If unset, the player's transform at room activation is used.
     */
    UPROPERTY(
        EditInstanceOnly,
        Category = "Room|Reset")
    TObjectPtr<AActor> PlayerResetPoint;

    /**
     * Resettable non-player actors that belong to this room initially.
     *
     * Dynamic participants can register through
     * RegisterResetParticipant().
     */
    UPROPERTY(
        EditInstanceOnly,
        Category = "Room|Reset")
    TArray<TObjectPtr<AActor>> InitialResetParticipants;

private:
    struct FRegisteredResetParticipant
    {
        TWeakObjectPtr<AActor> Actor;
        FTransform ResetTransform;
    };

    UFUNCTION()
    void HandleActivePlayerDeath(AActor *DamageCauser);

    void RegisterInitialResetParticipants();
    void PerformRoomReset();
    void ClearActivePlayerTracking();
    void SetRoomState(ERoomLifecycleState NewState);

    FTransform ResolvePlayerResetTransform(
        const APawn *Player) const;

    TWeakObjectPtr<APawn> ActivePlayer;
    TWeakObjectPtr<UHealthComponent> ActivePlayerHealthComponent;

    TArray<FRegisteredResetParticipant> RegisteredParticipants;

    FTransform ActivePlayerResetTransform;

    UPROPERTY(VisibleInstanceOnly, Category = "Room|Lifecycle")
    ERoomLifecycleState RoomState =
        ERoomLifecycleState::Inactive;

    bool bResetPending = false;
};
