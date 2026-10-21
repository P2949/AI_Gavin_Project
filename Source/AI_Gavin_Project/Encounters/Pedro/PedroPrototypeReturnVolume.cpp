#include "Encounters/Pedro/PedroPrototypeReturnVolume.h"

#include "AI_Gavin_Project.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

APedroPrototypeReturnVolume::APedroPrototypeReturnVolume()
{
    PrimaryActorTick.bCanEverTick = false;

    ReturnVolume =
        CreateDefaultSubobject<UBoxComponent>(
            TEXT("ReturnVolume"));

    SetRootComponent(ReturnVolume);

    ReturnVolume->SetBoxExtent(
        FVector(
            100.0f,
            100.0f,
            100.0f));

    ReturnVolume->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly);

    ReturnVolume->SetCollisionObjectType(
        ECC_WorldDynamic);

    ReturnVolume->SetCollisionResponseToAllChannels(
        ECR_Ignore);

    ReturnVolume->SetCollisionResponseToChannel(
        ECC_Pawn,
        ECR_Overlap);

    ReturnVolume->SetGenerateOverlapEvents(true);
    ReturnVolume->SetCanEverAffectNavigation(false);
    ReturnVolume->SetHiddenInGame(true);

    ReturnVolume->OnComponentBeginOverlap.AddDynamic(
        this,
        &APedroPrototypeReturnVolume::HandleReturnOverlap);
}

#if WITH_EDITOR

EDataValidationResult
APedroPrototypeReturnVolume::IsDataValid(
    FDataValidationContext &Context) const
{
    EDataValidationResult Result =
        CombineDataValidationResults(
            Super::IsDataValid(Context),
            EDataValidationResult::Valid);

    if (ReturnMap.IsNull())
    {
        Context.AddError(
            FText::Format(
                NSLOCTEXT(
                    "PedroPrototypeReturnVolume",
                    "MissingReturnMap",
                    "Pedro prototype return volume '{0}' "
                    "requires a ReturnMap."),
                FText::FromString(GetName())));

        Result = CombineDataValidationResults(
            Result,
            EDataValidationResult::Invalid);
    }

    return Result;
}

#endif

void APedroPrototypeReturnVolume::BeginPlay()
{
    Super::BeginPlay();

    if (ReturnMap.IsNull())
    {
        UE_LOG(
            LogAI_Gavin_Project,
            Error,
            TEXT(
                "PedroPrototypeReturnVolume '%s' "
                "requires a ReturnMap."),
            *GetName());
    }
}

void APedroPrototypeReturnVolume::HandleReturnOverlap(
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

    APawn *EnteringPawn =
        Cast<APawn>(OtherActor);

    if (!EnteringPawn ||
        !EnteringPawn->IsPlayerControlled() ||
        ReturnMap.IsNull())
    {
        return;
    }

    UGameplayStatics::OpenLevelBySoftObjectPtr(
        this,
        ReturnMap,
        true,
        FString());
}
