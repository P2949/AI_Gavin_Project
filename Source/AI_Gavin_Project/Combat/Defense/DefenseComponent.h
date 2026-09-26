#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DefenseComponent.generated.h"

class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
        FOnDefenseBlockingChangedSignature,
        bool,
        bIsBlocking
);

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class AI_GAVIN_PROJECT_API UDefenseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDefenseComponent();

	UFUNCTION(BlueprintPure, Category = "Combat|Defense")
	float CalculateDamageAfterMitigation(
		float IncomingDamage,
		const UDamageType* DamageType
	) const;

	UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
	void SetBlocking(bool bNewBlocking);

	UFUNCTION(BlueprintPure, Category = "Combat|Defense")
	bool IsBlocking() const
	{
		return bIsBlocking;
	}

	UPROPERTY(BlueprintAssignable, Category = "Combat|Defense")
	FOnDefenseBlockingChangedSignature OnBlockingChanged;

protected:
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Defense|Damage Multipliers",
		meta=(ClampMin="0.0")
	)
	float PhysicalDamageMultiplier = 1.0f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Defense|Damage Multipliers",
		meta=(ClampMin="0.0")
	)
	float ElementalDamageMultiplier = 1.0f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Defense|Blocking",
		meta=(ClampMin="0.0")
	)
	float BlockingPhysicalDamageMultiplier = 0.25f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Defense|Blocking",
		meta=(ClampMin="0.0")
	)
	float BlockingElementalDamageMultiplier = 0.50f;

	UPROPERTY(
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Defense|Blocking"
	)
	bool bIsBlocking = false;
};
