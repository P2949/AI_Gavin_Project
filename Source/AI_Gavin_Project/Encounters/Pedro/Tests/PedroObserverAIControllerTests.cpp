#include "Encounters/Pedro/PedroObserverAIController.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Encounters/Pedro/PedroObserverCharacter.h"
#include "Misc/AutomationTest.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "UObject/Package.h"

#define PEDRO_CONTROLLER_UNIT(Name) \
    IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPedroAwareness##Name##Test, \
        "AI_Gavin_Project.Pedro.Awareness." #Name, \
        EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
    bool FPedroAwareness##Name##Test::RunTest(const FString& /*Parameters*/)

PEDRO_CONTROLLER_UNIT(ControllerConfiguration)
{
    auto* Controller = NewObject<APedroObserverAIController>(GetTransientPackage());
    if (!TestNotNull(TEXT("controller"), Controller)) return false;
    const auto* Sight = Controller->GetSightSettings();
    return TestTrue(TEXT("sight defaults"), Sight && Controller->GetAIPerceptionComponent() &&
        Sight->SightRadius == 1500.0f && Sight->LoseSightRadius == 1800.0f &&
        Sight->PeripheralVisionAngleDegrees == 75.0f && Sight->GetMaxAge() == 5.0f &&
        Sight->AutoSuccessRangeFromLastSeenLocation == FAISystem::InvalidRange &&
        Sight->DetectionByAffiliation.bDetectNeutrals && Sight->DetectionByAffiliation.bDetectEnemies &&
        Sight->DetectionByAffiliation.bDetectFriendlies);
}
PEDRO_CONTROLLER_UNIT(NonPlayerFiltering)
{
    auto* Pawn = NewObject<APedroObserverCharacter>(GetTransientPackage());
    return TestTrue(TEXT("uncontrolled pawn and null rejected"),
        !APedroObserverAIController::IsSupportedPlayer(Pawn) && !APedroObserverAIController::IsSupportedPlayer(nullptr));
}
PEDRO_CONTROLLER_UNIT(MissingObserver)
{
    auto* Controller = NewObject<APedroObserverAIController>(GetTransientPackage());
    return TestTrue(TEXT("no active awareness without possession"), !Controller->IsAwarenessRunning() &&
        !Controller->GetAwarenessState().bHasLastKnownPlayerLocation &&
        Controller->GetAwarenessState().Status == EPedroAwarenessStatus::Unaware);
}
PEDRO_CONTROLLER_UNIT(DefaultCompatibility)
{
    const auto* Observer = GetDefault<APedroObserverCharacter>();
    return TestTrue(TEXT("new controller is not default"), Observer->AIControllerClass != APedroObserverAIController::StaticClass() &&
        Observer->AutoPossessPlayer == EAutoReceiveInput::Disabled && !Observer->bUseControllerRotationYaw);
}
#undef PEDRO_CONTROLLER_UNIT
#endif
