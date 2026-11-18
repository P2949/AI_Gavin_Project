#include "Encounters/Pedro/PedroAwarenessState.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Encounters/Pedro/PedroObserverCharacter.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include <limits>

#define PEDRO_AWARENESS_UNIT(Name) \
    IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPedroAwareness##Name##Test, \
        "AI_Gavin_Project.Pedro.Awareness." #Name, \
        EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
    bool FPedroAwareness##Name##Test::RunTest(const FString& /*Parameters*/)

namespace PedroAwarenessUnit
{
APawn* MakePawn() { return NewObject<APedroObserverCharacter>(GetTransientPackage()); }
bool Empty(const FPedroAwarenessState& S)
{
    return S.Status == EPedroAwarenessStatus::Unaware && !S.ObservedPlayer.IsValid() &&
        !S.bHasLastKnownPlayerLocation && S.LastKnownPlayerLocation.IsZero() && S.LastSuccessfulSightingTime == 0.0;
}
}

PEDRO_AWARENESS_UNIT(InitialState)
{
    return TestTrue(TEXT("empty"), PedroAwarenessUnit::Empty(FPedroAwarenessState{}));
}
PEDRO_AWARENESS_UNIT(FirstSighting)
{
    FPedroAwarenessState S;
    APawn* P = PedroAwarenessUnit::MakePawn();
    const FVector At(123,456,789);
    return TestTrue(TEXT("accepted"), S.AcceptSighting(P,At,1.0)) &&
        TestTrue(TEXT("record"), S.Status == EPedroAwarenessStatus::Perceived && S.ObservedPlayer.Get() == P &&
            S.bHasLastKnownPlayerLocation && S.LastKnownPlayerLocation == At && S.LastSuccessfulSightingTime == 1.0);
}
PEDRO_AWARENESS_UNIT(RefreshSighting)
{
    FPedroAwarenessState S;
    APawn* P = PedroAwarenessUnit::MakePawn();
    return TestTrue(TEXT("updates"), S.AcceptSighting(P,FVector::ZeroVector,1) &&
        S.AcceptSighting(P,FVector(20,30,40),2) && S.LastKnownPlayerLocation == FVector(20,30,40) &&
        S.LastSuccessfulSightingTime == 2 && !S.AcceptSighting(P,FVector(99,99,99),1) &&
        S.LastKnownPlayerLocation == FVector(20,30,40));
}
PEDRO_AWARENESS_UNIT(LoseSight)
{
    FPedroAwarenessState S;
    S.AcceptSighting(PedroAwarenessUnit::MakePawn(),FVector(10,20,30),2);
    S.LoseSight();
    return TestTrue(TEXT("memory retained"), S.Status == EPedroAwarenessStatus::LostSight &&
        S.bHasLastKnownPlayerLocation && S.LastKnownPlayerLocation == FVector(10,20,30) && S.LastSuccessfulSightingTime == 2);
}
PEDRO_AWARENESS_UNIT(IgnoreFailedStimulusLocation)
{
    FPedroAwarenessState S;
    APawn* P = PedroAwarenessUnit::MakePawn();
    S.AcceptSighting(P,FVector(10,20,30),2);
    // Loss has no location argument; repeated losses cannot update memory.
    S.LoseSight(); S.LoseSight();
    return TestTrue(TEXT("loss cannot update memory"), S.Status == EPedroAwarenessStatus::LostSight &&
        S.ObservedPlayer.Get() == P && S.LastKnownPlayerLocation == FVector(10,20,30) && S.LastSuccessfulSightingTime == 2);
}
PEDRO_AWARENESS_UNIT(Reacquire)
{
    FPedroAwarenessState S;
    APawn* P = PedroAwarenessUnit::MakePawn();
    S.AcceptSighting(P,FVector(10,20,30),2); S.LoseSight();
    return TestTrue(TEXT("new sight"), S.AcceptSighting(P,FVector(30,40,50),3) &&
        S.Status == EPedroAwarenessStatus::Perceived && S.LastKnownPlayerLocation == FVector(30,40,50));
}
PEDRO_AWARENESS_UNIT(LossBeforeAcquisition)
{
    FPedroAwarenessState S; S.LoseSight();
    return TestTrue(TEXT("no fabricated memory"), PedroAwarenessUnit::Empty(S));
}
PEDRO_AWARENESS_UNIT(InvalidLocation)
{
    FPedroAwarenessState S;
    APawn* P = PedroAwarenessUnit::MakePawn();
    FVector Bad = FVector::ZeroVector; Bad.X = std::numeric_limits<double>::quiet_NaN();
    const bool bRejected = !S.AcceptSighting(P,Bad,1) &&
        !S.AcceptSighting(P,FAISystem::InvalidLocation,1) &&
        !S.AcceptSighting(P,FVector::ZeroVector,std::numeric_limits<double>::infinity()) &&
        !S.AcceptSighting(P,FVector::ZeroVector,-1);
    return TestTrue(TEXT("invalid inputs rejected"), bRejected && PedroAwarenessUnit::Empty(S));
}
PEDRO_AWARENESS_UNIT(Reset)
{
    FPedroAwarenessState S; S.AcceptSighting(PedroAwarenessUnit::MakePawn(),FVector(1,2,3),4);
    S.LoseSight(); S.Reset();
    return TestTrue(TEXT("reset"), PedroAwarenessUnit::Empty(S));
}
PEDRO_AWARENESS_UNIT(InvalidTarget)
{
    FPedroAwarenessState S;
    return TestTrue(TEXT("invalid target"), !S.AcceptSighting(nullptr,FVector(2,3,4),5) && PedroAwarenessUnit::Empty(S));
}
#undef PEDRO_AWARENESS_UNIT
#endif
