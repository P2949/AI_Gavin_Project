#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class AController;
class AActor;
class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnHealthChangedSignature,
	float,
	OldHealth,
	float,
	NewHealth,
	float,
	Delta
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnDeathSignature,
	AActor*,
	DamageCauser
);

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class AI_GAVIN_PROJECT_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetCurrentHealth() const
	{
		return CurrentHealth;
	}

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const
	{
		return MaxHealth;
	}
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthNormalized() const
	{
		return MaxHealth > 0.0f
			? CurrentHealth / MaxHealth
			: 0.0f;
	}

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const
	{
		return CurrentHealth <= 0.0f;
	}
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Health")
	void ResetHealth();

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDeathSignature OnDeath;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health", meta=(ClampMin="1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Health")
	float CurrentHealth = 0.0f;
	UFUNCTION()
	void HandleTakeAnyDamage(
		AActor* DamagedActor,
		float Damage,
		const UDamageType* DamageType,
		AController* InstigatedBy,
		AActor* DamageCauser
	);

	void SetCurrentHealth(
		float NewHealth,
		AActor* DamageCauser = nullptr
	);
};
