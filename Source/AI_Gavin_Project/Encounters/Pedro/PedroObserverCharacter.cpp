#include "Encounters/Pedro/PedroObserverCharacter.h"

#include "Components/SceneComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

APedroObserverCharacter::APedroObserverCharacter()
{
    // The AI controller owns autonomous behaviour; the character needs no actor tick.
    PrimaryActorTick.bCanEverTick = false;
    AutoPossessPlayer = EAutoReceiveInput::Disabled;
    SetCanBeDamaged(false);
    bUseControllerRotationYaw = false;

    // The attention policy owns physical yaw, not ordinary movement or controller facing.
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;

    ObserverEye = CreateDefaultSubobject<USceneComponent>(TEXT("ObserverEye"));
    ObserverEye->SetupAttachment(GetRootComponent());
    ObserverEye->SetRelativeLocation(FVector(0.0, 0.0, 64.0));
    ObserverEye->SetRelativeRotation(FRotator::ZeroRotator);
}

float APedroObserverCharacter::TakeDamage(float DamageAmount,
    const FDamageEvent& DamageEvent, AController* EventInstigator,
    AActor* DamageCauser)
{
    // Real melee sweeps still deliver UMeleeAttackComponent::OnAttackHit.
    // Damage cannot kill or make Pedro an ordinary combat opponent.
    return 0.0f;
}

void APedroObserverCharacter::GetActorEyesViewPoint(
    FVector& OutLocation,
    FRotator& OutRotation) const
{
    if (!IsValid(ObserverEye))
    {
        Super::GetActorEyesViewPoint(OutLocation, OutRotation);
        return;
    }

    OutLocation = ObserverEye->GetComponentLocation();
    OutRotation = ObserverEye->GetComponentRotation();
}
