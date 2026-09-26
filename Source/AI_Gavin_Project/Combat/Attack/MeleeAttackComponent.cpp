#include "Combat/Attack/MeleeAttackComponent.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Combat/Damage/PhysicalDamageType.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UMeleeAttackComponent::UMeleeAttackComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    PrimaryComponentTick.bStartWithTickEnabled = false;

    DefaultAttackSettings.DamageTypeClass =
        UPhysicalDamageType::StaticClass();
}

bool UMeleeAttackComponent::TryStartAttack()
{
    if (!CanAttack())
    {
        return false;
    }

    UWorld *World = GetWorld();

    if (!World)
    {
        return false;
    }

    ActiveAttackSettings = DefaultAttackSettings;

    SetAttackState(EMeleeAttackState::Windup);

    if (AttackState != EMeleeAttackState::Windup)
    {
        return true;
    }

    if (ActiveAttackSettings.WindupDuration <= 0.0f)
    {
        HandleWindupComplete();
        return true;
    }

    World->GetTimerManager().SetTimer(
        WindupTimerHandle,
        this,
        &UMeleeAttackComponent::HandleWindupComplete,
        ActiveAttackSettings.WindupDuration,
        false);

    return true;
}

bool UMeleeAttackComponent::CancelAttack()
{
    if (!IsAttackInProgress())
    {
        return false;
    }

    if (UWorld *World = GetWorld())
    {
        FTimerManager &TimerManager = World->GetTimerManager();

        TimerManager.ClearTimer(WindupTimerHandle);
        TimerManager.ClearTimer(RecoveryTimerHandle);
    }

    SetAttackState(EMeleeAttackState::Ready);

    return true;
}

void UMeleeAttackComponent::SetAttackOriginComponent(
    USceneComponent *NewAttackOrigin)
{
    AttackOriginComponent = NewAttackOrigin;
}

void UMeleeAttackComponent::SetAttackState(
    EMeleeAttackState NewState)
{
    if (AttackState == NewState)
    {
        return;
    }

    const EMeleeAttackState OldState = AttackState;
    AttackState = NewState;

    OnAttackStateChanged.Broadcast(
        OldState,
        AttackState);
}

void UMeleeAttackComponent::HandleWindupComplete()
{
    if (AttackState != EMeleeAttackState::Windup)
    {
        return;
    }

    PerformAttack();

    if (AttackState != EMeleeAttackState::Windup)
    {
        return;
    }

    SetAttackState(EMeleeAttackState::Recovery);

    if (AttackState != EMeleeAttackState::Recovery)
    {
        return;
    }

    if (ActiveAttackSettings.RecoveryDuration <= 0.0f)
    {
        HandleRecoveryComplete();
        return;
    }

    UWorld *World = GetWorld();

    if (!World)
    {
        SetAttackState(EMeleeAttackState::Ready);
        return;
    }

    World->GetTimerManager().SetTimer(
        RecoveryTimerHandle,
        this,
        &UMeleeAttackComponent::HandleRecoveryComplete,
        ActiveAttackSettings.RecoveryDuration,
        false);
}

void UMeleeAttackComponent::PerformAttack()
{
    UWorld *World = GetWorld();
    AActor *Owner = GetOwner();

    if (!World || !Owner)
    {
        return;
    }

    const FVector Start =
        AttackOriginComponent
            ? AttackOriginComponent->GetComponentLocation()
            : Owner->GetActorLocation();

    const FVector Forward =
        AttackOriginComponent
            ? AttackOriginComponent->GetForwardVector()
            : Owner->GetActorForwardVector();

    const FVector End =
        Start + Forward.GetSafeNormal() * ActiveAttackSettings.Range;

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Owner);

    FHitResult HitResult;

    const FCollisionShape AttackShape =
        FCollisionShape::MakeSphere(
            ActiveAttackSettings.Radius);

    const bool bHit =
        World->SweepSingleByChannel(
            HitResult,
            Start,
            End,
            FQuat::Identity,
            ActiveAttackSettings.TraceChannel,
            AttackShape,
            QueryParams);

    if (!bHit)
    {
        return;
    }

    AActor *HitActor = HitResult.GetActor();

    if (!HitActor)
    {
        return;
    }

    AController *EventInstigator = Owner->GetInstigatorController();

    if (const APawn *OwnerPawn = Cast<APawn>(Owner))
    {
        EventInstigator = OwnerPawn->GetController();
    }

    const FVector HitDirection = Forward.GetSafeNormal();

    const float AppliedDamage =
        UGameplayStatics::ApplyPointDamage(
            HitActor,
            ActiveAttackSettings.Damage,
            HitDirection,
            HitResult,
            EventInstigator,
            Owner,
            ActiveAttackSettings.DamageTypeClass);

    OnAttackHit.Broadcast(
        HitActor,
        HitResult,
        AppliedDamage);
}

void UMeleeAttackComponent::HandleRecoveryComplete()
{
    if (AttackState != EMeleeAttackState::Recovery)
    {
        return;
    }

    SetAttackState(EMeleeAttackState::Ready);
}
