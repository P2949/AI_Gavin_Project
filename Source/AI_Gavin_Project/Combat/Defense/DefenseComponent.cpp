#include "Combat/Defense/DefenseComponent.h"

#include "Combat/Damage/CombatDamageType.h"

UDefenseComponent::UDefenseComponent()
{
        PrimaryComponentTick.bCanEverTick = false;
        PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UDefenseComponent::SetBlocking(bool bNewBlocking)
{
	if (bIsBlocking == bNewBlocking)
	{
		return;
	}

	bIsBlocking = bNewBlocking;

	OnBlockingChanged.Broadcast(bIsBlocking);
}

float UDefenseComponent::CalculateDamageAfterMitigation(
        float IncomingDamage,
        const UDamageType* DamageType
) const
{
        if (IncomingDamage <= 0.0f)
        {
                return IncomingDamage;
        }

        const UCombatDamageType* CombatDamageType =
                Cast<UCombatDamageType>(DamageType);

        if (!CombatDamageType)
        {
                return IncomingDamage;
        }

        float DamageMultiplier = 1.0f;

        switch (CombatDamageType->GetDamageCategory())
        {
        case EDamageCategory::Physical:
                DamageMultiplier = PhysicalDamageMultiplier;

                if (bIsBlocking)
                {
                        DamageMultiplier *=
                                BlockingPhysicalDamageMultiplier;
                }

                break;

        case EDamageCategory::Elemental:
                DamageMultiplier = ElementalDamageMultiplier;

                if (bIsBlocking)
                {
                        DamageMultiplier *=
                                BlockingElementalDamageMultiplier;
                }

                break;

        default:
                return IncomingDamage;
        }

        return IncomingDamage * DamageMultiplier;
}
