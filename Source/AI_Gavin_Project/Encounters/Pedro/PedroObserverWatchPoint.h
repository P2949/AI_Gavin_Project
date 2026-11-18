#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PedroObserverWatchPoint.generated.h"

/** Authored position only. No remote sensing or evidence. */
UCLASS()
class AI_GAVIN_PROJECT_API APedroObserverWatchPoint : public AActor
{
    GENERATED_BODY()
public:
    APedroObserverWatchPoint();
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Pedro|Watch") FName WatchPointId;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Pedro|Watch") float ObservationRadius=1200;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Pedro|Watch") float BasePriority=.5;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Pedro|Watch") bool bEnabled=true;
    bool IsUsable() const;
};
