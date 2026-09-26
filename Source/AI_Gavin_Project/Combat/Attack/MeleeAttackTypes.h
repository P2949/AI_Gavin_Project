#pragma once

#include "CoreMinimal.h"
#include "MeleeAttackTypes.generated.h"

class UDamageType;

UENUM(BlueprintType)
enum class EMeleeAttackState : uint8
{
    Ready UMETA(DisplayName = "Ready"),
    Windup UMETA(DisplayName = "Windup"),
    Recovery UMETA(DisplayName = "Recovery")
};

USTRUCT(BlueprintType)
struct AI_GAVIN_PROJECT_API FMeleeAttackSettings
{
    GENERATED_BODY()

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Attack",
        meta = (ClampMin = "0.0"))
    float Damage = 25.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Attack",
        meta = (ClampMin = "0.0"))
    float Range = 175.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Attack",
        meta = (ClampMin = "0.0"))
    float Radius = 35.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Attack|Timing",
        meta = (ClampMin = "0.0"))
    float WindupDuration = 0.15f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Attack|Timing",
        meta = (ClampMin = "0.0"))
    float RecoveryDuration = 0.35f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Attack")
    TSubclassOf<UDamageType> DamageTypeClass;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Attack")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;
};
