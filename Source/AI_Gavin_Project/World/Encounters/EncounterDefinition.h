#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "EncounterDefinition.generated.h"

class FDataValidationContext;
class UWorld;

/**
 * Stable metadata describing one main encounter.
 *
 * The definition deliberately contains only information that the shared
 * world/progression layer needs. Encounter-specific gameplay remains owned
 * by the encounter itself.
 */
UCLASS(BlueprintType)
class AI_GAVIN_PROJECT_API UEncounterDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(
        FDataValidationContext &Context) const override;
#endif

    FName GetEncounterId() const
    {
        return EncounterId;
    }

    const TSoftObjectPtr<UWorld> &GetEncounterMap() const
    {
        return EncounterMap;
    }

protected:
    /**
     * Stable identifier used by shared progression state.
     *
     * This should remain stable even if the encounter's display name,
     * map name, or presentation changes later.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Encounter")
    FName EncounterId;

    /**
     * Map containing the encounter.
     *
     * A soft reference keeps encounter maps from becoming hard-loaded
     * dependencies of the shared progression layer.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Encounter")
    TSoftObjectPtr<UWorld> EncounterMap;
};
