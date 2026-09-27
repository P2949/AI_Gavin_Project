#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoomLifecycleActor.generated.h"

class APawn;
class UHealthComponent;

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

    bool IsRoomActive() const
    {
        return bIsRoomActive;
    }

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason) override;

    /**
     * Convenience for the current single-room first playable.
     *
     * Future room entry logic can call ActivateRoom explicitly instead.
     */
    UPROPERTY(
        EditInstanceOnly,
        Category = "Room|Lifecycle")
    bool bAutoActivateOnBeginPlay = true;

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

    FTransform ResolvePlayerResetTransform(
        const APawn *Player) const;

    TWeakObjectPtr<APawn> ActivePlayer;
    TWeakObjectPtr<UHealthComponent> ActivePlayerHealthComponent;

    TArray<FRegisteredResetParticipant> RegisteredParticipants;

    FTransform ActivePlayerResetTransform;

    bool bIsRoomActive = false;
    bool bResetPending = false;
};
