#pragma once

#include "CoreMinimal.h"
#include "DamageCategory.generated.h"

UENUM(BlueprintType)
enum class EDamageCategory : uint8
{
	Physical UMETA(DisplayName = "Physical"),
	Elemental UMETA(DisplayName = "Elemental")
};
