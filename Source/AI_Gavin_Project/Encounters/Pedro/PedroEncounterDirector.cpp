#include "Encounters/Pedro/PedroEncounterDirector.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

#include "Player/Interaction/PlayerInteractionComponent.h"
#include "World/Encounters/EncounterDefinition.h"
#include "World/Encounters/EncounterProgressSubsystem.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

APedroEncounterDirector::APedroEncounterDirector()
{
    PrimaryActorTick.bCanEverTick = false;
}

#if WITH_EDITOR

EDataValidationResult APedroEncounterDirector::IsDataValid(
    FDataValidationContext &Context) const
{
    EDataValidationResult Result =
        CombineDataValidationResults(
            Super::IsDataValid(Context),
            EDataValidationResult::Valid);

    if (!IsValid(EncounterDefinition))
    {
        Context.AddError(
            FText::FromString(
                TEXT(
                    "Pedro encounter director requires an "
                    "EncounterDefinition.")));

        Result = CombineDataValidationResults(
            Result,
            EDataValidationResult::Invalid);
    }

    if (!IsValid(ExploratoryOutcomeActor))
    {
        Context.AddError(
            FText::FromString(
                TEXT(
                    "Pedro encounter director requires an "
                    "ExploratoryOutcomeActor.")));

        Result = CombineDataValidationResults(
            Result,
            EDataValidationResult::Invalid);
    }

    if (!IsValid(RepetitiveOutcomeActor))
    {
        Context.AddError(
            FText::FromString(
                TEXT(
                    "Pedro encounter director requires a "
                    "RepetitiveOutcomeActor.")));

        Result = CombineDataValidationResults(
            Result,
            EDataValidationResult::Invalid);
    }

    if (IsValid(ExploratoryOutcomeActor) &&
        ExploratoryOutcomeActor == RepetitiveOutcomeActor)
    {
        Context.AddError(
            FText::FromString(
                TEXT(
                    "Pedro encounter director outcome actors "
                    "must be different actors.")));

        Result = CombineDataValidationResults(
            Result,
            EDataValidationResult::Invalid);
    }

    if (InteractionsBeforeOutcome < 1)
    {
        Context.AddError(
            FText::FromString(
                TEXT(
                    "InteractionsBeforeOutcome must be at "
                    "least 1.")));

        Result = CombineDataValidationResults(
            Result,
            EDataValidationResult::Invalid);
    }

    if (UniqueInteractionsForExploratoryOutcome < 1)
    {
        Context.AddError(
            FText::FromString(
                TEXT(
                    "UniqueInteractionsForExploratoryOutcome "
                    "must be at least 1.")));

        Result = CombineDataValidationResults(
            Result,
            EDataValidationResult::Invalid);
    }

    if (UniqueInteractionsForExploratoryOutcome >
        InteractionsBeforeOutcome)
    {
        Context.AddError(
            FText::FromString(
                TEXT(
                    "UniqueInteractionsForExploratoryOutcome "
                    "cannot exceed InteractionsBeforeOutcome.")));

        Result = CombineDataValidationResults(
            Result,
            EDataValidationResult::Invalid);
    }

    return Result;
}

#endif

void APedroEncounterDirector::BeginPlay()
{
    Super::BeginPlay();

    ObservationState.Reset();
    UniqueInteractedActors.Reset();
    bHasPresentedOutcome = false;

    SetOutcomeActorActive(
        ExploratoryOutcomeActor,
        false);

    SetOutcomeActorActive(
        RepetitiveOutcomeActor,
        false);

    ACharacter *PlayerCharacter =
        UGameplayStatics::GetPlayerCharacter(
            this,
            0);

    if (!IsValid(PlayerCharacter))
    {
        return;
    }

    InteractionComponent =
        PlayerCharacter
            ->FindComponentByClass<
                UPlayerInteractionComponent>();

    if (!IsValid(InteractionComponent))
    {
        return;
    }

    InteractionComponent
        ->OnInteractionDispatched
        .AddDynamic(
            this,
            &APedroEncounterDirector::
                HandleInteractionDispatched);
}

void APedroEncounterDirector::HandleInteractionDispatched(
    AActor *InteractionTarget,
    AActor *Interactor)
{
    static_cast<void>(Interactor);

    if (bHasPresentedOutcome ||
        !IsValid(InteractionTarget))
    {
        return;
    }

    ++ObservationState.InteractionCount;

    UniqueInteractedActors.Add(
        InteractionTarget);

    ObservationState.UniqueInteractionCount =
        UniqueInteractedActors.Num();

    EvaluatePrototypeOutcome();
}

void APedroEncounterDirector::EvaluatePrototypeOutcome()
{
    if (bHasPresentedOutcome ||
        ObservationState.InteractionCount <
            InteractionsBeforeOutcome)
    {
        return;
    }

    const bool bExploratoryOutcome =
        ObservationState.UniqueInteractionCount >=
        UniqueInteractionsForExploratoryOutcome;

    PresentPrototypeOutcome(
        bExploratoryOutcome);
}

void APedroEncounterDirector::PresentPrototypeOutcome(
    bool bExploratoryOutcome)
{
    if (bHasPresentedOutcome)
    {
        return;
    }

    bHasPresentedOutcome = true;

    SetOutcomeActorActive(
        ExploratoryOutcomeActor,
        bExploratoryOutcome);

    SetOutcomeActorActive(
        RepetitiveOutcomeActor,
        !bExploratoryOutcome);

    if (!IsValid(EncounterDefinition))
    {
        return;
    }

    UWorld *World = GetWorld();

    if (!World)
    {
        return;
    }

    UGameInstance *GameInstance =
        World->GetGameInstance();

    if (!GameInstance)
    {
        return;
    }

    UEncounterProgressSubsystem *ProgressSubsystem =
        GameInstance->GetSubsystem<
            UEncounterProgressSubsystem>();

    if (!ProgressSubsystem)
    {
        return;
    }

    ProgressSubsystem->ResolveEncounter(
        EncounterDefinition);
}

void APedroEncounterDirector::SetOutcomeActorActive(
    AActor *OutcomeActor,
    bool bActive)
{
    if (!IsValid(OutcomeActor))
    {
        return;
    }

    OutcomeActor->SetActorHiddenInGame(
        !bActive);

    OutcomeActor->SetActorEnableCollision(
        bActive);
}

void APedroEncounterDirector::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(InteractionComponent))
    {
        InteractionComponent
            ->OnInteractionDispatched
            .RemoveDynamic(
                this,
                &APedroEncounterDirector::
                    HandleInteractionDispatched);
    }

    InteractionComponent = nullptr;
    UniqueInteractedActors.Reset();

    Super::EndPlay(EndPlayReason);
}
