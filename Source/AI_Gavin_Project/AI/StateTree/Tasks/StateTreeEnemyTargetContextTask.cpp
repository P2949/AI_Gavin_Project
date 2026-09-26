#include "AI/StateTree/Tasks/StateTreeEnemyTargetContextTask.h"

#include "AI/Controllers/EnemyAIController.h"
#include "AIController.h"
#include "StateTreeExecutionContext.h"

FStateTreeEnemyTargetContextTask::
	FStateTreeEnemyTargetContextTask()
{
	// This task continuously exposes controller state rather than
	// performing an action that should complete the tree/state.
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = false;

#if WITH_EDITORONLY_DATA
	bConsideredForCompletion = false;
	bCanEditConsideredForCompletion = false;
#endif
}

EStateTreeRunStatus
FStateTreeEnemyTargetContextTask::EnterState(
	FStateTreeExecutionContext &Context,
	const FStateTreeTransitionResult &Transition) const
{
	static_cast<void>(Transition);

	UpdateTargetContext(Context);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus
FStateTreeEnemyTargetContextTask::Tick(
	FStateTreeExecutionContext &Context,
	float DeltaTime) const
{
	static_cast<void>(DeltaTime);

	UpdateTargetContext(Context);

	return EStateTreeRunStatus::Running;
}

void FStateTreeEnemyTargetContextTask::UpdateTargetContext(
	FStateTreeExecutionContext &Context) const
{
	FInstanceDataType &InstanceData =
		Context.GetInstanceData(*this);

	const AEnemyAIController *EnemyController =
		Cast<AEnemyAIController>(
			InstanceData.AIController);

	if (!EnemyController)
	{
		InstanceData.TargetActor = nullptr;
		InstanceData.bHasTarget = false;
		InstanceData.bIsTargetCurrentlyPerceived = false;
		InstanceData.LastKnownTargetLocation =
			FVector::ZeroVector;
		InstanceData.bHasLastKnownTargetLocation = false;

		return;
	}

	AActor *TargetActor =
		EnemyController->GetCurrentTargetActor();

	InstanceData.TargetActor = TargetActor;
	InstanceData.bHasTarget = IsValid(TargetActor);

	InstanceData.bIsTargetCurrentlyPerceived =
		EnemyController->IsCurrentTargetPerceived();

	InstanceData.LastKnownTargetLocation =
		EnemyController->GetLastKnownTargetLocation();

	InstanceData.bHasLastKnownTargetLocation =
		EnemyController->HasLastKnownTargetLocation();
}
