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

void ASlimeEnemy::ResetForRoom(
	const FTransform &ResetTransform)
{
	if (MeleeAttackComponent)
	{
		MeleeAttackComponent->CancelAttack();
	}

	ShutdownControllerForRoomReset();

	UCharacterMovementComponent *Movement =
		GetCharacterMovement();

	if (Movement)
	{
		Movement->StopMovementImmediately();
		Movement->ClearAccumulatedForces();
	}

	SetActorTransform(
		ResetTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	if (Movement)
	{
		Movement->SetMovementMode(MOVE_Walking);
		Movement->StopMovementImmediately();
	}

	if (UHealthComponent *Health = GetHealthComponent())
	{
		Health->ResetHealth();
	}

	SpawnDefaultController();
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
		Movement->ClearAccumulatedForces();
		Movement->DisableMovement();
	}

	ShutdownControllerForRoomReset();
}

void ASlimeEnemy::ShutdownControllerForRoomReset()
{
	AController *OwningController =
		GetController();

	if (!IsValid(OwningController))
	{
		return;
	}

	// Unpossessing gives the AI controller a clean shutdown boundary:
	// its StateTree stops and perception/target state is cleared.
	OwningController->UnPossess();

	// The dead/reset slime must not leave a detached controller actor
	// behind in the world.
	OwningController->Destroy();
}
