#include "Encounters/Pedro/PedroDirectObservation.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include <limits>

namespace
{
constexpr float Range = 1000.0f;
constexpr float HalfAngle = 60.0f;
const FVector Eye(0.0, 0.0, 0.0);
const FVector Forward(1.0, 0.0, 0.0);

EPedroDirectObservationStatus Check(const FVector& Position,
    float MaxRange = Range, float HalfFov = HalfAngle)
{
    return PedroDirectObservation::CheckGeometry(
        Eye, Forward, Position, MaxRange, HalfFov);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPedroDirectAcceptsVisibleTest,
    "AI_Gavin_Project.Pedro.DirectObservation.Geometry.Visible",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroDirectAcceptsVisibleTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    TestTrue(TEXT("Ahead and inside range is geometrically visible"),
        Check(FVector(300, 0, 0)) == EPedroDirectObservationStatus::Visible);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPedroDirectRejectsDistanceTest,
    "AI_Gavin_Project.Pedro.DirectObservation.Geometry.Range",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroDirectRejectsDistanceTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    TestTrue(TEXT("Outside range is rejected"),
        Check(FVector(1001, 0, 0)) == EPedroDirectObservationStatus::TooFar);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPedroDirectRejectsAngleTest,
    "AI_Gavin_Project.Pedro.DirectObservation.Geometry.Angle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroDirectRejectsAngleTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    TestTrue(TEXT("Behind observer is rejected"),
        Check(FVector(-100, 0, 0)) ==
            EPedroDirectObservationStatus::OutsideViewCone);
    TestTrue(TEXT("Off-axis is rejected"),
        Check(FVector(100, 500, 0)) ==
            EPedroDirectObservationStatus::OutsideViewCone);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPedroDirectBoundaryTest,
    "AI_Gavin_Project.Pedro.DirectObservation.Geometry.Boundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroDirectBoundaryTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    TestTrue(TEXT("Exactly maximum distance is accepted"),
        Check(FVector(1000, 0, 0)) == EPedroDirectObservationStatus::Visible);
    TestTrue(TEXT("Event at observer eye is accepted by geometry"),
        Check(FVector::ZeroVector) == EPedroDirectObservationStatus::Visible);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPedroDirectFullCircleTest,
    "AI_Gavin_Project.Pedro.DirectObservation.Geometry.FullCircle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroDirectFullCircleTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    TestTrue(TEXT("180-degree half-angle allows behind"),
        Check(FVector(-100, 0, 0), Range, 180.0f) ==
            EPedroDirectObservationStatus::Visible);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPedroDirectInvalidSettingsTest,
    "AI_Gavin_Project.Pedro.DirectObservation.Geometry.InvalidSettings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroDirectInvalidSettingsTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    TestTrue(TEXT("Zero distance rejects"),
        Check(FVector(100, 0, 0), 0.0f) ==
            EPedroDirectObservationStatus::InvalidSettings);
    TestTrue(TEXT("Oversized angle rejects"),
        Check(FVector(100, 0, 0), Range, 181.0f) ==
            EPedroDirectObservationStatus::InvalidSettings);
    TestTrue(TEXT("Negative angle rejects"),
        Check(FVector(100, 0, 0), Range, -1.0f) ==
            EPedroDirectObservationStatus::InvalidSettings);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPedroDirectNonFiniteTest,
    "AI_Gavin_Project.Pedro.DirectObservation.Geometry.NonFinite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroDirectNonFiniteTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const float Infinity = std::numeric_limits<float>::infinity();
    const float NaN = std::numeric_limits<float>::quiet_NaN();
    TestTrue(TEXT("Infinite distance rejects"),
        Check(FVector(100, 0, 0), Infinity) ==
            EPedroDirectObservationStatus::InvalidSettings);
    TestTrue(TEXT("NaN view angle rejects"),
        Check(FVector(100, 0, 0), Range, NaN) ==
            EPedroDirectObservationStatus::InvalidSettings);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPedroDirectZeroForwardTest,
    "AI_Gavin_Project.Pedro.DirectObservation.Geometry.ZeroForward",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPedroDirectZeroForwardTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    TestTrue(TEXT("Zero facing vector fails closed"),
        PedroDirectObservation::CheckGeometry(
            Eye, FVector::ZeroVector, FVector(100, 0, 0), Range, HalfAngle) ==
            EPedroDirectObservationStatus::InvalidGeometry);
    return true;
}
#endif
