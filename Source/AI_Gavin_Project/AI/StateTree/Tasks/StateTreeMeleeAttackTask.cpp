#include "AI/StateTree/Tasks/StateTreeMeleeAttackTask.h"

#include "Combat/Attack/MeleeAttackComponent.h"
#include "GameFramework/Actor.h"
#include "StateTreeExecutionContext.h"

FStateTreeMeleeAttackTask::FStateTreeMeleeAttackTask()
{
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FStateTreeMeleeAttackTask::EnterState(
	FStateTreeExecutionContext &Context,
	const FStateTreeTransitionResult &Transition) const
{
	static_cast<void>(Transition);

	FInstanceDataType &InstanceData =
		Context.GetInstanceData(*this);

	InstanceData.AttackComponent = nullptr;
	InstanceData.bStartedAttack = false;

	if (!IsValid(InstanceData.Actor) ||
		!IsValid(InstanceData.TargetActor))
	{
		return EStateTreeRunStatus::Failed;
	}

	UMeleeAttackComponent *AttackComponent =
		InstanceData.Actor->FindComponentByClass<UMeleeAttackComponent>();

	if (!AttackComponent)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.AttackComponent = AttackComponent;

	if (!AttackComponent->TryStartAttackAtTarget(
			InstanceData.TargetActor))
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.bStartedAttack = true;

	// Zero-duration attacks may complete entirely during TryStartAttack().
	if (!AttackComponent->IsAttackInProgress())
	{
		InstanceData.AttackComponent = nullptr;
		InstanceData.bStartedAttack = false;

		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeMeleeAttackTask::Tick(
	FStateTreeExecutionContext &Context,
	float DeltaTime) const
{
	static_cast<void>(DeltaTime);

	FInstanceDataType &InstanceData =
		Context.GetInstanceData(*this);

	UMeleeAttackComponent *AttackComponent =
		InstanceData.AttackComponent;

	if (!IsValid(AttackComponent) ||
		!InstanceData.bStartedAttack)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (AttackComponent->IsAttackInProgress())
	{
		return EStateTreeRunStatus::Running;
	}

	// This task no longer owns an active attack once it has completed.
	InstanceData.AttackComponent = nullptr;
	InstanceData.bStartedAttack = false;

	return EStateTreeRunStatus::Succeeded;
}

void FStateTreeMeleeAttackTask::ExitState(
	FStateTreeExecutionContext &Context,
	const FStateTreeTransitionResult &Transition) const
{
	static_cast<void>(Transition);

	FInstanceDataType &InstanceData =
		Context.GetInstanceData(*this);

	UMeleeAttackComponent *AttackComponent =
		InstanceData.AttackComponent;

	if (InstanceData.bStartedAttack &&
		IsValid(AttackComponent) &&
		AttackComponent->IsAttackInProgress())
	{
		AttackComponent->CancelAttack();
	}

	InstanceData.AttackComponent = nullptr;
	InstanceData.bStartedAttack = false;
}
