#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatantCharacter.generated.h"

class UDefenseComponent;
class UHealthComponent;

UCLASS(Abstract)
class AI_GAVIN_PROJECT_API ACombatantCharacter : public ACharacter
{
	GENERATED_BODY()

public:
        ACombatantCharacter();

        virtual float TakeDamage(
                float DamageAmount,
                const FDamageEvent& DamageEvent,
                AController* EventInstigator,
                AActor* DamageCauser
        ) override;

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	UHealthComponent* GetHealthComponent() const
	{
		return HealthComponent;
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Defense")
	UDefenseComponent* GetDefenseComponent() const
	{
		return DefenseComponent;
	}

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDefenseComponent> DefenseComponent;
};
