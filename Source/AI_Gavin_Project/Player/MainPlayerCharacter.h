#pragma once

#include "CoreMinimal.h"
#include "Combat/Characters/CombatantCharacter.h"
#include "Combat/Attack/MeleeAttackTypes.h"
#include "MainPlayerCharacter.generated.h"

class UCameraComponent;
class UMeleeAttackComponent;
class USceneComponent;
class UInputAction;
struct FInputActionValue;

UCLASS()
class AI_GAVIN_PROJECT_API AMainPlayerCharacter : public ACombatantCharacter
{
	GENERATED_BODY()

public:
	AMainPlayerCharacter();

protected:
	// First-person camera.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// Reusable melee attack mechanics.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMeleeAttackComponent> MeleeAttackComponent;

	// Defines where and in which direction player melee attacks originate.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> MeleeAttackOrigin;

	// Movement input.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	// Camera/look input.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	// Jump input.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	// Primary attack input.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AttackAction;

	// Hold-to-block input.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> BlockAction;

	// Movement speed multiplier applied while blocking.
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Movement|Blocking",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlockingMovementSpeedMultiplier = 0.65f;

	virtual void BeginPlay() override;

	virtual void SetupPlayerInputComponent(
		UInputComponent *PlayerInputComponent) override;

private:
	void Move(const FInputActionValue &Value);
	void Look(const FInputActionValue &Value);

	void StartAttack();
	void StartBlocking();
	void StopBlocking();

	UFUNCTION()
	void HandleAttackStateChanged(
		EMeleeAttackState OldState,
		EMeleeAttackState NewState);

	UFUNCTION()
	void HandleBlockingChanged(bool bIsBlocking);

	bool bBlockInputHeld = false;
	float WalkSpeedBeforeBlocking = 0.0f;
};
