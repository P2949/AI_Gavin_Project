#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "StateTreeEnemyTargetContextTask.generated.h"

class AActor;
class AAIController;

USTRUCT()
struct AI_GAVIN_PROJECT_API FStateTreeEnemyTargetContextTaskInstanceData
{
	GENERATED_BODY()

	// Supplied automatically by the AI StateTree schema.
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	// Currently selected logical target.
	UPROPERTY(VisibleAnywhere, Category = Output)
	TObjectPtr<AActor> TargetActor = nullptr;

	// Whether a valid target is currently selected.
	UPROPERTY(VisibleAnywhere, Category = Output)
	bool bHasTarget = false;

	// Whether at least one active perception stimulus currently
	// supports the selected target.
	UPROPERTY(VisibleAnywhere, Category = Output)
	bool bIsTargetCurrentlyPerceived = false;

	// Most recent known location of the target while actively seen.
	UPROPERTY(VisibleAnywhere, Category = Output)
	FVector LastKnownTargetLocation = FVector::ZeroVector;

	// Whether LastKnownTargetLocation contains valid remembered data.
	UPROPERTY(VisibleAnywhere, Category = Output)
	bool bHasLastKnownTargetLocation = false;
};

USTRUCT(
	meta = (DisplayName = "Enemy Target Context",
			Category = "AI|Targeting"))
struct AI_GAVIN_PROJECT_API FStateTreeEnemyTargetContextTask
	: public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType =
		FStateTreeEnemyTargetContextTaskInstanceData;

	FStateTreeEnemyTargetContextTask();

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

private:
	void UpdateTargetContext(
		FStateTreeExecutionContext &Context) const;
};
