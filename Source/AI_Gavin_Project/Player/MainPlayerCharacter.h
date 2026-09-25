#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MainPlayerCharacter.generated.h"

class UCameraComponent;
class UInputAction;
struct FInputActionValue;

UCLASS()
class AI_GAVIN_PROJECT_API AMainPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMainPlayerCharacter();

protected:
	// First-person camera.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// Movement input.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	// Camera/look input.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	// Jump input.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	virtual void SetupPlayerInputComponent(
		UInputComponent* PlayerInputComponent
	) override;

private:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
};
