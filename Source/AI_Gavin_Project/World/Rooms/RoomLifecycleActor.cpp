#include "World/Rooms/RoomLifecycleActor.h"

#include "Combat/Health/HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "World/Rooms/RoomResettable.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

ARoomLifecycleActor::ARoomLifecycleActor()
{
    PrimaryActorTick.bCanEverTick = false;
}

#if WITH_EDITOR
EDataValidationResult ARoomLifecycleActor::IsDataValid(
    FDataValidationContext &Context) const
{
    EDataValidationResult Result =
        CombineDataValidationResults(
            Super::IsDataValid(Context),
            EDataValidationResult::Valid);

    TSet<AActor *> SeenParticipants;

    for (AActor *Participant : InitialResetParticipants)
    {
        if (!IsValid(Participant))
        {
            Context.AddWarning(
                FText::Format(
                    NSLOCTEXT(
                        "RoomLifecycleActor",
                        "InvalidInitialResetParticipant",
                        "Room lifecycle '{0}' contains an invalid "
                        "initial reset participant."),
                    FText::FromString(GetName())));
            continue;
        }

        if (Participant == this)
        {
            Context.AddError(
                FText::Format(
                    NSLOCTEXT(
                        "RoomLifecycleActor",
                        "SelfInitialResetParticipant",
                        "Room lifecycle '{0}' cannot register itself "
                        "as an initial reset participant."),
                    FText::FromString(GetName())));

            Result = CombineDataValidationResults(
                Result,
                EDataValidationResult::Invalid);
            continue;
        }

        if (SeenParticipants.Contains(Participant))
        {
            Context.AddWarning(
                FText::Format(
                    NSLOCTEXT(
                        "RoomLifecycleActor",
                        "DuplicateInitialResetParticipant",
                        "Room lifecycle '{0}' contains duplicate "
                        "initial reset participant '{1}'."),
                    FText::FromString(GetName()),
                    FText::FromString(Participant->GetName())));
            continue;
        }

        SeenParticipants.Add(Participant);

        if (!Participant->GetClass()->ImplementsInterface(
                URoomResettable::StaticClass()))
        {
            Context.AddError(
                FText::Format(
                    NSLOCTEXT(
                        "RoomLifecycleActor",
                        "ParticipantNotResettable",
                        "Room lifecycle '{0}' cannot use initial "
                        "reset participant '{1}' because it does not "
                        "implement IRoomResettable."),
                    FText::FromString(GetName()),
                    FText::FromString(Participant->GetName())));

            Result = CombineDataValidationResults(
                Result,
                EDataValidationResult::Invalid);
        }
    }

    return Result;
}
#endif

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

    if (IsRoomCompleted())
    {
        return false;
    }

    if (IsRoomActive())
    {
        if (ActivePlayer.Get() == Player)
        {
            return true;
        }

        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "RoomLifecycleActor '%s' is already active for "
                "player '%s' and cannot switch to player '%s'."),
            *GetName(),
            *GetNameSafe(ActivePlayer.Get()),
            *Player->GetName());

        return false;
    }

    if (!Player->GetClass()->ImplementsInterface(
            URoomResettable::StaticClass()))
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

    ActivePlayer = Player;
    ActivePlayerHealthComponent = PlayerHealth;
    ActivePlayerResetTransform =
        ResolvePlayerResetTransform(Player);

    PlayerHealth->OnDeath.AddDynamic(
        this,
        &ARoomLifecycleActor::HandleActivePlayerDeath);

    SetRoomState(ERoomLifecycleState::Active);

    return true;
}

void ARoomLifecycleActor::DeactivateRoom()
{
    bResetPending = false;
    bCompletionPending = false;
    ClearActivePlayerTracking();

    if (IsRoomActive())
    {
        SetRoomState(ERoomLifecycleState::Inactive);
    }
}

bool ARoomLifecycleActor::CompleteRoom()
{
    if (!IsRoomActive() ||
        bResetPending)
    {
        return false;
    }

    UHealthComponent *PlayerHealth =
        ActivePlayerHealthComponent.Get();

    if (!IsValid(PlayerHealth))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "RoomLifecycleActor '%s' cannot complete because "
                "its active player HealthComponent is invalid."),
            *GetName());

        return false;
    }

    if (PlayerHealth->IsDead())
    {
        RequestRoomReset();
        return false;
    }

    if (bCompletionPending)
    {
        return true;
    }

    bCompletionPending = true;

    GetWorldTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateUObject(
            this,
            &ARoomLifecycleActor::PerformRoomCompletion));

    return true;
}

void ARoomLifecycleActor::PerformRoomCompletion()
{
    if (!bCompletionPending)
    {
        return;
    }

    bCompletionPending = false;

    if (!IsRoomActive() ||
        bResetPending)
    {
        return;
    }

    UHealthComponent *PlayerHealth =
        ActivePlayerHealthComponent.Get();

    if (!IsValid(PlayerHealth))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "RoomLifecycleActor '%s' lost its active player "
                "HealthComponent before room completion."),
            *GetName());

        return;
    }

    if (PlayerHealth->IsDead())
    {
        RequestRoomReset();
        return;
    }

    ClearActivePlayerTracking();
    SetRoomState(ERoomLifecycleState::Completed);
}

void ARoomLifecycleActor::ClearActivePlayerTracking()
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
}

void ARoomLifecycleActor::SetRoomState(
    ERoomLifecycleState NewState)
{
    if (RoomState == NewState)
    {
        return;
    }

    const ERoomLifecycleState OldState = RoomState;
    RoomState = NewState;

    OnRoomStateChanged.Broadcast(
        OldState,
        RoomState);
}

bool ARoomLifecycleActor::RegisterResetParticipant(
    AActor *Participant)
{
    if (!IsValid(Participant) ||
        Participant == this)
    {
        return false;
    }

    if (!Participant->GetClass()->ImplementsInterface(
            URoomResettable::StaticClass()))
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
    if (!IsRoomActive())
    {
        return;
    }

    // Player death has priority over an unresolved room-completion
    // request. A queued completion callback becomes a harmless no-op.
    bCompletionPending = false;

    if (bResetPending)
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
    bCompletionPending = false;

    if (!IsRoomActive())
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

    if (Player->GetClass()->ImplementsInterface(
            URoomResettable::StaticClass()))
    {
        IRoomResettable::Execute_ResetForRoom(
            Player,
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

        if (Participant->GetClass()->ImplementsInterface(
                URoomResettable::StaticClass()))
        {
            IRoomResettable::Execute_ResetForRoom(
                Participant,
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
