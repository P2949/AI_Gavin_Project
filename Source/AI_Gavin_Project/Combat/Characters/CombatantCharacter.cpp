#include "Combat/Characters/CombatantCharacter.h"

#include "Combat/Defense/DefenseComponent.h"
#include "Combat/Health/HealthComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/DamageType.h"

ACombatantCharacter::ACombatantCharacter()
{
	HealthComponent =
		CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

	DefenseComponent =
		CreateDefaultSubobject<UDefenseComponent>(TEXT("DefenseComponent"));
}

float ACombatantCharacter::TakeDamage(
	float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser
)
{
	float ResolvedDamage = DamageAmount;
	if (DefenseComponent)
	{
		const UDamageType* DamageType =
			DamageEvent.DamageTypeClass
				? DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>()
				: nullptr;

		ResolvedDamage =
			DefenseComponent->CalculateDamageAfterMitigation(
				DamageAmount,
				DamageType
			);
	}

	return Super::TakeDamage(
		ResolvedDamage,
		DamageEvent,
		EventInstigator,
		DamageCauser
	);
}
