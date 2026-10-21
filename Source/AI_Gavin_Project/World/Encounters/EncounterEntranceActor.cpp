// EncounterEntranceActor.cpp

#include "World/Encounters/EncounterEntranceActor.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

#include "World/Encounters/EncounterDefinition.h"
#include "World/Encounters/EncounterProgressSubsystem.h"
#include "Components/BoxComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

AEncounterEntranceActor::AEncounterEntranceActor()
{
    PrimaryActorTick.bCanEverTick = false;

    InteractionBounds =
        CreateDefaultSubobject<UBoxComponent>(
            TEXT("InteractionBounds"));

    SetRootComponent(InteractionBounds);

    InteractionBounds->InitBoxExtent(
        FVector(
            50.0,
            50.0,
            100.0));

    InteractionBounds->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly);

    InteractionBounds->SetCollisionResponseToAllChannels(
        ECR_Ignore);

    InteractionBounds->SetCollisionResponseToChannel(
        ECC_Visibility,
        ECR_Block);

    InteractionBounds->SetGenerateOverlapEvents(false);
}

void AEncounterEntranceActor::BeginPlay()
{
    Super::BeginPlay();

    RefreshResolvedPresentation();
}

void AEncounterEntranceActor::Interact_Implementation(
    AActor *Interactor)
{
    static_cast<void>(Interactor);

    if (!IsValid(EncounterDefinition))
    {
        return;
    }

    const TSoftObjectPtr<UWorld> &EncounterMap =
        EncounterDefinition->GetEncounterMap();

    if (EncounterMap.IsNull())
    {
        return;
    }

    UGameplayStatics::OpenLevelBySoftObjectPtr(
        this,
        EncounterMap,
        true,
        FString());
}

void AEncounterEntranceActor::RefreshResolvedPresentation()
{
    if (!IsValid(ResolvedIndicatorActor))
    {
        return;
    }

    UWorld *World = GetWorld();

    if (!World || !IsValid(EncounterDefinition))
    {
        ResolvedIndicatorActor->SetActorHiddenInGame(true);
        return;
    }

    UGameInstance *GameInstance =
        World->GetGameInstance();

    if (!GameInstance)
    {
        ResolvedIndicatorActor->SetActorHiddenInGame(true);
        return;
    }

    UEncounterProgressSubsystem *ProgressSubsystem =
        GameInstance->GetSubsystem<
            UEncounterProgressSubsystem>();

    const bool bResolved =
        ProgressSubsystem &&
        ProgressSubsystem->IsEncounterResolved(
            EncounterDefinition);

    ResolvedIndicatorActor->SetActorHiddenInGame(
        !bResolved);
}

#if WITH_EDITOR

EDataValidationResult AEncounterEntranceActor::IsDataValid(
    FDataValidationContext &Context) const
{
    EDataValidationResult Result =
        Super::IsDataValid(Context);

    if (!IsValid(EncounterDefinition))
    {
        Context.AddError(
            FText::FromString(
                TEXT(
                    "Encounter entrance requires an "
                    "EncounterDefinition.")));

        Result = CombineDataValidationResults(
            Result,
            EDataValidationResult::Invalid);
    }

    return Result;
}

#endif
