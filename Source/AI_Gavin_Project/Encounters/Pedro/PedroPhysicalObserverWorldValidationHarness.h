#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "PedroPhysicalObserverWorldValidationHarness.generated.h"

/**
 * Opt-in, UNSAVED Empty Level PIE-only fixture for the real Pedro Character.
 * It demonstrates actor translation/rotation affecting direct acquisition
 * without changing existing maps, production logic, or evidence provenance.
 */
UCLASS(NotBlueprintable)
class AI_GAVIN_PROJECT_API APedroPhysicalObserverWorldValidationHarness : public AActor
{
    GENERATED_BODY()

public:
    APedroPhysicalObserverWorldValidationHarness();

    /** Explicitly enable on ONE actor in a new, unsaved Untitled PIE world. */
    UPROPERTY(EditInstanceOnly, Category = "Pedro|Validation")
    bool bRunValidationInPIE = false;

protected:
    virtual void BeginPlay() override;

private:
    enum class ECase : uint8
    {
        Visible,
        TurnAway,
        TurnBack,
        MoveOutOfRange,
        MoveIntoRange,
        CoverBlocks,
        NoRetroactive,
        EyeFollowsPose,
        Count
    };

    static const TCHAR* CaseName(ECase Case);
    bool RunCase(ECase Case, int32 CaseIndex);
};
