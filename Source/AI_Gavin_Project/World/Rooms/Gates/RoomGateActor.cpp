#include "World/Rooms/Gates/RoomGateActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "World/Rooms/RoomLifecycleActor.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

ARoomGateActor::ARoomGateActor()
{
    PrimaryActorTick.bCanEverTick = false;

    GateCollision =
        CreateDefaultSubobject<UBoxComponent>(
            TEXT("GateCollision"));

    SetRootComponent(GateCollision);

    GateCollision->SetBoxExtent(
        FVector(100.0f, 20.0f, 150.0f));

    GateCollision->SetCollisionObjectType(
        ECC_WorldDynamic);

    GateCollision->SetCollisionResponseToAllChannels(
        ECR_Block);

    GateCollision->SetCollisionEnabled(
        ECollisionEnabled::QueryAndPhysics);

    GateCollision->SetGenerateOverlapEvents(false);
    GateCollision->SetCanEverAffectNavigation(true);

    GateVisual =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("GateVisual"));

    GateVisual->SetupAttachment(GateCollision);
    GateVisual->SetCollisionEnabled(
        ECollisionEnabled::NoCollision);
}

#if WITH_EDITOR
EDataValidationResult ARoomGateActor::IsDataValid(
    FDataValidationContext &Context) const
{
    EDataValidationResult Result =
        CombineDataValidationResults(
            Super::IsDataValid(Context),
            EDataValidationResult::Valid);

    if (!IsValid(RoomLifecycle))
    {
        Context.AddError(
            FText::Format(
                NSLOCTEXT(
                    "RoomGateActor",
                    "MissingRoomLifecycle",
                    "Room gate '{0}' requires a valid "
                    "RoomLifecycleActor."),
                FText::FromString(GetName())));

        Result = CombineDataValidationResults(
            Result,
            EDataValidationResult::Invalid);
    }

    return Result;
}
#endif

void ARoomGateActor::BeginPlay()
{
    Super::BeginPlay();

    if (!IsValid(RoomLifecycle))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "RoomGateActor '%s' requires a valid "
                "RoomLifecycleActor."),
            *GetName());

        return;
    }

    RoomLifecycle->OnRoomStateChanged.AddDynamic(
        this,
        &ARoomGateActor::HandleRoomStateChanged);

    ApplyRoomState(
        RoomLifecycle->GetRoomState());
}

void ARoomGateActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(RoomLifecycle))
    {
        RoomLifecycle->OnRoomStateChanged.RemoveDynamic(
            this,
            &ARoomGateActor::HandleRoomStateChanged);
    }

    Super::EndPlay(EndPlayReason);
}

void ARoomGateActor::HandleRoomStateChanged(
    ERoomLifecycleState OldState,
    ERoomLifecycleState NewState)
{
    static_cast<void>(OldState);

    ApplyRoomState(NewState);
}

void ARoomGateActor::ApplyRoomState(
    ERoomLifecycleState State)
{
    const bool bShouldBeOpen =
        ShouldBeOpen(State);

    GateCollision->SetCollisionEnabled(
        bShouldBeOpen
            ? ECollisionEnabled::NoCollision
            : ECollisionEnabled::QueryAndPhysics);

    GateVisual->SetVisibility(
        !bShouldBeOpen,
        true);
}

bool ARoomGateActor::ShouldBeOpen(
    ERoomLifecycleState State) const
{
    switch (State)
    {
    case ERoomLifecycleState::Inactive:
        return bOpenWhileInactive;

    case ERoomLifecycleState::Active:
        return false;

    case ERoomLifecycleState::Completed:
        return true;

    default:
        return false;
    }
}
