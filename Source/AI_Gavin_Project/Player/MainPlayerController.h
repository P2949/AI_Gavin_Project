#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Player/Input/InputMappingContextConfig.h"
#include "MainPlayerController.generated.h"

class UPlayerHUDWidget;

UCLASS()
class AI_GAVIN_PROJECT_API AMainPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn *InPawn) override;
	virtual void OnUnPossess() override;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Input")
	TArray<FInputMappingContextConfig> DefaultMappingContexts;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "UI")
	TSubclassOf<UPlayerHUDWidget> PlayerHUDWidgetClass;

private:
	void InitializeInputMappingContexts();
	void InitializePlayerHUD();
	void RefreshPlayerHUDDataSource();

	UPROPERTY(Transient)
	TObjectPtr<UPlayerHUDWidget> PlayerHUDWidget;
};
