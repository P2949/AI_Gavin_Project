#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "EnemyAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UStateTreeAIComponent;

UCLASS(Abstract)
class AI_GAVIN_PROJECT_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();

	UFUNCTION(BlueprintPure, Category = "AI|Targeting")
	AActor *GetCurrentTargetActor() const
	{
		return CurrentTargetActor.Get();
	}

	UFUNCTION(BlueprintPure, Category = "AI|Targeting")
	bool IsCurrentTargetPerceived() const
	{
		return CurrentTargetActor.IsValid() &&
			   bIsCurrentTargetPerceived;
	}

	UFUNCTION(BlueprintPure, Category = "AI|Targeting")
	bool HasLastKnownTargetLocation() const
	{
		return bHasLastKnownTargetLocation;
	}

	UFUNCTION(BlueprintPure, Category = "AI|Targeting")
	FVector GetLastKnownTargetLocation() const
	{
		return LastKnownTargetLocation;
	}

protected:
	virtual void OnPossess(APawn *InPawn) override;
	virtual void OnUnPossess() override;

	// Determines whether a perceived actor is eligible to become
	// this controller's target.
	virtual bool IsValidTargetActor(
		const AActor *Actor) const;

	// Called when a valid candidate is perceived while another
	// target is already selected. The baseline enemy keeps its
	// current target.
	virtual bool ShouldReplaceCurrentTarget(
		const AActor *CandidateActor) const;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "AI|StateTree")
	TObjectPtr<UStateTreeAIComponent> StateTreeAIComponent;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "AI|Perception")
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

private:
	UFUNCTION()
	void HandleTargetPerceptionUpdated(
		AActor *Actor,
		FAIStimulus Stimulus);

	void UpdateCurrentTargetPerceptionState();
	void ResetTargetState();

	UPROPERTY(
		Transient,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "AI|Targeting",
		meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<AActor> CurrentTargetActor;

	UPROPERTY(
		Transient,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "AI|Targeting",
		meta = (AllowPrivateAccess = "true"))
	FVector LastKnownTargetLocation = FVector::ZeroVector;

	UPROPERTY(
		Transient,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "AI|Targeting",
		meta = (AllowPrivateAccess = "true"))
	bool bHasLastKnownTargetLocation = false;

	UPROPERTY(
		Transient,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "AI|Targeting",
		meta = (AllowPrivateAccess = "true"))
	bool bIsCurrentTargetPerceived = false;
};
