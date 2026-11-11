#include "Encounters/Pedro/PedroObserverCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/SceneComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroObserverCharacterHasEyeTest,
    "AI_Gavin_Project.Pedro.ObserverCharacter.EyeComponent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroObserverCharacterHasEyeTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    APedroObserverCharacter* Observer =
        NewObject<APedroObserverCharacter>(GetTransientPackage());
    if (!TestNotNull(TEXT("Pedro observer character exists"), Observer))
    {
        return false;
    }
    USceneComponent* Eye = Observer->GetObserverEye();
    if (!TestNotNull(TEXT("Dedicated eye component exists"), Eye))
    {
        return false;
    }
    TestTrue(TEXT("Eye is attached to the character's root"),
        Eye->GetAttachParent() == Observer->GetRootComponent());
    TestTrue(TEXT("Eye has a positive vertical offset"),
        Eye->GetRelativeLocation().Z > 0.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPedroObserverCharacterUsesEyeTest,
    "AI_Gavin_Project.Pedro.ObserverCharacter.EyeViewPoint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroObserverCharacterUsesEyeTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    APedroObserverCharacter* Observer =
        NewObject<APedroObserverCharacter>(GetTransientPackage());
    if (!TestNotNull(TEXT("Pedro observer character exists"), Observer))
    {
        return false;
    }
    USceneComponent* Eye = Observer->GetObserverEye();
    if (!TestNotNull(TEXT("Dedicated eye component exists"), Eye))
    {
        return false;
    }

    const FVector NewEyeOffset(20.0, -10.0, 115.0);
    const FRotator NewEyeRotation(5.0, 65.0, 0.0);
    Eye->SetRelativeLocationAndRotation(NewEyeOffset, NewEyeRotation);

    FVector ActualLocation;
    FRotator ActualRotation;
    Observer->GetActorEyesViewPoint(ActualLocation, ActualRotation);

    TestTrue(TEXT("The viewpoint uses the current eye location"),
        ActualLocation.Equals(Eye->GetComponentLocation(), 0.01));
    TestTrue(TEXT("The viewpoint uses the current eye facing"),
        ActualRotation.Equals(Eye->GetComponentRotation(), 0.01));
    return true;
}
#endif
