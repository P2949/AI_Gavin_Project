#include "Encounters/Pedro/PedroEncounterDirector.h"

#include "Encounters/Pedro/PedroObservationSourceComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

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
    bOutcomeEvaluationQueued = false;

    SetOutcomeActorActive(
        ExploratoryOutcomeActor,
        false);

    SetOutcomeActorActive(
        RepetitiveOutcomeActor,
        false);

    // Factual sources do not require a valid player interaction component.
    // Register them before the later player lookup can return early.
    DiscoverLevelObservationSources();

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

    InteractionComponent->OnInteractionDispatched.AddDynamic(
        this,
        &APedroEncounterDirector::HandleInteractionDispatched);
}

void APedroEncounterDirector::DiscoverLevelObservationSources()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // Discover Pedro-specific components once on level-placed actors.
    // Runtime-spawned sources must call RegisterObservationSource themselves.
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
        {
            continue;
        }

        TArray<UPedroObservationSourceComponent*> Sources;
        Actor->GetComponents<UPedroObservationSourceComponent>(Sources);

        for (UPedroObservationSourceComponent* Source : Sources)
        {
            RegisterObservationSource(Source);
        }
    }
}

bool APedroEncounterDirector::RegisterObservationSource(
    UPedroObservationSourceComponent* Source)
{
    if (bHasPresentedOutcome || !IsValid(Source) ||
        !IsValid(Source->GetOwner()))
    {
        return false;
    }

    // A Director may not subscribe to another World's sources.
    // Null-world objects are allowed so the logic can be unit-tested.
    if (const UWorld* World = GetWorld())
    {
        if (Source->GetWorld() != World)
        {
            return false;
        }
    }

    for (const TWeakObjectPtr<UPedroObservationSourceComponent>& Existing : BoundSources)
    {
        if (Existing.Get() == Source)
        {
            return false;
        }
    }

    Source->OnWorldEventProduced.AddUObject(
        this, &APedroEncounterDirector::HandleWorldEventProduced);
    BoundSources.Add(Source);
    return true;
}

void APedroEncounterDirector::HandleWorldEventProduced(
    const FPedroBehaviourEvent& Event)
{
    if (bHasPresentedOutcome || !bUsePrototypeAssumedAcquisition)
    {
        return;
    }

    // This is NOT real perception or a witness claim. The temporary policy
    // assumes awareness only to exercise the observation-to-score pipeline.
    FPedroEvidenceRecord Evidence;
    Evidence.Event = Event;
    Evidence.KnowledgeSource = EPedroKnowledgeSource::PrototypeAssumed;
    Evidence.AcquiredAtWorldTimeSeconds = GetWorld()
        ? GetWorld()->GetTimeSeconds()
        : Event.WorldTimeSeconds;

    if (ObservationState.RecordEvidence(Evidence))
    {
        UE_LOG(LogTemp, Log,
            TEXT("Pedro PROTOTYPE-ASSUMED evidence (not LOS): kind=%d "
                 "subject=%s opportunity=%s total=%d"),
            static_cast<int32>(Event.Kind),
            *Event.SubjectId.ToString(),
            *Event.OpportunityId.ToString(),
            ObservationState.GetEvidenceCount());
    }
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

    // OnInteractionDispatched is fired before the actual Interact call.
    // Defer the old outcome by one tick so this interaction can report a
    // confirmed factual event before the encounter is marked resolved.
    if (ObservationState.InteractionCount >= InteractionsBeforeOutcome)
    {
        QueuePrototypeOutcomeEvaluation();
    }
}

void APedroEncounterDirector::QueuePrototypeOutcomeEvaluation()
{
    if (bHasPresentedOutcome || bOutcomeEvaluationQueued)
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        bOutcomeEvaluationQueued = true;
        PendingPrototypeOutcomeTimer = World->GetTimerManager().SetTimerForNextTick(
            this, &APedroEncounterDirector::EvaluatePrototypeOutcome);
    }
    else
    {
        // Only needed when evaluating a worldless Director in isolated tests.
        EvaluatePrototypeOutcome();
    }
}

void APedroEncounterDirector::EvaluatePrototypeOutcome()
{
    bOutcomeEvaluationQueued = false;

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

void APedroEncounterDirector::UnregisterObservationSources()
{
    for (const TWeakObjectPtr<UPedroObservationSourceComponent>& Source : BoundSources)
    {
        if (UPedroObservationSourceComponent* ValidSource = Source.Get())
        {
            ValidSource->OnWorldEventProduced.RemoveAll(this);
        }
    }

    BoundSources.Reset();
}

void APedroEncounterDirector::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(InteractionComponent))
    {
        InteractionComponent->OnInteractionDispatched.RemoveDynamic(
            this,
            &APedroEncounterDirector::HandleInteractionDispatched);
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PendingPrototypeOutcomeTimer);
    }

    UnregisterObservationSources();
    InteractionComponent = nullptr;
    UniqueInteractedActors.Reset();
    bOutcomeEvaluationQueued = false;

    Super::EndPlay(EndPlayReason);
}
