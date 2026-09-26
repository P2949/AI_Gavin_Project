#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Player/Input/InputMappingContextConfig.h"
#include "MainPlayerController.generated.h"

UCLASS()
class AI_GAVIN_PROJECT_API AMainPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Input")
	TArray<FInputMappingContextConfig> DefaultMappingContexts;
};
