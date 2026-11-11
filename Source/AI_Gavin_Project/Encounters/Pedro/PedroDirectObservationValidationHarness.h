#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PedroDirectObservationValidationHarness.generated.h"

/**
 * Temporary, opt-in, unsaved-map PIE fixture for eight direct sightline cases.
 * It does not create assets or modify the production/ChoiceTest/ActiveTest maps.
 */
UCLASS(NotBlueprintable)
class AI_GAVIN_PROJECT_API APedroDirectObservationValidationHarness : public AActor
{
    GENERATED_BODY()

public:
    APedroDirectObservationValidationHarness();

    UPROPERTY(EditInstanceOnly, Category="Pedro|Validation")
    bool bRunValidationInPIE = false;

protected:
    virtual void BeginPlay() override;

private:
    enum class ECase : uint8
    {
        Visible,
        Occluded,
        TooFar,
        Behind,
        MissingObserver,
        ConflictingPolicies,
        PrototypeCompatibility,
        InvalidSettings,
        Count
    };

    static const TCHAR* Name(ECase Case);
    bool RunCase(ECase Case);
};
