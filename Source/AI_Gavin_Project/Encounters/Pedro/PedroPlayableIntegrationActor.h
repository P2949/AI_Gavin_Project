#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/HitResult.h"
#include "TimerManager.h"
#include "PedroPlayableIntegrationActor.generated.h"

class APawn;
class AController;
class APlayerController;
class APedroEncounterDirector;
class APedroObserverCharacter;
class APedroObserverAIController;
class APedroObserverWatchPoint;
class UMeleeAttackComponent;
class FDataValidationContext;

/**
 * Encounter-local wiring for the authored, autonomous Pedro level.
 * The Director owns knowledge and completion; the AI controller owns motion.
 * No tick, scanning for arbitrary actors, scoring, or movement logic here.
 */
UCLASS()
class AI_GAVIN_PROJECT_API APedroPlayableIntegrationActor : public AActor
{
    GENERATED_BODY()

public:
    APedroPlayableIntegrationActor();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Pedro|Playable")
    TObjectPtr<APedroObserverCharacter> Observer;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Pedro|Playable")
    TObjectPtr<APedroEncounterDirector> Director;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Pedro|Playable")
    TArray<TObjectPtr<APedroObserverWatchPoint>> WatchPoints;

    UFUNCTION(BlueprintPure, Category="Pedro|Playable")
    bool IsPlayableActive() const { return bActivated; }

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    static constexpr int32 MaxStartupRetries = 3;

    UFUNCTION()
    void OnObserverControllerChanged(APawn* Pawn, AController* Previous, AController* Current);

    UFUNCTION()
    void OnPlayerPawnChanged(APawn* Previous, APawn* Current);

    UFUNCTION()
    void OnPlayerMeleeHit(AActor* HitActor, const FHitResult& Hit, float AppliedDamage);

    void ScheduleStartupRetry();
    void RetryStartup();
    bool Activate();
    void Deactivate();
    void BindPlayer();
    void UnbindPlayer();
    bool ValidateBindings(FString& Reason) const;
    void LogBlocked(const TCHAR* Reason) const;

    UPROPERTY(Transient, VisibleInstanceOnly, Category="Pedro|Playable")
    bool bActivated = false;

    TWeakObjectPtr<APedroObserverAIController> BoundController;
    TWeakObjectPtr<APlayerController> BoundPlayerController;
    TWeakObjectPtr<APawn> BoundPlayer;
    TWeakObjectPtr<UMeleeAttackComponent> BoundMelee;
    FTimerHandle StartupRetryHandle;
    int32 StartupRetries = 0;
};
