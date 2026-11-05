#pragma once

#include "CoreMinimal.h"

#include "PedroOpportunityState.generated.h"

/** Core opportunities gate the future encounter conclusion; bonus ones do not. */
UENUM(BlueprintType)
enum class EPedroOpportunityTier : uint8
{
    Core,
    Bonus
};

/** Authored on the Pedro Director. IDs must be unique and not NAME_None. */
USTRUCT(BlueprintType)
struct FPedroOpportunityDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedro|Opportunity")
    FName OpportunityId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedro|Opportunity")
    EPedroOpportunityTier Tier = EPedroOpportunityTier::Core;
};

/**
 * The two independent conditions needed for an opportunity to be sampled.
 * bPlayerExposed is a world fact; bPedroAcquiredResponse is observer knowledge.
 * Neither flag should imply Pedro witnessed the player's exposure.
 */
USTRUCT(BlueprintType)
struct FPedroOpportunityProgressEntry
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Opportunity")
    FName OpportunityId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Opportunity")
    EPedroOpportunityTier Tier = EPedroOpportunityTier::Core;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Opportunity")
    bool bPlayerExposed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Opportunity")
    bool bPedroAcquiredResponse = false;

    bool IsSampled() const
    {
        return bPlayerExposed && bPedroAcquiredResponse;
    }
};

/**
 * Provisional opportunity bookkeeping. It never awards traits or resolves
 * an encounter. Unknown opportunities do not affect progress.
 *
 * A core opportunity is sampled once after meaningful exposure AND at least
 * one accepted piece of Pedro's knowledge about the resulting response.
 * The two signals may arrive in either order. Extra actions don't increase
 * the number of sampled opportunities. Bonus opportunities never gate progress.
 */
USTRUCT(BlueprintType)
struct FPedroOpportunityState
{
    GENERATED_BODY()

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly,
        Category = "Pedro|Opportunities")
    TArray<FPedroOpportunityProgressEntry> Entries;

    /** Atomic setup: invalid/duplicate IDs leave the previous state unchanged. */
    bool Configure(const TArray<FPedroOpportunityDefinition>& Definitions);

    /** Confirm the player really encountered the choice; no observation claim. */
    bool MarkPlayerExposed(FName OpportunityId);

    /** Called only after FPedroObservationState accepted a knowledge record. */
    bool NoteAcquiredResponse(FName OpportunityId);

    const FPedroOpportunityProgressEntry* Find(FName OpportunityId) const;

    int32 GetCoreCount() const;
    int32 GetSampledCoreCount() const;
    int32 GetBonusCount() const;
    int32 GetSampledBonusCount() const;

    /** Rounds upward: 75% of 3 requires 3; 75% of 4 requires 3. */
    int32 GetRequiredCoreSamples(float Fraction = 0.75f) const;

    /** Empty core configuration never counts as sufficient information. */
    bool HasSufficientCoreSamples(float Fraction = 0.75f) const;

    void Reset();

private:
    FPedroOpportunityProgressEntry* FindMutable(FName OpportunityId);
};
