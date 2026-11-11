#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "PedroObserverCharacter.generated.h"

class USceneComponent;

/**
 * Pedro's physical point-of-view host.
 *
 * This is deliberately NOT an AI brain or a perception memory component.
 * The existing APedroEncounterDirector queries this actor via
 * GetActorEyesViewPoint for each confirmed behaviour event.
 *
 * No new evidence or acquisition policy is introduced by this class.
 */
UCLASS(Blueprintable)
class AI_GAVIN_PROJECT_API APedroObserverCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    APedroObserverCharacter();

    /** The current world-space eye point and facing, even after movement. */
    virtual void GetActorEyesViewPoint(
        FVector& OutLocation,
        FRotator& OutRotation) const override;

    /** Child Blueprints can adjust the eye point without changing C++. */
    UFUNCTION(BlueprintPure, Category = "Pedro|Observer")
    USceneComponent* GetObserverEye() const { return ObserverEye.Get(); }

protected:
    /** A component-local eye offset; the owning Character drives its pose. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedro|Observer")
    TObjectPtr<USceneComponent> ObserverEye;
};
