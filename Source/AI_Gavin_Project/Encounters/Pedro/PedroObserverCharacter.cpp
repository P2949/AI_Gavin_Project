#include "Encounters/Pedro/PedroObserverCharacter.h"

#include "Components/SceneComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

APedroObserverCharacter::APedroObserverCharacter()
{
    // The default CharacterMovementComponent can support navigation later.
    // This phase intentionally supplies no autonomous behaviour or ticking AI.
    PrimaryActorTick.bCanEverTick = false;
    AutoPossessPlayer = EAutoReceiveInput::Disabled;
    bUseControllerRotationYaw = false;

    // Do not let movement or controller facing change the test's deliberate
    // observer orientation without a separate, explicit follow/look policy.
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;

    ObserverEye = CreateDefaultSubobject<USceneComponent>(TEXT("ObserverEye"));
    ObserverEye->SetupAttachment(GetRootComponent());
    ObserverEye->SetRelativeLocation(FVector(0.0, 0.0, 64.0));
    ObserverEye->SetRelativeRotation(FRotator::ZeroRotator);
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
