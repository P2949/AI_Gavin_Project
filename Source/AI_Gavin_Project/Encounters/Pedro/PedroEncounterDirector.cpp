#include "Encounters/Pedro/PedroEncounterDirector.h"

#include "Encounters/Pedro/PedroActivePresentation.h"
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

namespace
{
const TCHAR* PedroShadowStatusName(EPedroCompletionStatus Status)
{
    switch (Status)
    {
    case EPedroCompletionStatus::NoCoreOpportunities:
        return TEXT("NoCoreOpportunities");
    case EPedroCompletionStatus::InvalidSampleFraction:
        return TEXT("InvalidSampleFraction");
    case EPedroCompletionStatus::NoAcquiredEvidence:
        return TEXT("NoAcquiredEvidence");
    case EPedroCompletionStatus::InsufficientCoreSamples:
        return TEXT("InsufficientCoreSamples");
    case EPedroCompletionStatus::InconsistentSampleEvidence:
        return TEXT("InconsistentSampleEvidence");
    case EPedroCompletionStatus::Ready:
        return TEXT("Ready");
    }
    return TEXT("Unknown");
}

const TCHAR* PedroShadowInterpretationName(EPedroInterpretation Interpretation)
{
    switch (Interpretation)
    {
    case EPedroInterpretation::Gentle:
        return TEXT("Gentle");
    case EPedroInterpretation::Seeking:
        return TEXT("Seeking");
    case EPedroInterpretation::Disruptive:
        return TEXT("Disruptive");
    }
    return TEXT("Unknown");
}
} // namespace

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

    if (CompletionPolicy == EPedroCompletionPolicy::LegacyInteractionCount)
    {
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

    }
    else
    {
        if (!IsValid(GentleOutcomeActor) ||
            !IsValid(SeekingOutcomeActor) ||
            !IsValid(DisruptiveOutcomeActor))
        {
            Context.AddError(FText::FromString(TEXT(
                "Evidence-driven Pedro requires Gentle, Seeking and Disruptive actors.")));
            Result = CombineDataValidationResults(
                Result, EDataValidationResult::Invalid);
        }
        else if (GentleOutcomeActor == SeekingOutcomeActor ||
                 GentleOutcomeActor == DisruptiveOutcomeActor ||
                 SeekingOutcomeActor == DisruptiveOutcomeActor)
        {
            Context.AddError(FText::FromString(TEXT(
                "Evidence-driven Pedro outcome actors must all be distinct.")));
            Result = CombineDataValidationResults(
                Result, EDataValidationResult::Invalid);
        }

        bool bHasCoreOpportunity = false;
        for (const FPedroOpportunityDefinition& Definition : OpportunityDefinitions)
        {
            if (Definition.Tier == EPedroOpportunityTier::Core)
            {
                bHasCoreOpportunity = true;
                break;
            }
        }
        if (!bHasCoreOpportunity)
        {
            Context.AddError(FText::FromString(TEXT(
                "Evidence-driven Pedro requires at least one authored core opportunity.")));
            Result = CombineDataValidationResults(
                Result, EDataValidationResult::Invalid);
        }
    }

    // CompletionPolicy must be one of the two supported values.
    if (CompletionPolicy != EPedroCompletionPolicy::LegacyInteractionCount &&
        CompletionPolicy != EPedroCompletionPolicy::EvidenceDriven)
    {
        Context.AddError(FText::FromString(TEXT(
            "Pedro completion policy has an unsupported value.")));
        Result = CombineDataValidationResults(
            Result, EDataValidationResult::Invalid);
    }

    if (CompletionPolicy == EPedroCompletionPolicy::EvidenceDriven &&
        IsValid(EncounterDefinition) &&
        EncounterDefinition->GetEncounterId().IsNone())
    {
        Context.AddError(FText::FromString(TEXT(
            "Evidence-driven Pedro requires a nonempty encounter ID.")));
        Result = CombineDataValidationResults(
            Result, EDataValidationResult::Invalid);
    }

    // No definitions is valid while the old interaction-count prototype remains.
    // When authored, each ID must be stable, nonempty and unique.
    TSet<FName> SeenOpportunityIds;
    for (const FPedroOpportunityDefinition& Definition : OpportunityDefinitions)
    {
        if (Definition.OpportunityId.IsNone() ||
            SeenOpportunityIds.Contains(Definition.OpportunityId))
        {
            Context.AddError(FText::FromString(TEXT(
                "Pedro opportunity IDs must be nonempty and unique.")));
            Result = CombineDataValidationResults(
                Result, EDataValidationResult::Invalid);
        }
        SeenOpportunityIds.Add(Definition.OpportunityId);
    }

    if (!FMath::IsFinite(CoreSampleFraction) ||
        CoreSampleFraction < 0.01f || CoreSampleFraction > 1.0f)
    {
        Context.AddError(FText::FromString(TEXT(
            "Pedro CoreSampleFraction must be between 0.01 and 1.0.")));
        Result = CombineDataValidationResults(
            Result, EDataValidationResult::Invalid);
    }

    return Result;
}

#endif

void APedroEncounterDirector::BeginPlay()
{
    Super::BeginPlay();

    ObservationState.Reset();
    if (!ObservationState.Opportunities.Configure(OpportunityDefinitions))
    {
        // Invalid authoring is already reported by IsDataValid().
        // Fail closed: do not count a partial or ambiguous opportunity set.
        UE_LOG(LogTemp, Error,
            TEXT("Pedro opportunity definitions contain duplicate or empty IDs."));
    }

    UniqueInteractedActors.Reset();
    bHasPresentedOutcome = false;
    bOutcomeEvaluationQueued = false;
    bShadowCompletionQueued = false;
    bHasShadowCompletionDecision = false;
    ShadowCompletionDecision = FPedroCompletionDecision{};
    bActiveCompletionQueued = false;
    bHasFinalClassification = false;
    FinalClassification = FPedroClassificationResult{};

    SetOutcomeActorActive(
        ExploratoryOutcomeActor,
        false);

    SetOutcomeActorActive(
        RepetitiveOutcomeActor,
        false);

    // Preserve the legacy level completely, even if these optional actors
    // are mistakenly assigned to an actor also used by the legacy path.
    if (CompletionPolicy == EPedroCompletionPolicy::EvidenceDriven)
    {
        SetOutcomeActorActive(GentleOutcomeActor, false);
        SetOutcomeActorActive(SeekingOutcomeActor, false);
        SetOutcomeActorActive(DisruptiveOutcomeActor, false);
    }

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
    Source->OnOpportunityExposed.AddUObject(
        this, &APedroEncounterDirector::HandleOpportunityExposed);
    BoundSources.Add(Source);
    return true;
}

void APedroEncounterDirector::HandleOpportunityExposed(FName OpportunityId)
{
    if (bHasPresentedOutcome)
    {
        return;
    }

    if (ObservationState.Opportunities.MarkPlayerExposed(OpportunityId))
    {
        UE_LOG(LogTemp, Log,
            TEXT("Pedro opportunity player-exposed (not a sighting): %s"),
            *OpportunityId.ToString());
        QueueShadowCompletionEvaluation();
        QueueActiveCompletionEvaluation();
    }
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

        // Track only explicitly configured core/bonus opportunities.
        // This threshold is informational until the final classification
        // and completion systems are implemented.
        const FPedroOpportunityState& Progress = ObservationState.Opportunities;
        UE_LOG(LogTemp, Log,
            TEXT("Pedro opportunity progress: core=%d/%d required=%d "
                 "bonus=%d/%d thresholdReached=%s (no auto-resolution)"),
            Progress.GetSampledCoreCount(),
            Progress.GetCoreCount(),
            Progress.GetRequiredCoreSamples(CoreSampleFraction),
            Progress.GetSampledBonusCount(),
            Progress.GetBonusCount(),
            Progress.HasSufficientCoreSamples(CoreSampleFraction)
                ? TEXT("yes") : TEXT("no"));

        QueueShadowCompletionEvaluation();
        QueueActiveCompletionEvaluation();
    }
}

void APedroEncounterDirector::HandleInteractionDispatched(
    AActor *InteractionTarget,
    AActor *Interactor)
{
    static_cast<void>(Interactor);

    // In active mode, dispatched clicks never advance legacy completion.
    if (CompletionPolicy != EPedroCompletionPolicy::LegacyInteractionCount)
    {
        return;
    }

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
    if (CompletionPolicy != EPedroCompletionPolicy::LegacyInteractionCount ||
        bHasPresentedOutcome || bOutcomeEvaluationQueued)
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

void APedroEncounterDirector::QueueShadowCompletionEvaluation()
{
    if (!bEnableCompletionGateShadowEvaluation || bShadowCompletionQueued)
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        // The pre-interaction hook runs before Interact(). Coalesce exposure
        // and evidence emitted by that action into one next-tick snapshot.
        bShadowCompletionQueued = true;
        PendingShadowCompletionTimer = World->GetTimerManager().SetTimerForNextTick(
            this, &APedroEncounterDirector::EvaluateShadowCompletionGate);
    }
    else
    {
        // Standalone automation tests use worldless transient Directors.
        EvaluateShadowCompletionGate();
    }
}

void APedroEncounterDirector::EvaluateShadowCompletionGate()
{
    bShadowCompletionQueued = false;

    if (!bEnableCompletionGateShadowEvaluation)
    {
        return;
    }

    const FPedroCompletionDecision NewDecision =
        PedroCompletionGate::Evaluate(ObservationState, CoreSampleFraction);

    const bool bMeaningfulChange =
        !bHasShadowCompletionDecision ||
        NewDecision.Status != ShadowCompletionDecision.Status ||
        NewDecision.SampledCoreCount != ShadowCompletionDecision.SampledCoreCount ||
        NewDecision.SampledBonusCount != ShadowCompletionDecision.SampledBonusCount ||
        (NewDecision.CanConclude() &&
         (!ShadowCompletionDecision.CanConclude() ||
          NewDecision.Classification.Interpretation !=
              ShadowCompletionDecision.Classification.Interpretation));

    ShadowCompletionDecision = NewDecision;
    bHasShadowCompletionDecision = true;

    if (!bMeaningfulChange)
    {
        return;
    }

    const TCHAR* Candidate = NewDecision.CanConclude()
        ? PedroShadowInterpretationName(NewDecision.Classification.Interpretation)
        : TEXT("None");

    UE_LOG(LogTemp, Display,
        TEXT("PEDRO_SHADOW status=%s sampledCore=%d totalCore=%d "
             "requiredCore=%d sampledBonus=%d acquired=%d ready=%s "
             "candidate=%s readOnly=yes actualLOS=no"),
        PedroShadowStatusName(NewDecision.Status),
        NewDecision.SampledCoreCount,
        NewDecision.CoreCount,
        NewDecision.RequiredCoreSamples,
        NewDecision.SampledBonusCount,
        NewDecision.AcquiredEvidenceCount,
        NewDecision.CanConclude() ? TEXT("yes") : TEXT("no"),
        Candidate);
}

void APedroEncounterDirector::QueueActiveCompletionEvaluation()
{
    if (CompletionPolicy != EPedroCompletionPolicy::EvidenceDriven ||
        bHasPresentedOutcome || bActiveCompletionQueued)
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        // Player exposure and acquired response can arrive in either order.
        // Defer both until the interaction's factual events are complete.
        bActiveCompletionQueued = true;
        PendingActiveCompletionTimer = World->GetTimerManager().SetTimerForNextTick(
            this, &APedroEncounterDirector::EvaluateActiveCompletion);
    }
    else
    {
        // Worldless test actors cannot resolve shared progression.
        EvaluateActiveCompletion();
    }
}

void APedroEncounterDirector::EvaluateActiveCompletion()
{
    bActiveCompletionQueued = false;

    if (CompletionPolicy != EPedroCompletionPolicy::EvidenceDriven ||
        bHasPresentedOutcome)
    {
        return;
    }

    const FPedroCompletionDecision Decision =
        PedroCompletionGate::Evaluate(ObservationState, CoreSampleFraction);
    if (!Decision.CanConclude())
    {
        return;
    }

    const FPedroPresentationPlan Plan = PedroActivePresentation::Prepare(
        Decision, GentleOutcomeActor, SeekingOutcomeActor, DisruptiveOutcomeActor);
    if (!Plan.IsReady())
    {
        UE_LOG(LogTemp, Display,
            TEXT("PEDRO_ACTIVE blocked=InvalidPresentation status=%d"),
            static_cast<int32>(Plan.Status));
        return;
    }

    if (!IsValid(EncounterDefinition))
    {
        UE_LOG(LogTemp, Display, TEXT("PEDRO_ACTIVE blocked=MissingEncounterDefinition"));
        return;
    }
    if (EncounterDefinition->GetEncounterId().IsNone())
    {
        UE_LOG(LogTemp, Display, TEXT("PEDRO_ACTIVE blocked=EmptyEncounterId"));
        return;
    }

    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        UE_LOG(LogTemp, Display, TEXT("PEDRO_ACTIVE blocked=MissingWorld"));
        return;
    }

    // Reject cross-world and stale actor references before touching progress.
    if (GentleOutcomeActor->GetWorld() != World ||
        SeekingOutcomeActor->GetWorld() != World ||
        DisruptiveOutcomeActor->GetWorld() != World)
    {
        UE_LOG(LogTemp, Display, TEXT("PEDRO_ACTIVE blocked=ActorWorldMismatch"));
        return;
    }

    UGameInstance* GameInstance = World->GetGameInstance();
    UEncounterProgressSubsystem* Progress = GameInstance
        ? GameInstance->GetSubsystem<UEncounterProgressSubsystem>()
        : nullptr;
    if (!IsValid(Progress))
    {
        UE_LOG(LogTemp, Display, TEXT("PEDRO_ACTIVE blocked=MissingProgressSubsystem"));
        return;
    }
    if (Progress->IsEncounterResolved(EncounterDefinition))
    {
        UE_LOG(LogTemp, Display, TEXT("PEDRO_ACTIVE blocked=AlreadyResolved"));
        return;
    }

    // Lock against reentrant events before ResolveEncounter broadcasts.
    // Roll back visuals and local state if that call refuses resolution.
    bHasPresentedOutcome = true;
    bHasFinalClassification = true;
    FinalClassification = Decision.Classification;
    SetOutcomeActorActive(GentleOutcomeActor,
        Plan.Interpretation == EPedroInterpretation::Gentle);
    SetOutcomeActorActive(SeekingOutcomeActor,
        Plan.Interpretation == EPedroInterpretation::Seeking);
    SetOutcomeActorActive(DisruptiveOutcomeActor,
        Plan.Interpretation == EPedroInterpretation::Disruptive);

    if (!Progress->ResolveEncounter(EncounterDefinition))
    {
        SetOutcomeActorActive(GentleOutcomeActor, false);
        SetOutcomeActorActive(SeekingOutcomeActor, false);
        SetOutcomeActorActive(DisruptiveOutcomeActor, false);
        bHasPresentedOutcome = false;
        bHasFinalClassification = false;
        FinalClassification = FPedroClassificationResult{};
        UE_LOG(LogTemp, Display, TEXT("PEDRO_ACTIVE blocked=ResolutionRejected"));
        return;
    }

    UE_LOG(LogTemp, Display,
        TEXT("PEDRO_OUTCOME policy=EvidenceDriven interpretation=%s "
             "sampledCore=%d requiredCore=%d resolved=yes actualLOS=no"),
        PedroShadowInterpretationName(Plan.Interpretation),
        Decision.SampledCoreCount, Decision.RequiredCoreSamples);
}

void APedroEncounterDirector::EvaluatePrototypeOutcome()
{
    bOutcomeEvaluationQueued = false;

    if (CompletionPolicy != EPedroCompletionPolicy::LegacyInteractionCount ||
        bHasPresentedOutcome ||
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
    if (CompletionPolicy != EPedroCompletionPolicy::LegacyInteractionCount ||
        bHasPresentedOutcome)
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
            ValidSource->OnOpportunityExposed.RemoveAll(this);
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
        World->GetTimerManager().ClearTimer(PendingShadowCompletionTimer);
        World->GetTimerManager().ClearTimer(PendingActiveCompletionTimer);
    }

    UnregisterObservationSources();
    InteractionComponent = nullptr;
    UniqueInteractedActors.Reset();
    bOutcomeEvaluationQueued = false;
    bShadowCompletionQueued = false;
    bActiveCompletionQueued = false;

    Super::EndPlay(EndPlayReason);
}
