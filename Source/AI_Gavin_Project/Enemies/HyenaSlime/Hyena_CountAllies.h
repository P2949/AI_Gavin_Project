#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "Hyena_CountAllies.generated.h"



/**
 * 
 */
UCLASS()
class AI_GAVIN_PROJECT_API UHyena_CountAllies : public UBTService{
	
	GENERATED_BODY()
	
public:
	UHyena_CountAllies();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Allies")
	float Radius = 1500.f;

	UPROPERTY(EditAnywhere, Category = "Allies")
	FBlackboardKeySelector AllyCountKey;

};
