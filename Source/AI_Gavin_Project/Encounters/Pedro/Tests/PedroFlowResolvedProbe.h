#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PedroFlowResolvedProbe.generated.h"

class UEncounterDefinition;

/** Development-only observer of the real shared GameInstance progress delegate.
 *  Does not call ResolveEncounter or alter gameplay state. */
UCLASS(Transient)
class AI_GAVIN_PROJECT_API UPedroFlowResolvedProbe : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION()
    void OnEncounterResolved(UEncounterDefinition* Encounter);

    UPROPERTY(Transient)
    int32 NotificationCount = 0;

    UPROPERTY(Transient)
    FName LastEncounterId = NAME_None;
};
