#include "World/Rooms/RoomLifecycleActor.h"

#include "Combat/Health/HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "World/Rooms/RoomResettable.h"

ARoomLifecycleActor::ARoomLifecycleActor()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ARoomLifecycleActor::BeginPlay()
{
    Super::BeginPlay();

    RegisterInitialResetParticipants();

    if (!bAutoActivateOnBeginPlay)
    {
        return;
    }

    APawn *Player =
        UGameplayStatics::GetPlayerPawn(this, 0);

    if (!Player)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "RoomLifecycleActor '%s' could not auto-activate "
                "because no player pawn was found."),
            *GetName());

        return;
    }

    ActivateRoom(Player);
}

void ARoomLifecycleActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    DeactivateRoom();

    RegisteredParticipants.Reset();

    Super::EndPlay(EndPlayReason);
}

bool ARoomLifecycleActor::ActivateRoom(APawn *Player)
{
    if (!IsValid(Player))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "RoomLifecycleActor '%s' cannot activate with "
                "an invalid player."),
            *GetName());

        return false;
    }

    if (!Cast<IRoomResettable>(Player))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "RoomLifecycleActor '%s' cannot activate because "
                "player '%s' does not implement IRoomResettable."),
            *GetName(),
            *Player->GetName());

        return false;
    }

    UHealthComponent *PlayerHealth =
        Player->FindComponentByClass<UHealthComponent>();

    if (!PlayerHealth)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "RoomLifecycleActor '%s' cannot activate because "
                "player '%s' has no HealthComponent."),
            *GetName(),
            *Player->GetName());

        return false;
    }

    DeactivateRoom();

    ActivePlayer = Player;
    ActivePlayerHealthComponent = PlayerHealth;
    ActivePlayerResetTransform =
        ResolvePlayerResetTransform(Player);

    PlayerHealth->OnDeath.AddDynamic(
        this,
        &ARoomLifecycleActor::HandleActivePlayerDeath);

    bIsRoomActive = true;

    return true;
}

void ARoomLifecycleActor::DeactivateRoom()
{
    if (UHealthComponent *PlayerHealth =
            ActivePlayerHealthComponent.Get())
    {
        PlayerHealth->OnDeath.RemoveDynamic(
            this,
            &ARoomLifecycleActor::HandleActivePlayerDeath);
    }

    ActivePlayer.Reset();
    ActivePlayerHealthComponent.Reset();

    bIsRoomActive = false;
    bResetPending = false;
}

bool ARoomLifecycleActor::RegisterResetParticipant(
    AActor *Participant)
{
    if (!IsValid(Participant) ||
        Participant == this)
    {
        return false;
    }

    if (!Cast<IRoomResettable>(Participant))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "RoomLifecycleActor '%s' cannot register '%s' "
                "because it does not implement IRoomResettable."),
            *GetName(),
            *Participant->GetName());

        return false;
    }

    const bool bAlreadyRegistered =
        RegisteredParticipants.ContainsByPredicate(
            [Participant](
                const FRegisteredResetParticipant &Entry)
            {
                return Entry.Actor.Get() == Participant;
            });

    if (bAlreadyRegistered)
    {
        return true;
    }

    FRegisteredResetParticipant &NewEntry =
        RegisteredParticipants.AddDefaulted_GetRef();

    NewEntry.Actor = Participant;
    NewEntry.ResetTransform =
        Participant->GetActorTransform();

    return true;
}

bool ARoomLifecycleActor::UnregisterResetParticipant(
    AActor *Participant)
{
    if (!Participant)
    {
        return false;
    }

    const int32 RemovedCount =
        RegisteredParticipants.RemoveAll(
            [Participant](
                const FRegisteredResetParticipant &Entry)
            {
                return Entry.Actor.Get() == Participant;
            });

    return RemovedCount > 0;
}

void ARoomLifecycleActor::RequestRoomReset()
{
    if (!bIsRoomActive ||
        bResetPending)
    {
        return;
    }

    bResetPending = true;

    GetWorldTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateUObject(
            this,
            &ARoomLifecycleActor::PerformRoomReset));
}

void ARoomLifecycleActor::HandleActivePlayerDeath(
    AActor *DamageCauser)
{
    static_cast<void>(DamageCauser);

    RequestRoomReset();
}

void ARoomLifecycleActor::RegisterInitialResetParticipants()
{
    for (AActor *Participant : InitialResetParticipants)
    {
        if (!Participant)
        {
            continue;
        }

        RegisterResetParticipant(Participant);
    }
}

void ARoomLifecycleActor::PerformRoomReset()
{
    if (!bResetPending)
    {
        return;
    }

    bResetPending = false;

    if (!bIsRoomActive)
    {
        return;
    }

    APawn *Player = ActivePlayer.Get();

    if (!IsValid(Player))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "RoomLifecycleActor '%s' lost its active player "
                "before the room reset could execute."),
            *GetName());

        DeactivateRoom();
        return;
    }

    if (IRoomResettable *ResettablePlayer =
            Cast<IRoomResettable>(Player))
    {
        ResettablePlayer->ResetForRoom(
            ActivePlayerResetTransform);
    }

    for (const FRegisteredResetParticipant &Entry :
         RegisteredParticipants)
    {
        AActor *Participant = Entry.Actor.Get();

        if (!IsValid(Participant) ||
            Participant == Player)
        {
            continue;
        }

        if (IRoomResettable *ResettableParticipant =
                Cast<IRoomResettable>(Participant))
        {
            ResettableParticipant->ResetForRoom(
                Entry.ResetTransform);
        }
    }

    RegisteredParticipants.RemoveAll(
        [](const FRegisteredResetParticipant &Entry)
        {
            return !Entry.Actor.IsValid();
        });
}

FTransform ARoomLifecycleActor::ResolvePlayerResetTransform(
    const APawn *Player) const
{
    if (IsValid(PlayerResetPoint))
    {
        return PlayerResetPoint->GetActorTransform();
    }

    return Player
        ? Player->GetActorTransform()
        : FTransform::Identity;
}
