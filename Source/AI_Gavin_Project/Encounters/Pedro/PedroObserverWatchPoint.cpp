#include "Encounters/Pedro/PedroObserverWatchPoint.h"
#include "Encounters/Pedro/PedroContextPolicy.h"
#include "Components/SceneComponent.h"
APedroObserverWatchPoint::APedroObserverWatchPoint()
{
    PrimaryActorTick.bCanEverTick=false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("WatchPointRoot")));
    SetActorEnableCollision(false);
}
bool APedroObserverWatchPoint::IsUsable() const
{return bEnabled&&!WatchPointId.IsNone()&&PedroContextPolicy::ValidPoint(GetActorLocation())&&!GetActorRotation().ContainsNaN()&&FMath::IsFinite(ObservationRadius)&&ObservationRadius>0&&FMath::IsFinite(BasePriority)&&BasePriority>=0&&BasePriority<=1;}
