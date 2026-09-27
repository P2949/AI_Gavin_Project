#include "AI/Controllers/EnemyAIController.h"

#include "Components/StateTreeAIComponent.h"
#include "GameFramework/Pawn.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"

AEnemyAIController::AEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	StateTreeAIComponent =
		CreateDefaultSubobject<UStateTreeAIComponent>(
			TEXT("StateTreeAIComponent"));

	check(StateTreeAIComponent);

	// The controller owns when its StateTree begins running.
	bStartAILogicOnPossess = false;
	StateTreeAIComponent->SetStartLogicAutomatically(false);

	// Keep the controller spatially associated with its possessed pawn.
	bAttachToPawn = true;

	AIPerceptionComponent =
		CreateDefaultSubobject<UAIPerceptionComponent>(
			TEXT("AIPerceptionComponent"));

	check(AIPerceptionComponent);

	SetPerceptionComponent(*AIPerceptionComponent);

	SightConfig =
		CreateDefaultSubobject<UAISenseConfig_Sight>(
			TEXT("SightConfig"));

	check(SightConfig);

	// Baseline sight values. Concrete enemy controllers can tune these
	// without changing the shared perception architecture.
	SightConfig->SightRadius = 1500.0f;
	SightConfig->LoseSightRadius = 1800.0f;
	SightConfig->PeripheralVisionAngleDegrees = 90.0f;
	SightConfig->SetMaxAge(5.0f);

	// Perception gathers sensory information broadly. Target-selection
	// policy decides which perceived actors are actually relevant.
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

	AIPerceptionComponent->ConfigureSense(*SightConfig);
	AIPerceptionComponent->SetDominantSense(
		SightConfig->GetSenseImplementation());

	AIPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
		this,
		&AEnemyAIController::HandleTargetPerceptionUpdated);
}

void AEnemyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	RefreshLastKnownTargetLocationFromSight();
}

void AEnemyAIController::OnPossess(APawn *InPawn)
{
	Super::OnPossess(InPawn);

	// A controller may be reused for another pawn. Never allow
	// target knowledge from an earlier possession to leak across.
	if (AIPerceptionComponent)
	{
		AIPerceptionComponent->ForgetAll();
	}

	ResetTargetState();

	if (StateTreeAIComponent)
	{
		StateTreeAIComponent->StartLogic();
	}
}

void AEnemyAIController::OnUnPossess()
{
	if (StateTreeAIComponent)
	{
		StateTreeAIComponent->StopLogic(
			TEXT("Enemy AI controller unpossessed pawn."));
	}

	if (AIPerceptionComponent)
	{
		AIPerceptionComponent->ForgetAll();
	}

	ResetTargetState();

	Super::OnUnPossess();
}

bool AEnemyAIController::IsValidTargetActor(
	const AActor *Actor) const
{
	const APawn *Pawn = Cast<APawn>(Actor);

	return IsValid(Pawn) &&
		   Pawn->IsPlayerControlled();
}

bool AEnemyAIController::ShouldReplaceCurrentTarget(
	const AActor *CandidateActor) const
{
	static_cast<void>(CandidateActor);

	return !CurrentTargetActor.IsValid();
}

void AEnemyAIController::HandleTargetPerceptionUpdated(
	AActor *Actor,
	FAIStimulus Stimulus)
{
	if (!GetPawn() ||
		!IsValidTargetActor(Actor))
	{
		return;
	}

	const bool bIsCurrentTarget =
		CurrentTargetActor.Get() == Actor;

	if (Stimulus.WasSuccessfullySensed())
	{
		if (!bIsCurrentTarget)
		{
			if (CurrentTargetActor.IsValid() &&
				!ShouldReplaceCurrentTarget(Actor))
			{
				return;
			}

			CurrentTargetActor = Actor;
		}

		bIsCurrentTargetPerceived = true;

		LastKnownTargetLocation =
			Stimulus.StimulusLocation;

		bHasLastKnownTargetLocation = true;

		return;
	}

	if (bIsCurrentTarget)
	{
		UpdateCurrentTargetPerceptionState();
	}
}

void AEnemyAIController::UpdateCurrentTargetPerceptionState()
{
	AActor *TargetActor = CurrentTargetActor.Get();

	if (!TargetActor || !AIPerceptionComponent)
	{
		bIsCurrentTargetPerceived = false;
		return;
	}

	bIsCurrentTargetPerceived =
		AIPerceptionComponent->HasAnyCurrentStimulus(
			*TargetActor);
}

void AEnemyAIController::RefreshLastKnownTargetLocationFromSight()
{
	AActor *TargetActor = CurrentTargetActor.Get();

	if (!TargetActor ||
		!AIPerceptionComponent ||
		!SightConfig)
	{
		return;
	}

	if (!AIPerceptionComponent->HasActiveStimulus(
			*TargetActor,
			SightConfig->GetSenseID()))
	{
		return;
	}

	LastKnownTargetLocation =
		TargetActor->GetActorLocation();

	bHasLastKnownTargetLocation = true;
}

void AEnemyAIController::ResetTargetState()
{
	CurrentTargetActor.Reset();

	LastKnownTargetLocation = FVector::ZeroVector;

	bHasLastKnownTargetLocation = false;
	bIsCurrentTargetPerceived = false;
}
