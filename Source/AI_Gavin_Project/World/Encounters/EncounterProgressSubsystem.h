#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "EncounterProgressSubsystem.generated.h"

class UEncounterDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnEncounterResolvedSignature,
    UEncounterDefinition *,
    Encounter);

/**
 * Owns encounter-resolution state for the current game session.
 *
 * Its GameInstance lifetime allows progression to survive normal map travel
 * without making encounter actors, maps, or the player responsible for
 * cross-encounter state.
 *
 * This subsystem deliberately does not own save-game persistence,
 * rewards, inventory, player state, dialogue, or encounter-local state.
 */
UCLASS()
class AI_GAVIN_PROJECT_API UEncounterProgressSubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /**
     * Marks an encounter resolved for this run.
     *
     * Returns true only when this call newly resolves the encounter.
     * Invalid encounters and already-resolved encounters return false.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "Encounter|Progress")
    bool ResolveEncounter(
        UEncounterDefinition *Encounter);

    UFUNCTION(
        BlueprintPure,
        Category = "Encounter|Progress")
    bool IsEncounterResolved(
        UEncounterDefinition *Encounter) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Encounter|Progress")
    int32 GetResolvedEncounterCount() const
    {
        return ResolvedEncounterIds.Num();
    }

    /**
     * Clears session encounter progression.
     *
     * This represents starting a new run, not resetting an individual room
     * or retrying an encounter.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "Encounter|Progress")
    void ResetRun();

    /**
     * Broadcast once when an encounter becomes resolved.
     *
     * Repeated attempts to resolve the same encounter do not broadcast.
     */
    UPROPERTY(
        BlueprintAssignable,
        Category = "Encounter|Progress")
    FOnEncounterResolvedSignature OnEncounterResolved;

private:
    /**
     * Stable encounter IDs resolved during the current GameInstance.
     */
    UPROPERTY(Transient)
    TSet<FName> ResolvedEncounterIds;
};
