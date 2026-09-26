#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Attack/MeleeAttackTypes.h"
#include "Engine/HitResult.h"
#include "MeleeAttackComponent.generated.h"

class AActor;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnMeleeAttackStateChangedSignature,
    EMeleeAttackState,
    OldState,
    EMeleeAttackState,
    NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnMeleeAttackHitSignature,
    AActor *,
    HitActor,
    const FHitResult &,
    HitResult,
    float,
    AppliedDamage);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class AI_GAVIN_PROJECT_API UMeleeAttackComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMeleeAttackComponent();

    UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
    bool TryStartAttack();

    UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
    bool CancelAttack();

    UFUNCTION(BlueprintPure, Category = "Combat|Attack")
    FMeleeAttackSettings GetActiveAttackSettings() const
    {
        return ActiveAttackSettings;
    }

    UFUNCTION(BlueprintPure, Category = "Combat|Attack")
    EMeleeAttackState GetAttackState() const
    {
        return AttackState;
    }

    UFUNCTION(BlueprintPure, Category = "Combat|Attack")
    bool CanAttack() const
    {
        return AttackState == EMeleeAttackState::Ready;
    }

    UFUNCTION(BlueprintPure, Category = "Combat|Attack")
    bool IsAttackInProgress() const
    {
        return AttackState != EMeleeAttackState::Ready;
    }

    UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
    void SetAttackOriginComponent(USceneComponent *NewAttackOrigin);

    UFUNCTION(BlueprintPure, Category = "Combat|Attack")
    USceneComponent *GetAttackOriginComponent() const
    {
        return AttackOriginComponent;
    }

    UPROPERTY(BlueprintAssignable, Category = "Combat|Attack")
    FOnMeleeAttackStateChangedSignature OnAttackStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Attack")
    FOnMeleeAttackHitSignature OnAttackHit;

protected:
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Attack")
    FMeleeAttackSettings DefaultAttackSettings;

    UPROPERTY(
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Attack")
    FMeleeAttackSettings ActiveAttackSettings;

    UPROPERTY(
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Attack")
    EMeleeAttackState AttackState = EMeleeAttackState::Ready;

    UPROPERTY(
        Transient,
        BlueprintReadOnly,
        Category = "Attack")
    TObjectPtr<USceneComponent> AttackOriginComponent;

private:
    void SetAttackState(EMeleeAttackState NewState);
    void HandleWindupComplete();
    void PerformAttack();
    void HandleRecoveryComplete();

    FTimerHandle WindupTimerHandle;
    FTimerHandle RecoveryTimerHandle;
};
