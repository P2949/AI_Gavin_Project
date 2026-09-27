#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "StateTreeMeleeAttackTask.generated.h"

class AActor;
class UMeleeAttackComponent;

USTRUCT()
struct AI_GAVIN_PROJECT_API FStateTreeMeleeAttackTaskInstanceData
{
	GENERATED_BODY()

	// Controlled pawn supplied by the AI StateTree schema's Actor context.
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> Actor = nullptr;

	// Actor this execution of the melee task intends to attack.
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<AActor> TargetActor = nullptr;

	// Component used by this execution of the task.
	UPROPERTY(Transient)
	TObjectPtr<UMeleeAttackComponent> AttackComponent = nullptr;

	// Prevents this task from cancelling an attack it did not start.
	UPROPERTY(Transient)
	bool bStartedAttack = false;
};

USTRUCT(
	meta = (DisplayName = "Melee Attack",
			Category = "AI|Combat"))
struct AI_GAVIN_PROJECT_API FStateTreeMeleeAttackTask
	: public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType =
		FStateTreeMeleeAttackTaskInstanceData;

	FStateTreeMeleeAttackTask();

	virtual const UStruct *GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext &Context,
		const FStateTreeTransitionResult &Transition) const override;

	virtual EStateTreeRunStatus Tick(
		FStateTreeExecutionContext &Context,
		float DeltaTime) const override;

	virtual void ExitState(
		FStateTreeExecutionContext &Context,
		const FStateTreeTransitionResult &Transition) const override;
};
