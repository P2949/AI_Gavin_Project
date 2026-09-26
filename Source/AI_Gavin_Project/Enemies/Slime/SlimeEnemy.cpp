#include "Enemies/Slime/SlimeEnemy.h"

#include "Combat/Attack/MeleeAttackComponent.h"
#include "Combat/Attack/MeleeAttackTypes.h"
#include "Combat/Health/HealthComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"

ASlimeEnemy::ASlimeEnemy()
{
	PrimaryActorTick.bCanEverTick = false;

	// The slime faces the direction in which navigation moves it.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent *Movement =
		GetCharacterMovement();

	check(Movement);

	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate =
		FRotator(0.0f, 360.0f, 0.0f);
	Movement->MaxWalkSpeed = 300.0f;

	// Let the configured AI controller possess placed or spawned slimes.
	AutoPossessAI =
		EAutoPossessAI::PlacedInWorldOrSpawned;

	MeleeAttackOrigin =
		CreateDefaultSubobject<USceneComponent>(
			TEXT("MeleeAttackOrigin"));

	check(MeleeAttackOrigin);

	MeleeAttackOrigin->SetupAttachment(GetRootComponent());
	MeleeAttackOrigin->SetRelativeLocation(
		FVector(40.0f, 0.0f, 0.0f));

	MeleeAttackComponent =
		CreateDefaultSubobject<UMeleeAttackComponent>(
			TEXT("MeleeAttackComponent"));

	check(MeleeAttackComponent);

	MeleeAttackComponent->SetAttackOriginComponent(
		MeleeAttackOrigin);

	FMeleeAttackSettings AttackSettings =
		MeleeAttackComponent->GetDefaultAttackSettings();

	AttackSettings.Damage = 20.0f;
	AttackSettings.Range = 150.0f;
	AttackSettings.Radius = 45.0f;
	AttackSettings.WindupDuration = 0.35f;
	AttackSettings.RecoveryDuration = 0.65f;
	AttackSettings.KnockbackHorizontalVelocity = 500.0f;
	AttackSettings.KnockbackVerticalVelocity = 180.0f;

	MeleeAttackComponent->SetDefaultAttackSettings(
		AttackSettings);
}

void ASlimeEnemy::BeginPlay()
{
	Super::BeginPlay();

	if (UHealthComponent *Health = GetHealthComponent())
	{
		Health->OnDeath.AddDynamic(
			this,
			&ASlimeEnemy::HandleDeath);
	}
}

void ASlimeEnemy::HandleDeath(AActor *DamageCauser)
{
	static_cast<void>(DamageCauser);

	if (MeleeAttackComponent)
	{
		MeleeAttackComponent->CancelAttack();
	}

	if (UCharacterMovementComponent *Movement =
			GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	if (AController *OwningController = GetController())
	{
		OwningController->UnPossess();
	}
}
