#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/Encounters/EncounterDefinition.h"
#include "Encounters/Pedro/PedroObservationTypes.h"
#include "PedroActiveWorldValidationHarness.generated.h"

class APedroEncounterDirector;
class UPedroObservationSourceComponent;
class UEncounterProgressSubsystem;

/** A transient definition with a unique encounter ID for PIE-only validation. */
UCLASS()
class AI_GAVIN_PROJECT_API UPedroWorldValidationEncounterDefinition
    : public UEncounterDefinition
{
    GENERATED_BODY()

public:
    void SetValidationId(FName InId) { EncounterId = InId; }
};

/**
 * Place ONE instance in a NEW, UNSAVED empty level, set bRunValidationInPIE,
 * and start PIE. All encounter actors and definitions are generated at runtime.
 * NEVER place or enable this fixture in a production map.
 *
 * The harness exercises a real PIE UWorld and its GameInstance progress
 * subsystem, and prints machine-checkable PEDRO_WORLD_TEST records.
 * It intentionally does not simulate player movement, return-to-hub travel,
 * or actual observer line of sight.
 */
UCLASS(NotBlueprintable)
class AI_GAVIN_PROJECT_API APedroActiveWorldValidationHarness : public AActor
{
    GENERATED_BODY()

public:
    APedroActiveWorldValidationHarness();

    UPROPERTY(EditAnywhere, Category="Pedro|Validation")
    bool bRunValidationInPIE = false;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    enum class ECase : uint8
    {
        Seeking,
        Gentle,
        Disruptive,
        Reentrant,
        MissingOutcome,
        AlreadyResolved,
        InsufficientSamples,
        EmptyEncounterId,
        Count
    };

    void StartNextCase();
    void WaitOneTick();
    void VerifyCurrentCase();
    void TearDownCurrentCase();
    void AssertCondition(bool bCondition, const TCHAR* Description);
    void EmitResponse(FName OpportunityId, EPedroEvidenceKind Kind);

    UFUNCTION()
    void OnEncounterResolved(UEncounterDefinition* ResolvedDefinition);

    const TCHAR* CaseName() const;
    bool IsSuccessCase() const;
    EPedroEvidenceKind CaseEvidenceKind() const;

    UPROPERTY(Transient)
    TObjectPtr<UEncounterProgressSubsystem> Progress;

    UPROPERTY(Transient)
    TObjectPtr<APedroEncounterDirector> Director;

    UPROPERTY(Transient)
    TObjectPtr<UPedroWorldValidationEncounterDefinition> Definition;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> SpawnedActors;

    UPROPERTY(Transient)
    TObjectPtr<UPedroObservationSourceComponent> FirstSource;

    UPROPERTY(Transient)
    TObjectPtr<AActor> GentleActor;

    UPROPERTY(Transient)
    TObjectPtr<AActor> SeekingActor;

    UPROPERTY(Transient)
    TObjectPtr<AActor> DisruptiveActor;

    int32 CaseIndex = 0;
    int32 PassedCases = 0;
    int32 FailedCases = 0;
    int32 CurrentFailures = 0;
    int32 ResolutionBroadcasts = 0;
    int32 BroadcastsBeforeEvents = 0;
    int32 ResolvedCountBeforeEvents = 0;
    bool bCallbackObservedResolvedState = true;
};
