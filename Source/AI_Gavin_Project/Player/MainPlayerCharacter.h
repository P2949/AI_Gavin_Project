#pragma once

#include "CoreMinimal.h"
#include "Combat/Characters/CombatantCharacter.h"
#include "Combat/Attack/MeleeAttackTypes.h"
#include "World/Rooms/RoomResettable.h"
#include "MainPlayerCharacter.generated.h"

class UCameraComponent;
class UMeleeAttackComponent;
class UPlayerLocomotionComponent;
class USceneComponent;
class UInputAction;
struct FInputActionValue;

UCLASS()
class AI_GAVIN_PROJECT_API AMainPlayerCharacter
	: public ACombatantCharacter,
	  public IRoomResettable
{
	GENERATED_BODY()

public:
	AMainPlayerCharacter();

	virtual void ResetForRoom(
		const FTransform &ResetTransform) override;

	UFUNCTION(BlueprintPure, Category = "Player|Locomotion")
	UPlayerLocomotionComponent *GetLocomotionComponent() const
	{
		return LocomotionComponent;
	}

protected:
	// First-person camera.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// Owns player locomotion tuning and movement-speed policy.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPlayerLocomotionComponent> LocomotionComponent;

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

	// Hold-to-sprint input.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	// Primary attack input.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AttackAction;

	// Hold-to-block input.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> BlockAction;

	virtual void BeginPlay() override;

	virtual void SetupPlayerInputComponent(
		UInputComponent *PlayerInputComponent) override;

private:
	void Move(const FInputActionValue &Value);
	void Look(const FInputActionValue &Value);

	UFUNCTION()
	void HandleDeath(AActor *DamageCauser);

	void StartAttack();

	void StartSprinting();
	void StopSprinting();

	void StartBlocking();
	void StopBlocking();

	UFUNCTION()
	void HandleAttackStateChanged(
		EMeleeAttackState OldState,
		EMeleeAttackState NewState);

	UFUNCTION()
	void HandleBlockingChanged(bool bIsBlocking);

	bool bBlockInputHeld = false;
};
