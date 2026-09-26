#include "Combat/Health/HealthComponent.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxHealth = FMath::Max(MaxHealth, 1.0f);
	CurrentHealth = MaxHealth;

	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(
			this,
			&UHealthComponent::HandleTakeAnyDamage
		);
	}
}

void UHealthComponent::HandleTakeAnyDamage(
	AActor* DamagedActor,
	float Damage,
	const UDamageType* DamageType,
	AController* InstigatedBy,
	AActor* DamageCauser
)
{
	if (Damage <= 0.0f || IsDead())
	{
		return;
	}

	SetCurrentHealth(
		CurrentHealth - Damage,
		DamageCauser
	);
}

void UHealthComponent::Heal(float Amount)
{
	if (Amount <= 0.0f || IsDead())
	{
		return;
	}

	SetCurrentHealth(
		CurrentHealth + Amount
	);
}

void UHealthComponent::ResetHealth()
{
	SetCurrentHealth(MaxHealth);
}

void UHealthComponent::SetCurrentHealth(
	float NewHealth,
	AActor* DamageCauser
)
{
	const float OldHealth = CurrentHealth;

	CurrentHealth = FMath::Clamp(
		NewHealth,
		0.0f,
		MaxHealth
	);

	if (FMath::IsNearlyEqual(OldHealth, CurrentHealth))
	{
		return;
	}

	OnHealthChanged.Broadcast(
		OldHealth,
		CurrentHealth,
		CurrentHealth - OldHealth
	);

	if (OldHealth > 0.0f && IsDead())
	{
		OnDeath.Broadcast(DamageCauser);
	}
}
