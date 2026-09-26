#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHUDWidget.generated.h"

class UHealthComponent;
class UProgressBar;

UCLASS(Abstract)
class AI_GAVIN_PROJECT_API UPlayerHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UI|HUD")
    void SetObservedHealthComponent(
        UHealthComponent *NewHealthComponent);

protected:
    virtual void NativeDestruct() override;

    UPROPERTY(
        meta = (BindWidget),
        BlueprintReadOnly,
        Category = "UI|HUD")
    TObjectPtr<UProgressBar> HealthProgressBar;

private:
    UFUNCTION()
    void HandleHealthChanged(
        float OldHealth,
        float NewHealth,
        float Delta);

    void RefreshHealthDisplay();

    TWeakObjectPtr<UHealthComponent> ObservedHealthComponent;
};
