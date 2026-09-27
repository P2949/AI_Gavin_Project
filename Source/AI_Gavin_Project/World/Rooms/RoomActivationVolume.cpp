#include "World/Rooms/RoomActivationVolume.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "World/Rooms/RoomLifecycleActor.h"

ARoomActivationVolume::ARoomActivationVolume()
{
    PrimaryActorTick.bCanEverTick = false;

    ActivationBox =
        CreateDefaultSubobject<UBoxComponent>(
            TEXT("ActivationBox"));

    SetRootComponent(ActivationBox);

    ActivationBox->SetBoxExtent(
        FVector(100.0f, 100.0f, 100.0f));

    ActivationBox->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly);

    ActivationBox->SetCollisionObjectType(
        ECC_WorldDynamic);

    ActivationBox->SetCollisionResponseToAllChannels(
        ECR_Ignore);

    ActivationBox->SetCollisionResponseToChannel(
        ECC_Pawn,
        ECR_Overlap);

    ActivationBox->SetGenerateOverlapEvents(true);
    ActivationBox->SetCanEverAffectNavigation(false);
    ActivationBox->SetHiddenInGame(true);

    ActivationBox->OnComponentBeginOverlap.AddDynamic(
        this,
        &ARoomActivationVolume::HandleActivationOverlap);
}

void ARoomActivationVolume::BeginPlay()
{
    Super::BeginPlay();

    if (!IsValid(RoomLifecycle))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "RoomActivationVolume '%s' requires a valid "
                "RoomLifecycleActor."),
            *GetName());
    }
}

void ARoomActivationVolume::HandleActivationOverlap(
    UPrimitiveComponent *OverlappedComponent,
    AActor *OtherActor,
    UPrimitiveComponent *OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult &SweepResult)
{
    static_cast<void>(OverlappedComponent);
    static_cast<void>(OtherComponent);
    static_cast<void>(OtherBodyIndex);
    static_cast<void>(bFromSweep);
    static_cast<void>(SweepResult);

    APawn *EnteringPawn = Cast<APawn>(OtherActor);

    if (!EnteringPawn ||
        !EnteringPawn->IsPlayerControlled() ||
        !IsValid(RoomLifecycle))
    {
        return;
    }

    RoomLifecycle->ActivateRoom(EnteringPawn);
}
