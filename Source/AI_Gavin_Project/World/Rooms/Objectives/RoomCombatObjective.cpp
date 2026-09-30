#include "World/Rooms/Objectives/RoomCombatObjective.h"

#include "AI_Gavin_Project.h"

#include "Combat/Health/HealthComponent.h"
#include "TimerManager.h"
#include "World/Rooms/RoomLifecycleActor.h"
#include "World/Rooms/RoomResettable.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

ARoomCombatObjective::ARoomCombatObjective()
{
    PrimaryActorTick.bCanEverTick = false;
}

#if WITH_EDITOR
EDataValidationResult ARoomCombatObjective::IsDataValid(
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
                    "RoomCombatObjective",
                    "MissingRoomLifecycle",
                    "Room combat objective '{0}' requires a valid "
                    "RoomLifecycleActor."),
                FText::FromString(GetName())));

        Result = CombineDataValidationResults(
            Result,
            EDataValidationResult::Invalid);
    }

    if (RequiredTargets.IsEmpty())
    {
        Context.AddError(
            FText::Format(
                NSLOCTEXT(
                    "RoomCombatObjective",
                    "MissingRequiredTargets",
                    "Room combat objective '{0}' requires at least "
                    "one combat target."),
                FText::FromString(GetName())));

        Result = CombineDataValidationResults(
            Result,
            EDataValidationResult::Invalid);
    }

    TSet<AActor *> SeenTargets;

    for (AActor *Target : RequiredTargets)
    {
        if (!IsValid(Target))
        {
            Context.AddError(
                FText::Format(
                    NSLOCTEXT(
                        "RoomCombatObjective",
                        "InvalidRequiredTarget",
                        "Room combat objective '{0}' contains an "
                        "invalid required target."),
                    FText::FromString(GetName())));

            Result = CombineDataValidationResults(
                Result,
                EDataValidationResult::Invalid);
            continue;
        }

        if (SeenTargets.Contains(Target))
        {
            Context.AddWarning(
                FText::Format(
                    NSLOCTEXT(
                        "RoomCombatObjective",
                        "DuplicateRequiredTarget",
                        "Room combat objective '{0}' contains "
                        "duplicate target '{1}'."),
                    FText::FromString(GetName()),
                    FText::FromString(Target->GetName())));
            continue;
        }

        SeenTargets.Add(Target);

        if (!Target->GetClass()->ImplementsInterface(
                URoomResettable::StaticClass()))
        {
            Context.AddError(
                FText::Format(
                    NSLOCTEXT(
                        "RoomCombatObjective",
                        "TargetNotResettable",
                        "Room combat objective '{0}' cannot use "
                        "target '{1}' because it does not implement "
                        "IRoomResettable."),
                    FText::FromString(GetName()),
                    FText::FromString(Target->GetName())));

            Result = CombineDataValidationResults(
                Result,
                EDataValidationResult::Invalid);
        }

        if (!Target->FindComponentByClass<UHealthComponent>())
        {
            Context.AddError(
                FText::Format(
                    NSLOCTEXT(
                        "RoomCombatObjective",
                        "TargetMissingHealthComponent",
                        "Room combat objective '{0}' cannot use "
                        "target '{1}' because it has no "
                        "HealthComponent."),
                    FText::FromString(GetName()),
                    FText::FromString(Target->GetName())));

            Result = CombineDataValidationResults(
                Result,
                EDataValidationResult::Invalid);
        }
    }

    return Result;
}
#endif

void ARoomCombatObjective::BeginPlay()
{
    Super::BeginPlay();

    if (!InitializeObjective())
    {
        return;
    }

    RoomLifecycle->OnRoomStateChanged.AddDynamic(
        this,
        &ARoomCombatObjective::HandleRoomStateChanged);

    for (const TWeakObjectPtr<UHealthComponent> &HealthEntry :
         RequiredHealthComponents)
    {
        if (UHealthComponent *Health = HealthEntry.Get())
        {
            Health->OnDeath.AddDynamic(
                this,
                &ARoomCombatObjective::HandleRequiredTargetDeath);
        }
    }

    bInitialized = true;

    if (RoomLifecycle->IsRoomActive())
    {
        QueueCompletionEvaluation();
    }
}

void ARoomCombatObjective::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    UnbindObjective();

    Super::EndPlay(EndPlayReason);
}

bool ARoomCombatObjective::InitializeObjective()
{
    if (!IsValid(RoomLifecycle))
    {
        UE_LOG(
            LogAI_Gavin_Project,
            Error,
            TEXT(
                "RoomCombatObjective '%s' requires a valid "
                "RoomLifecycleActor."),
            *GetName());

        return false;
    }

    if (RequiredTargets.IsEmpty())
    {
        UE_LOG(
            LogAI_Gavin_Project,
            Error,
            TEXT(
                "RoomCombatObjective '%s' requires at least one "
                "combat target."),
            *GetName());

        return false;
    }

    TSet<AActor *> SeenTargets;

    TArray<AActor *> ValidatedTargets;
    TArray<UHealthComponent *> ValidatedHealthComponents;

    ValidatedTargets.Reserve(RequiredTargets.Num());
    ValidatedHealthComponents.Reserve(RequiredTargets.Num());

    for (AActor *Target : RequiredTargets)
    {
        if (!IsValid(Target))
        {
            UE_LOG(
                LogAI_Gavin_Project,
                Error,
                TEXT(
                    "RoomCombatObjective '%s' contains an invalid "
                    "required target."),
                *GetName());

            return false;
        }

        if (SeenTargets.Contains(Target))
        {
            UE_LOG(
                LogAI_Gavin_Project,
                Warning,
                TEXT(
                    "RoomCombatObjective '%s' contains duplicate "
                    "target '%s'; duplicate ignored."),
                *GetName(),
                *Target->GetName());

            continue;
        }

        SeenTargets.Add(Target);

        if (!Target->GetClass()->ImplementsInterface(
                URoomResettable::StaticClass()))
        {
            UE_LOG(
                LogAI_Gavin_Project,
                Error,
                TEXT(
                    "RoomCombatObjective '%s' cannot use target "
                    "'%s' because it does not implement "
                    "IRoomResettable."),
                *GetName(),
                *Target->GetName());

            return false;
        }

        UHealthComponent *Health =
            Target->FindComponentByClass<UHealthComponent>();

        if (!Health)
        {
            UE_LOG(
                LogAI_Gavin_Project,
                Error,
                TEXT(
                    "RoomCombatObjective '%s' cannot use target "
                    "'%s' because it has no HealthComponent."),
                *GetName(),
                *Target->GetName());

            return false;
        }

        ValidatedTargets.Add(Target);
        ValidatedHealthComponents.Add(Health);
    }

    if (ValidatedTargets.IsEmpty())
    {
        UE_LOG(
            LogAI_Gavin_Project,
            Error,
            TEXT(
                "RoomCombatObjective '%s' has no unique valid "
                "combat targets."),
            *GetName());

        return false;
    }

    for (AActor *Target : ValidatedTargets)
    {
        if (!RoomLifecycle->RegisterResetParticipant(Target))
        {
            UE_LOG(
                LogAI_Gavin_Project,
                Error,
                TEXT(
                    "RoomCombatObjective '%s' failed to register "
                    "target '%s' with room '%s'."),
                *GetName(),
                *Target->GetName(),
                *RoomLifecycle->GetName());

            return false;
        }
    }

    RequiredHealthComponents.Reset();
    RequiredHealthComponents.Reserve(
        ValidatedHealthComponents.Num());

    for (UHealthComponent *Health :
         ValidatedHealthComponents)
    {
        RequiredHealthComponents.Add(Health);
    }

    return true;
}

void ARoomCombatObjective::UnbindObjective()
{
    if (IsValid(RoomLifecycle))
    {
        RoomLifecycle->OnRoomStateChanged.RemoveDynamic(
            this,
            &ARoomCombatObjective::HandleRoomStateChanged);
    }

    for (const TWeakObjectPtr<UHealthComponent> &HealthEntry :
         RequiredHealthComponents)
    {
        if (UHealthComponent *Health = HealthEntry.Get())
        {
            Health->OnDeath.RemoveDynamic(
                this,
                &ARoomCombatObjective::HandleRequiredTargetDeath);
        }
    }

    RequiredHealthComponents.Reset();
    bInitialized = false;
    bCompletionEvaluationPending = false;
}

void ARoomCombatObjective::QueueCompletionEvaluation()
{
    if (!bInitialized ||
        bCompletionEvaluationPending ||
        !IsValid(RoomLifecycle) ||
        !RoomLifecycle->IsRoomActive())
    {
        return;
    }

    bCompletionEvaluationPending = true;

    GetWorldTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateUObject(
            this,
            &ARoomCombatObjective::
                HandleDeferredCompletionEvaluation));
}

void ARoomCombatObjective::
    HandleDeferredCompletionEvaluation()
{
    bCompletionEvaluationPending = false;
    EvaluateCompletion();
}

void ARoomCombatObjective::EvaluateCompletion()
{
    if (!bInitialized ||
        !IsValid(RoomLifecycle) ||
        !RoomLifecycle->IsRoomActive())
    {
        return;
    }

    for (const TWeakObjectPtr<UHealthComponent> &HealthEntry :
         RequiredHealthComponents)
    {
        UHealthComponent *Health = HealthEntry.Get();

        if (!IsValid(Health))
        {
            UE_LOG(
                LogAI_Gavin_Project,
                Error,
                TEXT(
                    "RoomCombatObjective '%s' lost a required "
                    "HealthComponent before completion."),
                *GetName());

            return;
        }

        if (!Health->IsDead())
        {
            return;
        }
    }

    RoomLifecycle->CompleteRoom();
}

void ARoomCombatObjective::HandleRequiredTargetDeath(
    AActor *DamageCauser)
{
    static_cast<void>(DamageCauser);

    EvaluateCompletion();
}

void ARoomCombatObjective::HandleRoomStateChanged(
    ERoomLifecycleState OldState,
    ERoomLifecycleState NewState)
{
    static_cast<void>(OldState);

    if (NewState == ERoomLifecycleState::Active)
    {
        QueueCompletionEvaluation();
    }
}
