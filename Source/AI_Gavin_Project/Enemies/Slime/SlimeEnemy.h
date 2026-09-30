#pragma once

#include "CoreMinimal.h"
#include "Combat/Characters/CombatantCharacter.h"
#include "World/Rooms/RoomResettable.h"
#include "SlimeEnemy.generated.h"

class AActor;
class UMeleeAttackComponent;
class USceneComponent;

UCLASS()
class AI_GAVIN_PROJECT_API ASlimeEnemy
	: public ACombatantCharacter,
	  public IRoomResettable
{
	GENERATED_BODY()

public:
	ASlimeEnemy();

	virtual void ResetForRoom_Implementation(
		const FTransform &ResetTransform) override;

protected:
	virtual void BeginPlay() override;

	// Reusable melee combat mechanics.
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Components")
	TObjectPtr<UMeleeAttackComponent> MeleeAttackComponent;

	// Defines where and in which direction the slime attacks.
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Components")
	TObjectPtr<USceneComponent> MeleeAttackOrigin;

private:
	UFUNCTION()
	void HandleDeath(AActor *DamageCauser);

	void ShutdownControllerForRoomReset();
};
