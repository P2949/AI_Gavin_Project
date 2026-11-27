#include "Enemies/HyenaSlime/Hyena_CountAllies.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

UHyena_CountAllies::UHyena_CountAllies()
{
	NodeName = "Count Allies";
	Interval = 0.3f;
	RandomDeviation = 0.1f;
	// Restricts the dropdown in the editor to Int blackboard keys
	AllyCountKey.AddIntFilter(this, GET_MEMBER_NAME_CHECKED(UHyena_CountAllies, AllyCountKey));
}

void UHyena_CountAllies::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIC = OwnerComp.GetAIOwner();
	APawn* Pawn = AIC ? AIC->GetPawn() : nullptr;
	if (!Pawn) return;

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CountAllies), false, Pawn); // ignore self

	Pawn->GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		Pawn->GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Radius),
		Params);

	int32 Count = 0;
	for (const FOverlapResult& R : Overlaps)
	{
		const APawn* Other = Cast<APawn>(R.GetActor());
		if (Other && Other->GetClass() == Pawn->GetClass())
		{
			++Count;
		}
	}

	OwnerComp.GetBlackboardComponent()->SetValueAsInt(AllyCountKey.SelectedKeyName, Count);
}
