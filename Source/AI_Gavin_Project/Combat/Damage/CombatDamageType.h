#pragma once

#include "CoreMinimal.h"
#include "Combat/Damage/DamageCategory.h"
#include "GameFramework/DamageType.h"
#include "CombatDamageType.generated.h"

UCLASS(Abstract)
class AI_GAVIN_PROJECT_API UCombatDamageType : public UDamageType
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Damage")
	EDamageCategory GetDamageCategory() const
	{
		return DamageCategory;
	}

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	EDamageCategory DamageCategory = EDamageCategory::Physical;
};
